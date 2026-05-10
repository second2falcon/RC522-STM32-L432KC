#include "rc522.h"
#include <string.h>

/* ----------------------------------------------------------------
 * Internal: chip-select helpers
 * ---------------------------------------------------------------- */
static inline void CS_Low(void)
{
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);
}

static inline void CS_High(void)
{
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET);
}

/* ----------------------------------------------------------------
 * RC522_WriteRegister
 *
 * SPI address byte for write: (reg << 1) & 0x7E
 *   bit7 = 0  (write)
 *   bits[6:1] = register address
 *   bit0  = 0  (always)
 * ---------------------------------------------------------------- */
void RC522_WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t buf[2];
    buf[0] = (reg << 1) & 0x7E;
    buf[1] = value;

    CS_Low();
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    CS_High();
}

/* ----------------------------------------------------------------
 * RC522_ReadRegister
 *
 * SPI address byte for read: ((reg << 1) & 0x7E) | 0x80
 *   bit7 = 1  (read)
 *   bits[6:1] = register address
 *   bit0  = 0
 * ---------------------------------------------------------------- */
uint8_t RC522_ReadRegister(uint8_t reg)
{
    uint8_t addr  = ((reg << 1) & 0x7E) | 0x80;
    uint8_t value = 0;

    CS_Low();
    HAL_SPI_Transmit(&hspi1, &addr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &value, 1, HAL_MAX_DELAY);
    CS_High();

    return value;
}

void RC522_SetBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteRegister(reg, RC522_ReadRegister(reg) | mask);
}

void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteRegister(reg, RC522_ReadRegister(reg) & ~mask);
}

/* ----------------------------------------------------------------
 * RC522_AntennaOn / Off
 *
 * TxControlReg bits [1:0] enable the TX1 and TX2 antenna drivers.
 * Both must be set for normal ISO 14443A operation.
 * ---------------------------------------------------------------- */
void RC522_AntennaOn(void)
{
    uint8_t val = RC522_ReadRegister(TxControlReg);
    if ((val & 0x03) != 0x03)
    {
        RC522_SetBitMask(TxControlReg, 0x03);
    }
}

void RC522_AntennaOff(void)
{
    RC522_ClearBitMask(TxControlReg, 0x03);
}

/* ----------------------------------------------------------------
 * RC522_Reset
 *
 * Soft-reset: all registers return to power-on defaults.
 * The reset command executes automatically; 50 ms covers the worst-
 * case oscillator start-up time specified in the MFRC522 datasheet.
 * ---------------------------------------------------------------- */
void RC522_Reset(void)
{
    RC522_WriteRegister(CommandReg, PCD_RESETPHASE);
    HAL_Delay(50);
}

/* ----------------------------------------------------------------
 * RC522_Init
 *
 * 1. Pulse RST pin LOW → HIGH (hardware reset)
 * 2. Soft reset
 * 3. Configure internal timer (TAuto, ~500 ms timeout)
 * 4. 100 % ASK modulation (mandatory for ISO 14443A)
 * 5. CRC preset 0x6363  (ISO 14443A requirement)
 * 6. Enable antenna
 * ---------------------------------------------------------------- */
void RC522_Init(void)
{
    /* Hardware reset */
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RC522_RST_GPIO_Port, RC522_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);

    RC522_Reset();

    /*
     * Timer: TAuto = 1 — timer starts automatically at end of Tx.
     * 12-bit prescaler = 0xD3E (3390).
     * Timer freq = 13.56 MHz / (2 * 3391) ≈ 2 kHz.
     * Reload 1000 → timeout ≈ 500 ms (plenty of margin).
     */
    RC522_WriteRegister(TModeReg,      0x8D);
    RC522_WriteRegister(TPrescalerReg, 0x3E);
    RC522_WriteRegister(TReloadRegH,   0x03);
    RC522_WriteRegister(TReloadRegL,   0xE8);

    /* Force100ASK — required by ISO 14443A */
    RC522_WriteRegister(TxASKReg, 0x40);

    /* CRC preset value 0x6363 as required by ISO 14443A */
    RC522_WriteRegister(ModeReg, 0x3D);

    RC522_AntennaOn();
}

