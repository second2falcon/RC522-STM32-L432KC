#ifndef SERVO_MANAGER_H
#define SERVO_MANAGER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define SERVO_MANAGER_MAX_SERVOS 8U

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  uint16_t min_pulse_us;
  uint16_t max_pulse_us;
  uint16_t pulse_us;
  bool active;
} ServoChannel;

typedef struct {
  ServoChannel channels[SERVO_MANAGER_MAX_SERVOS];
  uint8_t count;
  uint32_t frame_us;
  uint32_t frame_start;
} ServoManager;

void ServoManager_Init(ServoManager *mgr, uint32_t frame_us);
bool ServoManager_Add(ServoManager *mgr, GPIO_TypeDef *port, uint16_t pin, uint16_t min_pulse_us, uint16_t max_pulse_us);
bool ServoManager_SetAngle(ServoManager *mgr, uint8_t idx, uint8_t angle_deg);
void ServoManager_Task(ServoManager *mgr);

#endif
