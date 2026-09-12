/*
 * mcp2515.c
 *
 *  Created on: Jul 26, 2026
 *      Author: alper
 */

#include "mcp2515.h"
#include "main.h"
extern SPI_HandleTypeDef hspi1;

/* A mode change runs asynchronously inside the MCP2515. Entering NORMAL
   mode in particular does not complete until the chip has seen 11
   consecutive recessive bits on the bus. At 500 kbit that is ~22 us on an
   idle bus, but it stretches while the bus is busy. */
#define MCP2515_MODE_TIMEOUT_MS 20u

/* Generous: the longest transaction here is 14 bytes, about 28 us at 4 MHz.
   Reaching this timeout means the SPI peripheral is wedged, not that the
   transfer was slow. */
#define MCP2515_SPI_TIMEOUT_MS  10u

/* Longest transaction: READ RX BUFFER - one command byte plus SIDH, SIDL,
   EID8, EID0, DLC and eight data bytes. */
#define MCP2515_MAX_TRANSFER    14u

static const MCP2515_BitTiming_t bitTiming8MHz[] = {
    [MCP2515_BITRATE_125KBPS] = { .cnf1 = 0x01, .cnf2 = 0xB1, .cnf3 = 0x05 },
    [MCP2515_BITRATE_250KBPS] = { .cnf1 = 0x00, .cnf2 = 0xB1, .cnf3 = 0x05 },
    /* With an 8 MHz crystal, 8 tq is the only way to reach 500 kbit:
       TQ = 2*(BRP+1)/Fosc = 250 ns, 8 x 250 ns = 2 us = 500 kbit/s
       CNF2 = 0x90 -> BTLMODE=1, PRSEG=1 tq, PHSEG1=3 tq
       CNF3 = 0x02 -> PHSEG2=3 tq   => sample point (1+1+3)/8 = 62.5% */
    [MCP2515_BITRATE_500KBPS] = { .cnf1 = 0x00, .cnf2 = 0x90, .cnf3 = 0x02 },
};

static const MCP2515_BitTiming_t bitTiming16MHz[] = {
    [MCP2515_BITRATE_125KBPS] = { .cnf1 = 0x03, .cnf2 = 0xB1, .cnf3 = 0x05 },
    [MCP2515_BITRATE_250KBPS] = { .cnf1 = 0x01, .cnf2 = 0xB1, .cnf3 = 0x05 },
    [MCP2515_BITRATE_500KBPS] = { .cnf1 = 0x00, .cnf2 = 0xB1, .cnf3 = 0x05 },
};

//Make Low CS Pin
static void MCP2515_CS_Low(void) {
	HAL_GPIO_WritePin(MCP2515_CS_PORT, MCP2515_CS_PIN, GPIO_PIN_RESET);
}

//Make High CS Pin
static void MCP2515_CS_High(void) {
	HAL_GPIO_WritePin(MCP2515_CS_PORT, MCP2515_CS_PIN, GPIO_PIN_SET);
}

/* One CS-low window, one HAL call, however many bytes the instruction needs.

   This replaces a byte-at-a-time helper, and the difference is not subtle.
   Measured on hardware, reading one frame took about 40 separate single-byte
   HAL_SPI_TransmitReceive calls costing 1.83 ms, of which only 82 us was the
   SPI transfer itself - the other 95% was per-call HAL overhead on a 16 MHz
   core. Batching removes that overhead rather than making the bus faster.

   HAL_SPI_TransmitReceive needs a valid receive pointer even when the reply
   is discarded, so write-style transactions still pass a scratch buffer. */
static void MCP2515_SPI_Transfer(const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    MCP2515_CS_Low();
    HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, length,
                            MCP2515_SPI_TIMEOUT_MS);
    MCP2515_CS_High();
}

// Reset MCP2515
void MCP2515_Reset(void)
{
    uint8_t tx = MCP2515_CMD_RESET;
    uint8_t rx;

    MCP2515_SPI_Transfer(&tx, &rx, 1);
    HAL_Delay(10);
}

// Read one register
uint8_t MCP2515_Read(uint8_t address)
{
    uint8_t tx[3] = { MCP2515_CMD_READ, address, 0x00 };
    uint8_t rx[3];

    MCP2515_SPI_Transfer(tx, rx, sizeof(tx));

    return rx[2];
}

//Read Status From MCP2515
uint8_t MCP2515_ReadStatus(void)
{
    uint8_t tx[2] = { MCP2515_CMD_READ_STATUS, 0x00 };
    uint8_t rx[2];

    MCP2515_SPI_Transfer(tx, rx, sizeof(tx));

    return rx[1];
}

