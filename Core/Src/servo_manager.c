#include "servo_manager.h"

static uint16_t map_angle_to_pulse(const ServoChannel *channel, uint8_t angle_deg)
{
  if (angle_deg > 180U) {
    angle_deg = 180U;
  }

  const uint32_t span = (uint32_t)(channel->max_pulse_us - channel->min_pulse_us);
  return (uint16_t)(channel->min_pulse_us + (span * angle_deg) / 180U);
}

void ServoManager_Init(ServoManager *mgr, uint32_t frame_us)
{
  mgr->count = 0U;
  mgr->frame_us = frame_us;
  mgr->frame_start = HAL_GetTick();
  for (uint8_t i = 0U; i < SERVO_MANAGER_MAX_SERVOS; i++) {
    mgr->channels[i].active = false;
  }
}

bool ServoManager_Add(ServoManager *mgr, GPIO_TypeDef *port, uint16_t pin, uint16_t min_pulse_us, uint16_t max_pulse_us)
{
  if (mgr->count >= SERVO_MANAGER_MAX_SERVOS || min_pulse_us >= max_pulse_us) {
    return false;
  }

  ServoChannel *ch = &mgr->channels[mgr->count];
  ch->port = port;
  ch->pin = pin;
  ch->min_pulse_us = min_pulse_us;
  ch->max_pulse_us = max_pulse_us;
  ch->pulse_us = (uint16_t)((min_pulse_us + max_pulse_us) / 2U);
  ch->active = true;
  HAL_GPIO_WritePin(ch->port, ch->pin, GPIO_PIN_RESET);

  mgr->count++;
  return true;
}

bool ServoManager_SetAngle(ServoManager *mgr, uint8_t idx, uint8_t angle_deg)
{
  if (idx >= mgr->count || !mgr->channels[idx].active) {
    return false;
  }

  mgr->channels[idx].pulse_us = map_angle_to_pulse(&mgr->channels[idx], angle_deg);
  return true;
}

void ServoManager_Task(ServoManager *mgr)
{
  uint32_t now = HAL_GetTick();
  if ((now - mgr->frame_start) >= (mgr->frame_us / 1000U)) {
    mgr->frame_start = now;

    for (uint8_t i = 0U; i < mgr->count; i++) {
      ServoChannel *ch = &mgr->channels[i];
      if (!ch->active) {
        continue;
      }
      HAL_GPIO_WritePin(ch->port, ch->pin, GPIO_PIN_SET);
      for (volatile uint32_t i = 0; i < (uint32_t)(ch->pulse_us * 8U); i++) { __NOP(); }
      HAL_GPIO_WritePin(ch->port, ch->pin, GPIO_PIN_RESET);
    }
  }
}
