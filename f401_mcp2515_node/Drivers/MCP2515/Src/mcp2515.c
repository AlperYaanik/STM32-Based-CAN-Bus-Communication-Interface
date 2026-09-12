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
static void MCP2515_CS_Low() {
	HAL_GPIO_WritePin(MCP2515_CS_PORT, MCP2515_CS_PIN, GPIO_PIN_RESET);
}

//Make High CS Pin
static void MCP2515_CS_High() {
	HAL_GPIO_WritePin(MCP2515_CS_PORT, MCP2515_CS_PIN, GPIO_PIN_SET);
}

// Transmit Data via MCP2515 SPI-CAN BUS
static uint8_t MCP2515_SPI_Transmit(uint8_t data) {
	uint8_t Rx;

	HAL_SPI_TransmitReceive(&hspi1, &data, &Rx, 1, 100);

	return Rx;
}

// Reset MCP2515
void MCP2515_Reset() {
	MCP2515_CS_Low();
	MCP2515_SPI_Transmit(0xC0);
	MCP2515_CS_High();
	HAL_Delay(10);
	//MCP2515 Commands {RESET 0xC0, READ 0x03,WRITE 0x02, BIT MODIFY 0x05}
}

// Read From MCP2515
uint8_t MCP2515_Read(uint8_t address) {
	uint8_t value;

	MCP2515_CS_Low();

	MCP2515_SPI_Transmit(0x03);
	MCP2515_SPI_Transmit(address);
	value = MCP2515_SPI_Transmit(0x00);

	MCP2515_CS_High();
	return value;
}

//Read Status From MCP2515
uint8_t MCP2515_ReadStatus(void)
{
    uint8_t status;

    MCP2515_CS_Low();

    MCP2515_SPI_Transmit(0xA0);      // READ STATUS Command
    status = MCP2515_SPI_Transmit(0x00);

    MCP2515_CS_High();

    return status;
}
//Write Data to a Register
void MCP2515_Write(uint8_t address, uint8_t data)
{
    MCP2515_CS_Low();

    MCP2515_SPI_Transmit(0x02);   // WRITE command
    MCP2515_SPI_Transmit(address);
    MCP2515_SPI_Transmit(data);

    MCP2515_CS_High();
}

// Modify MCP2515 Bit (to LOOPBACK State) {CONFIGURATION,NORMAL,LOOPBACK,LISTEN ONLY}
void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t data) {
	MCP2515_CS_Low();

	MCP2515_SPI_Transmit(0x05);
	MCP2515_SPI_Transmit(address);
	MCP2515_SPI_Transmit(mask);
	MCP2515_SPI_Transmit(data);

	MCP2515_CS_High();
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
bool MCP2515_SetLoopbackMode()
{
    return MCP2515_SetMode(MODE_LOOPBACK);
}

//Switch to Normal Mode
bool MCP2515_SetNormalMode()
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

//Prepare TX Buffer to Send Message
bool MCP2515_LoadTXBuffer(uint16_t id,
                          uint8_t length,
                          const uint8_t *data)
{
    if(length > 8)
        return false;

    MCP2515_Write(MCP_TXB0SIDH, id >> 3);
    MCP2515_Write(MCP_TXB0SIDL, (id & 0x07) << 5);

    MCP2515_Write(MCP_TXB0EID8, 0x00);
    MCP2515_Write(MCP_TXB0EID0, 0x00);

    MCP2515_Write(MCP_TXB0DLC, length);

    for(uint8_t i = 0; i < length; i++)
    {
        MCP2515_Write(MCP_TXB0D0 + i, data[i]);
    }

    return true;
}

//Select TxB0 to send
void MCP2515_RequestToSend(void)
{
    MCP2515_CS_Low();

    MCP2515_SPI_Transmit(0x81);

    MCP2515_CS_High();
}

/* RXB0 and RXB1 use the same register layout in the same order: SIDH, SIDL,
   EID8, EID0, DLC, D0..D7. One function can therefore read either buffer by
   offsetting from its SIDH address. */
static void MCP2515_ReadFrameAt(uint8_t sidhAddress, MCP2515_Frame_t *frame)
{
    uint8_t sidh = MCP2515_Read(sidhAddress);
    uint8_t sidl = MCP2515_Read(sidhAddress + 1);

    /* 11-bit identifier: the top 8 bits live in SIDH, the low 3 in bits
       7:5 of SIDL. */
    frame->id = ((uint16_t)sidh << 3) | (uint16_t)(sidl >> 5);

    frame->dlc = MCP2515_Read(sidhAddress + 4) & 0x0F;
    if (frame->dlc > 8)
    {
        frame->dlc = 8;   // keep a corrupt DLC from overrunning data[]
    }

    for (uint8_t i = 0; i < frame->dlc; i++)
    {
        frame->data[i] = MCP2515_Read(sidhAddress + 5 + i);
    }
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
    /* The old MCP2515_ReadRXBuffer read unconditionally and always returned
       true, so stale or garbage buffer contents were reported as a fresh
       frame even when nothing had arrived. Check CANINTF first instead. */
    uint8_t flags = MCP2515_Read(MCP_CANINTF);

    if (flags & CANINTF_RX0IF)
    {
        MCP2515_ReadFrameAt(MCP_RXB0SIDH, frame);
        /* Without clearing the flag the same frame is read forever, and no
           room is freed for the next one. BitModify clears the single bit
           without disturbing the other flags. */
        MCP2515_BitModify(MCP_CANINTF, CANINTF_RX0IF, 0x00);
        return true;
    }

    if (flags & CANINTF_RX1IF)
    {
        MCP2515_ReadFrameAt(MCP_RXB1SIDH, frame);
        MCP2515_BitModify(MCP_CANINTF, CANINTF_RX1IF, 0x00);
        return true;
    }

    return false;
}