/* ----------------------------------------------------------------
 * RC522_CalcCRC
 *
 * Uses the RC522's on-chip CRC coprocessor.
 * Result: pOutData[0] = CRCResultRegL, pOutData[1] = CRCResultRegH
 * ---------------------------------------------------------------- */
void RC522_CalcCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData)
{
    uint8_t i, n;

    RC522_ClearBitMask(DivIrqReg, 0x04);   /* clear CRCIRq */
    RC522_SetBitMask(FIFOLevelReg, 0x80);  /* flush FIFO */
    RC522_WriteRegister(CommandReg, PCD_IDLE);

    for (i = 0; i < len; i++)
    {
        RC522_WriteRegister(FIFODataReg, pIndata[i]);
    }

    RC522_WriteRegister(CommandReg, PCD_CALCCRC);

    /* Poll CRCIRq bit with a finite timeout */
    i = 0xFF;
    do {
        n = RC522_ReadRegister(DivIrqReg);
        i--;
    } while (i && !(n & 0x04));

    pOutData[0] = RC522_ReadRegister(CRCResultRegL);
    pOutData[1] = RC522_ReadRegister(CRCResultRegH);
}

/* ----------------------------------------------------------------
 * RC522_ToCard
 *
 * Low-level transceive: writes sendData to FIFO, issues command,
 * polls for completion, reads received bytes from FIFO.
 *
 * Returns MI_OK, MI_NOTAGERR, or MI_ERR.
 * *backLen is set to the number of received *bits*.
 * ---------------------------------------------------------------- */
uint8_t RC522_ToCard(uint8_t command,
                     uint8_t *sendData, uint8_t sendLen,
                     uint8_t *backData, uint16_t *backLen)
{
    uint8_t  status   = MI_ERR;
    uint8_t  irqEn    = 0x00;
    uint8_t  waitIRq  = 0x00;
    uint8_t  lastBits;
    uint8_t  n;
    uint16_t i;

    switch (command)
    {
        case PCD_AUTHENT:
            irqEn   = 0x12;  /* IdleIEn | ErrIEn */
            waitIRq = 0x10;  /* IdleIRq */
            break;
        case PCD_TRANSCEIVE:
            irqEn   = 0x77;  /* Tx|Rx|Idle|LoAlert|Err|Timer IRQ enables */
            waitIRq = 0x30;  /* RxIRq | IdleIRq */
            break;
        default:
            break;
    }

    RC522_WriteRegister(ComIEnReg,   irqEn | 0x80);  /* set IRQ push-pull */
    RC522_ClearBitMask(ComIrqReg,    0x80);           /* clear all IRQ flags */
    RC522_SetBitMask(FIFOLevelReg,   0x80);           /* flush FIFO */
    RC522_WriteRegister(CommandReg,  PCD_IDLE);

    for (i = 0; i < sendLen; i++)
    {
        RC522_WriteRegister(FIFODataReg, sendData[i]);
    }

    RC522_WriteRegister(CommandReg, command);

    if (command == PCD_TRANSCEIVE)
    {
        RC522_SetBitMask(BitFramingReg, 0x80);  /* StartSend = 1 */
    }

    /* Poll for RxIRq, IdleIRq, or TimerIRq (bit 0) */
    i = 2000;
    do {
        n = RC522_ReadRegister(ComIrqReg);
        i--;
    } while (i && !(n & 0x01) && !(n & waitIRq));

    RC522_ClearBitMask(BitFramingReg, 0x80);  /* StartSend = 0 */

    if (i == 0)
    {
        return MI_ERR;  /* loop timeout */
    }

    /* Check ErrorReg: BufferOvfl|ColErr|CRCErr|ProtocolErr */
    if (!(RC522_ReadRegister(ErrorReg) & 0x1B))
    {
        status = MI_OK;

        if (n & irqEn & 0x01)
        {
            status = MI_NOTAGERR;  /* only timer fired */
        }

        if (command == PCD_TRANSCEIVE)
        {
            n        = RC522_ReadRegister(FIFOLevelReg);
            lastBits = RC522_ReadRegister(ControlReg) & 0x07;

            *backLen = lastBits ? (uint16_t)(n - 1) * 8 + lastBits
                                : (uint16_t)n * 8;

            if (n == 0) n = 1;
            if (n > 16) n = 16;

            for (i = 0; i < n; i++)
            {
                backData[i] = RC522_ReadRegister(FIFODataReg);
            }
        }
    }
    else
    {
        status = MI_ERR;
    }

    return status;
}