//Write Data to a Register
void MCP2515_Write(uint8_t address, uint8_t data)
{
    uint8_t tx[3] = { MCP2515_CMD_WRITE, address, data };
    uint8_t rx[3];

    MCP2515_SPI_Transfer(tx, rx, sizeof(tx));
}

// Modify selected bits of a register, leaving the rest untouched
void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t data)
{
    uint8_t tx[4] = { MCP2515_CMD_BIT_MODIFY, address, mask, data };
    uint8_t rx[4];

    MCP2515_SPI_Transfer(tx, rx, sizeof(tx));
}

/* Requests the mode, then waits until CANSTAT reports that the chip is
   ACTUALLY in it. The previous version read CANSTAT once, immediately after
   writing the request; in loopback that happened to be fast enough, but
   entering NORMAL mode waits for the bus to go idle, so a single read there
   can report a spurious failure. */
bool MCP2515_SetMode(uint8_t mode)
{
    MCP2515_BitModify(MCP_CANCTRL, CANCTRL_REQOP_MASK, mode);

    uint32_t start = HAL_GetTick();
    do
    {
        if ((MCP2515_Read(MCP_CANSTAT) & CANCTRL_REQOP_MASK) == mode)
        {
            return true;
        }
    } while ((HAL_GetTick() - start) < MCP2515_MODE_TIMEOUT_MS);

    return false;
}

//Switch to Loopback Mode
bool MCP2515_SetLoopbackMode(void)
{
    return MCP2515_SetMode(MODE_LOOPBACK);
}

//Switch to Normal Mode
bool MCP2515_SetNormalMode(void)
{
    return MCP2515_SetMode(MODE_NORMAL);
}

//Switch to Configuration Mode
bool MCP2515_SetConfigurationMode(void)
{
    return MCP2515_SetMode(MODE_CONFIG);
}

//Select Communication Bitrate
bool MCP2515_SetBitrate(MCP2515_Bitrate_t bitrate)
{
#if (MCP2515_OSC_FREQUENCY == MCP2515_OSC_8MHZ)

    const MCP2515_BitTiming_t *timing = &bitTiming8MHz[bitrate];

#elif (MCP2515_OSC_FREQUENCY == MCP2515_OSC_16MHZ)

    const MCP2515_BitTiming_t *timing = &bitTiming16MHz[bitrate];

#else
#error Unsupported oscillator frequency
#endif

    MCP2515_Write(MCP_CNF1, timing->cnf1);
    MCP2515_Write(MCP_CNF2, timing->cnf2);
    MCP2515_Write(MCP_CNF3, timing->cnf3);

    /* Read back and verify: if the SPI link is broken, a Write is lost
       silently while a Read returns 0x00 - this check catches that case. */
    return (MCP2515_Read(MCP_CNF1) == timing->cnf1) &&
           (MCP2515_Read(MCP_CNF2) == timing->cnf2) &&
           (MCP2515_Read(MCP_CNF3) == timing->cnf3);
}

/* Puts both RX buffers in "accept everything" mode.
   After reset RXM is 00 (filtering enabled); since the filters are all zero
   every standard frame still gets through, but rather than relying on that
   implicit behaviour we set RXM=11 to disable filtering explicitly.
   BUKT matters here: the F103 sends the accel and gyro frames back to back
   (~300 us apart). While we are busy pushing a line out of the UART we
   cannot drain RXB0 in time, so BUKT lets the second frame land in RXB1
   instead of being lost to an overflow. */
static void MCP2515_ConfigureReceiveBuffers(void)
{
    MCP2515_Write(MCP_RXB0CTRL, RXBCTRL_RXM_ANY | RXB0CTRL_BUKT);
    MCP2515_Write(MCP_RXB1CTRL, RXBCTRL_RXM_ANY);

    MCP2515_Write(MCP_CANINTE, 0x00);   // INT pin is not wired, we poll instead
    MCP2515_Write(MCP_CANINTF, 0x00);   // clear any flags left over from reset
}

