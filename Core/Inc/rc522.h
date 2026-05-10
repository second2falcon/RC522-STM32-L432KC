/**
 * RC522 RFID reader driver for STM32 HAL (SPI).
 *
 * Wiring (STM32L432KC Nucleo):
 *   PA5  → RC522 SCK   (SPI1_SCK, AF5)
 *   PA6  → RC522 MISO  (SPI1_MISO, AF5)
 *   PA7  → RC522 MOSI  (SPI1_MOSI, AF5)
 *   PA4  → RC522 SDA   (CS, GPIO output, active LOW)
 *   PB0  → RC522 RST   (GPIO output, HIGH = normal, LOW = reset)
 */

#ifndef RC522_H
#define RC522_H

#include "main.h"

extern SPI_HandleTypeDef hspi1;

/* ----------------------------------------------------------------
 * Status codes
 * ---------------------------------------------------------------- */
#define MI_OK        0
#define MI_NOTAGERR  1
#define MI_ERR       2

/* ----------------------------------------------------------------
 * RC522 Register Map (6-bit register address, bits[6:1] of wire byte)
 * ---------------------------------------------------------------- */

/* Page 0 — Command and status */
#define CommandReg        0x01
#define ComIEnReg         0x02
#define DivIEnReg         0x03
#define ComIrqReg         0x04
#define DivIrqReg         0x05
#define ErrorReg          0x06
#define Status1Reg        0x07
#define Status2Reg        0x08
#define FIFODataReg       0x09
#define FIFOLevelReg      0x0A
#define WaterLevelReg     0x0B
#define ControlReg        0x0C
#define BitFramingReg     0x0D
#define CollReg           0x0E

/* Page 1 — Communication */
#define ModeReg           0x11
#define TxModeReg         0x12
#define RxModeReg         0x13
#define TxControlReg      0x14
#define TxASKReg          0x15
#define TxSelReg          0x16
#define RxSelReg          0x17
#define RxThresholdReg    0x18
#define DemodReg          0x19
#define MifareReg         0x1C
#define ManualRCVReg      0x1D
#define SerialSpeedReg    0x1F

/* Page 2 — Configuration */
#define CRCResultRegH     0x21
#define CRCResultRegL     0x22
#define ModWidthReg       0x24
#define RFCfgReg          0x26
#define GsNReg            0x27
#define CWGsPReg          0x28
#define ModGsPReg         0x29
#define TModeReg          0x2A
#define TPrescalerReg     0x2B
#define TReloadRegH       0x2C
#define TReloadRegL       0x2D
#define TCounterValueRegH 0x2E
#define TCounterValueRegL 0x2F

/* Page 3 — Test */
#define TestSel1Reg       0x31
#define TestSel2Reg       0x32
#define TestPinEnReg      0x33
#define TestPinValueReg   0x34
#define TestBusReg        0x35
#define AutoTestReg       0x36
#define VersionReg        0x37
#define AnalogTestReg     0x38
#define TestDAC1Reg       0x39
#define TestDAC2Reg       0x3A
#define TestADCReg        0x3B

/* ----------------------------------------------------------------
 * PCD (reader) command codes  →  written to CommandReg
 * ---------------------------------------------------------------- */
#define PCD_IDLE          0x00
#define PCD_AUTHENT       0x0E
#define PCD_RECEIVE       0x08
#define PCD_TRANSMIT      0x04
#define PCD_TRANSCEIVE    0x0C
#define PCD_RESETPHASE    0x0F
#define PCD_CALCCRC       0x03

/* ----------------------------------------------------------------
 * PICC (card) command codes
 * ---------------------------------------------------------------- */
#define PICC_REQIDL       0x26  /* REQA — invite IDLE cards */
#define PICC_REQALL       0x52  /* WUPA — invite ALL cards incl. halted */
#define PICC_ANTICOLL     0x93  /* Anti-collision CL1 */
#define PICC_SELECTTAG    0x93  /* SELECT CL1 */
#define PICC_AUTHENT1A    0x60
#define PICC_AUTHENT1B    0x61
#define PICC_READ         0x30
#define PICC_WRITE        0xA0
#define PICC_DECREMENT    0xC0
#define PICC_INCREMENT    0xC1
#define PICC_RESTORE      0xC2
#define PICC_TRANSFER     0xB0
#define PICC_HALT         0x50

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

/* Low-level register access */
void    RC522_WriteRegister(uint8_t reg, uint8_t value);
uint8_t RC522_ReadRegister(uint8_t reg);
void    RC522_SetBitMask(uint8_t reg, uint8_t mask);
void    RC522_ClearBitMask(uint8_t reg, uint8_t mask);

/* Antenna */
void RC522_AntennaOn(void);
void RC522_AntennaOff(void);

/* Initialisation */
void RC522_Reset(void);
void RC522_Init(void);

/* Hardware CRC (uses RC522 CRC coprocessor) */
void RC522_CalcCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData);

/* Core transceive */
uint8_t RC522_ToCard(uint8_t command,
                     uint8_t *sendData, uint8_t sendLen,
                     uint8_t *backData, uint16_t *backLen);

/* ISO 14443A protocol — single cascade (4-byte UID) */
uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t RC522_Anticoll(uint8_t *serNum);
uint8_t RC522_SelectCard(uint8_t *serNum);

/* High-level: full Request → Anticoll → Select sequence */
uint8_t RC522_ReadCardUID(uint8_t *uid, uint8_t *uid_len);

#endif /* RC522_H */