/* ----------------------------------------------------------------
 * RC522_Request
 *
 * Sends REQA (7-bit short frame) and returns the 2-byte ATQA.
 * Returns MI_OK when a card responds.
 * ---------------------------------------------------------------- */
uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType)
{
    uint8_t  status;
    uint16_t backBits;

    RC522_WriteRegister(BitFramingReg, 0x07);  /* TxLastBits = 7 */

    TagType[0] = reqMode;
    status = RC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

    if ((status != MI_OK) || (backBits != 0x10))
    {
        status = MI_ERR;
    }

    return status;
}

/* ----------------------------------------------------------------
 * RC522_Anticoll
 *
 * Single-cascade anti-collision for 4-byte UIDs (CL1).
 * Sends [0x93, 0x20], receives UID[4] + BCC.
 * Validates the BCC (XOR of all four UID bytes).
 * ---------------------------------------------------------------- */
uint8_t RC522_Anticoll(uint8_t *serNum)
{
    uint8_t  status;
    uint8_t  i;
    uint8_t  bcc    = 0;
    uint16_t unLen;
    uint8_t  cmd[2]    = {PICC_ANTICOLL, 0x20};
    uint8_t  backData[16];

    RC522_WriteRegister(BitFramingReg, 0x00);

    status = RC522_ToCard(PCD_TRANSCEIVE, cmd, 2, backData, &unLen);

    if (status == MI_OK)
    {
        for (i = 0; i < 4; i++)
        {
            serNum[i] = backData[i];
            bcc      ^= backData[i];
        }

        if (bcc != backData[4])
        {
            status = MI_ERR;  /* BCC mismatch */
        }
    }

    return status;
}

/* ----------------------------------------------------------------
 * RC522_SelectCard
 *
 * Sends SELECT CL1 with the 4-byte UID from anticollision.
 * Frame: [0x93, 0x70, UID[4], BCC, CRC[2]]
 * Returns MI_OK on a valid SAK response.
 * ---------------------------------------------------------------- */
uint8_t RC522_SelectCard(uint8_t *serNum)
{
    uint8_t  status;
    uint8_t  i;
    uint8_t  buf[9];
    uint16_t recvBits;
    uint8_t  backData[16];

    buf[0] = PICC_SELECTTAG;
    buf[1] = 0x70;  /* NVB = 7 full bytes to follow */
    for (i = 0; i < 4; i++)
    {
        buf[2 + i] = serNum[i];
    }
    buf[6] = serNum[0] ^ serNum[1] ^ serNum[2] ^ serNum[3];  /* BCC */
    RC522_CalcCRC(buf, 7, &buf[7]);

    status = RC522_ToCard(PCD_TRANSCEIVE, buf, 9, backData, &recvBits);

    /* SAK response is 24 bits (1 data byte + 2 CRC bytes) */
    if ((status != MI_OK) || (recvBits != 0x18))
    {
        status = MI_ERR;
    }

    return status;
}

/* ----------------------------------------------------------------
 * RC522_ReadCardUID
 *
 * High-level: Request → Anticoll → Select.
 * Fills uid[0..3] with the 4-byte UID and sets *uid_len = 4.
 * Returns MI_OK, MI_NOTAGERR (no card), or MI_ERR.
 * ---------------------------------------------------------------- */
uint8_t RC522_ReadCardUID(uint8_t *uid, uint8_t *uid_len)
{
    uint8_t status;
    uint8_t tagType[2];

    status = RC522_Request(PICC_REQIDL, tagType);
    if (status != MI_OK)
    {
        return status;
    }

    status = RC522_Anticoll(uid);
    if (status != MI_OK)
    {
        return status;
    }

    status = RC522_SelectCard(uid);
    if (status != MI_OK)
    {
        return MI_ERR;
    }

    *uid_len = 4;
    return MI_OK;
}