bool MCP2515_Init(MCP2515_Bitrate_t bitrate)
{
    /* CubeMX brings PA4 up LOW. If the SPI peripheral starts while CS is
       asserted, the MCP2515 mistakes the first clock edges for a command,
       so raise CS before anything else. */
    MCP2515_CS_High();
    HAL_Delay(10);

    MCP2515_Reset();

    /* After a reset the chip is GUARANTEED to be in Configuration mode
       (CANSTAT = 0x80). Reading anything else means the SPI wiring, the
       supply or the crystal is missing. */
    if ((MCP2515_Read(MCP_CANSTAT) & CANCTRL_REQOP_MASK) != MODE_CONFIG)
    {
        return false;
    }

    /* Writes to CNF1..CNF3 are only accepted in Configuration mode. */
    if (!MCP2515_SetBitrate(bitrate))
    {
        return false;
    }

    MCP2515_ConfigureReceiveBuffers();

    /* Normal mode both receives and acknowledges incoming frames. The ACK
       is essential: with no second node on the bus the F103 never sees one,
       so none of its frames count as transmitted. LISTEN-ONLY would receive
       without acknowledging. */
    return MCP2515_SetMode(MODE_NORMAL);
}

/* Loads identifier, DLC and data in one LOAD TX BUFFER transaction instead of
   six register writes. This node never transmits, so the path is not
   exercised on hardware here; it is kept consistent with the receive path
   rather than left as the odd one out. */
bool MCP2515_LoadTXBuffer(uint16_t id,
                          uint8_t length,
                          const uint8_t *data)
{
    uint8_t tx[MCP2515_MAX_TRANSFER] = {0};
    uint8_t rx[MCP2515_MAX_TRANSFER];

    if (length > 8)
    {
        return false;
    }

    tx[0] = MCP2515_CMD_LOAD_TXB0;
    tx[1] = (uint8_t)(id >> 3);            // SIDH: identifier bits 10:3
    tx[2] = (uint8_t)((id & 0x07) << 5);   // SIDL: bits 2:0 in the top three
    tx[3] = 0x00;                          // EID8, unused for standard frames
    tx[4] = 0x00;                          // EID0
    tx[5] = length;                        // DLC

    for (uint8_t i = 0; i < length; i++)
    {
        tx[6 + i] = data[i];
    }

    MCP2515_SPI_Transfer(tx, rx, (uint16_t)(6u + length));

    return true;
}

//Select TxB0 to send
void MCP2515_RequestToSend(void)
{
    uint8_t tx = MCP2515_CMD_RTS_TXB0;
    uint8_t rx;

    MCP2515_SPI_Transfer(&tx, &rx, 1);
}

uint8_t MCP2515_ReadAndClearOverflow(void)
{
    uint8_t overflow = MCP2515_Read(MCP_EFLG) & (EFLG_RX0OVR | EFLG_RX1OVR);

    if (overflow != 0u)
    {
        /* Writing zeros through a bit-modify clears only these two bits and
           leaves the error-passive and bus-off flags alone. */
        MCP2515_BitModify(MCP_EFLG, EFLG_RX0OVR | EFLG_RX1OVR, 0x00);
    }

    return overflow;
}

bool MCP2515_Receive(MCP2515_Frame_t *frame)
{
    /* READ STATUS costs two bytes and reports both receive flags at once, so
       the common case - nothing waiting - is a single short transaction. */
    uint8_t status = MCP2515_ReadStatus();
    uint8_t command;

    if ((status & MCP2515_STATUS_RX0IF) != 0u)
    {
        command = MCP2515_CMD_READ_RXB0;
    }
    else if ((status & MCP2515_STATUS_RX1IF) != 0u)
    {
        command = MCP2515_CMD_READ_RXB1;
    }
    else
    {
        return false;
    }

    /* One transaction for the whole buffer. Raising CS at the end also clears
       the matching RXnIF flag, which is why no bit-modify follows: with this
       instruction the read and the acknowledgement are the same operation.
       Nine separate register reads plus a bit-modify used to do this job. */
    uint8_t tx[MCP2515_MAX_TRANSFER] = {0};
    uint8_t rx[MCP2515_MAX_TRANSFER];

    tx[0] = command;
    MCP2515_SPI_Transfer(tx, rx, MCP2515_MAX_TRANSFER);

    /* rx[0] is clocked out while the command goes in. From rx[1] onwards:
       SIDH, SIDL, EID8, EID0, DLC, then D0..D7. */
    frame->id = ((uint16_t)rx[1] << 3) | (uint16_t)(rx[2] >> 5);

    frame->dlc = rx[5] & 0x0F;
    if (frame->dlc > 8)
    {
        frame->dlc = 8;   // keep a corrupt DLC from overrunning data[]
    }

    for (uint8_t i = 0; i < frame->dlc; i++)
    {
        frame->data[i] = rx[6 + i];
    }

    return true;
}
