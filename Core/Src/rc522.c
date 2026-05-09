#include "rc522.h"

#define RC522_REG_COMMAND 0x01U
#define RC522_REG_COM_IRQ 0x04U
#define RC522_REG_ERROR 0x06U
#define RC522_REG_FIFO_DATA 0x09U
#define RC522_REG_FIFO_LEVEL 0x0AU
#define RC522_REG_CONTROL 0x0CU
#define RC522_REG_BIT_FRAMING 0x0DU
#define RC522_REG_MODE 0x11U
#define RC522_REG_VERSION 0x37U
#define RC522_REG_TX_CONTROL 0x14U
#define RC522_REG_TX_AUTO 0x15U
#define RC522_REG_CRC_RESULT_L 0x22U
#define RC522_REG_CRC_RESULT_H 0x21U
#define RC522_REG_T_MODE 0x2AU
#define RC522_REG_T_PRESCALER 0x2BU
#define RC522_REG_T_RELOAD_H 0x2CU
#define RC522_REG_T_RELOAD_L 0x2DU

#define RC522_CMD_IDLE 0x00U
#define RC522_CMD_TRANSCEIVE 0x0CU
#define RC522_CMD_SOFT_RESET 0x0FU

#define PICC_CMD_REQA 0x26U
#define PICC_CMD_SELECT_CL1 0x93U
#define PICC_CMD_ANTICOLLISION 0x20U

static inline void rc522_delay(void) { for (volatile uint32_t i = 0; i < 24U; i++) { __NOP(); } }

static void spi_write_bitbang(RC522_Handle *h, uint8_t data)
{
  for (uint8_t i = 0U; i < 8U; i++) {
    HAL_GPIO_WritePin(h->sck_port, h->sck_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(h->mosi_port, h->mosi_pin, (data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    rc522_delay();
    HAL_GPIO_WritePin(h->sck_port, h->sck_pin, GPIO_PIN_SET);
    rc522_delay();
    data <<= 1U;
  }
}

static uint8_t spi_read_bitbang(RC522_Handle *h)
{
  uint8_t data = 0U;
  for (uint8_t i = 0U; i < 8U; i++) {
    data <<= 1U;
    HAL_GPIO_WritePin(h->sck_port, h->sck_pin, GPIO_PIN_RESET);
    rc522_delay();
    HAL_GPIO_WritePin(h->sck_port, h->sck_pin, GPIO_PIN_SET);
    if (HAL_GPIO_ReadPin(h->miso_port, h->miso_pin) == GPIO_PIN_SET) {
      data |= 0x01U;
    }
    rc522_delay();
  }
  return data;
}

static void rc522_write_reg(RC522_Handle *h, uint8_t reg, uint8_t value)
{
  HAL_GPIO_WritePin(h->nss_port, h->nss_pin, GPIO_PIN_RESET);
  spi_write_bitbang(h, (uint8_t)((reg << 1U) & 0x7EU));
  spi_write_bitbang(h, value);
  HAL_GPIO_WritePin(h->nss_port, h->nss_pin, GPIO_PIN_SET);
}

static uint8_t rc522_read_reg(RC522_Handle *h, uint8_t reg)
{
  HAL_GPIO_WritePin(h->nss_port, h->nss_pin, GPIO_PIN_RESET);
  spi_write_bitbang(h, (uint8_t)(((reg << 1U) & 0x7EU) | 0x80U));
  uint8_t v = spi_read_bitbang(h);
  HAL_GPIO_WritePin(h->nss_port, h->nss_pin, GPIO_PIN_SET);
  return v;
}

static bool rc522_transceive(RC522_Handle *h, const uint8_t *tx, uint8_t tx_len, uint8_t *rx, uint8_t *rx_len, uint8_t valid_bits)
{
  rc522_write_reg(h, RC522_REG_COMMAND, RC522_CMD_IDLE);
  rc522_write_reg(h, RC522_REG_COM_IRQ, 0x7FU);
  rc522_write_reg(h, RC522_REG_FIFO_LEVEL, 0x80U);

  for (uint8_t i = 0; i < tx_len; i++) {
    rc522_write_reg(h, RC522_REG_FIFO_DATA, tx[i]);
  }

  rc522_write_reg(h, RC522_REG_BIT_FRAMING, valid_bits);
  rc522_write_reg(h, RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
  rc522_write_reg(h, RC522_REG_BIT_FRAMING, (uint8_t)(valid_bits | 0x80U));

  uint32_t t0 = HAL_GetTick();
  while (!(rc522_read_reg(h, RC522_REG_COM_IRQ) & 0x30U)) {
    if ((HAL_GetTick() - t0) > 25U) {
      return false;
    }
  }

  if (rc522_read_reg(h, RC522_REG_ERROR) & 0x13U) {
    return false;
  }

  uint8_t fifo_len = rc522_read_reg(h, RC522_REG_FIFO_LEVEL);
  if (fifo_len == 0U || fifo_len > *rx_len) {
    return false;
  }

  for (uint8_t i = 0U; i < fifo_len; i++) {
    rx[i] = rc522_read_reg(h, RC522_REG_FIFO_DATA);
  }
  *rx_len = fifo_len;
  return true;
}

void RC522_Init(RC522_Handle *h)
{
  HAL_GPIO_WritePin(h->rst_port, h->rst_pin, GPIO_PIN_SET);
  HAL_Delay(10);
  rc522_write_reg(h, RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
  HAL_Delay(50);

  rc522_write_reg(h, RC522_REG_T_MODE, 0x8DU);
  rc522_write_reg(h, RC522_REG_T_PRESCALER, 0x3EU);
  rc522_write_reg(h, RC522_REG_T_RELOAD_L, 30U);
  rc522_write_reg(h, RC522_REG_T_RELOAD_H, 0U);
  rc522_write_reg(h, RC522_REG_TX_AUTO, 0x40U);
  rc522_write_reg(h, RC522_REG_MODE, 0x3DU);

  uint8_t tx = rc522_read_reg(h, RC522_REG_TX_CONTROL);
  if ((tx & 0x03U) != 0x03U) {
    rc522_write_reg(h, RC522_REG_TX_CONTROL, (uint8_t)(tx | 0x03U));
  }
}


bool RC522_CheckComm(RC522_Handle *h, uint8_t *version_reg)
{
  uint8_t version = rc522_read_reg(h, RC522_REG_VERSION);
  if (version_reg != NULL) {
    *version_reg = version;
  }

  return !((version == 0x00U) || (version == 0xFFU));
}

bool RC522_ReadUid(RC522_Handle *h, RC522_Uid *uid)
{
  uint8_t reqa = PICC_CMD_REQA;
  uint8_t atqa[2];
  uint8_t atqa_len = sizeof(atqa);
  if (!rc522_transceive(h, &reqa, 1U, atqa, &atqa_len, 0x07U)) {
    return false;
  }

  uint8_t anti_col[2] = {PICC_CMD_SELECT_CL1, PICC_CMD_ANTICOLLISION};
  uint8_t rx[5];
  uint8_t rx_len = sizeof(rx);
  if (!rc522_transceive(h, anti_col, 2U, rx, &rx_len, 0x00U) || rx_len != 5U) {
    return false;
  }

  uint8_t bcc = (uint8_t)(rx[0] ^ rx[1] ^ rx[2] ^ rx[3]);
  if (bcc != rx[4]) {
    return false;
  }

  uid->size = 4U;
  for (uint8_t i = 0; i < 4U; i++) {
    uid->bytes[i] = rx[i];
  }
  return true;
}
