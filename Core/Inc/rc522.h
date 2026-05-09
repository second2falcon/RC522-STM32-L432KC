#ifndef RC522_H
#define RC522_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define RC522_MAX_UID_LEN 10U

typedef struct {
  uint8_t bytes[RC522_MAX_UID_LEN];
  uint8_t size;
} RC522_Uid;

typedef struct {
  GPIO_TypeDef *sck_port;
  uint16_t sck_pin;
  GPIO_TypeDef *miso_port;
  uint16_t miso_pin;
  GPIO_TypeDef *mosi_port;
  uint16_t mosi_pin;
  GPIO_TypeDef *nss_port;
  uint16_t nss_pin;
  GPIO_TypeDef *rst_port;
  uint16_t rst_pin;
} RC522_Handle;

void RC522_Init(RC522_Handle *h);
bool RC522_ReadUid(RC522_Handle *h, RC522_Uid *uid);

#endif
