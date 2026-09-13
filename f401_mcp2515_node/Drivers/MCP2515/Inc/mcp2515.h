/*
 * mcp2515.h
 *
 *  Created on: Jul 26, 2026
 *      Author: alper
 */

#ifndef DRIVERS_MCP2515_INC_MCP2515_H_
#define DRIVERS_MCP2515_INC_MCP2515_H_

#include "main.h"
#include <stdbool.h>

/* SPI instruction set. Every transaction begins with one of these, sent
   while CS is held low. */
#define MCP2515_CMD_RESET        0xC0
#define MCP2515_CMD_READ         0x03
#define MCP2515_CMD_WRITE        0x02
#define MCP2515_CMD_BIT_MODIFY   0x05
#define MCP2515_CMD_READ_STATUS  0xA0
#define MCP2515_CMD_RTS_TXB0     0x81

/* LOAD TX BUFFER, starting at TXB0SIDH. Writes the identifier, DLC and data
   in one transaction instead of six separate register writes. */
#define MCP2515_CMD_LOAD_TXB0    0x40

/* READ RX BUFFER, starting at RXBnSIDH. Two things make these worth using
   over plain register reads: the whole buffer comes out in a single CS-low
   sequence, and the matching CANINTF.RXnIF flag is cleared automatically
   when CS rises - so no follow-up bit-modify is needed. */
#define MCP2515_CMD_READ_RXB0    0x90
#define MCP2515_CMD_READ_RXB1    0x94

/* READ STATUS reply bits. Bits 0 and 1 mirror CANINTF.RX0IF / RX1IF, which
   is all the receive path needs, and the command costs two bytes instead of
   the three a register read would. */
#define MCP2515_STATUS_RX0IF     0x01
#define MCP2515_STATUS_RX1IF     0x02

// Define Register Address
#define MCP_CANSTAT   0x0E
#define MCP_CANCTRL   0x0F

// Error / counter registers, used for diagnostics
#define MCP_TEC       0x1C   // Transmit Error Counter
#define MCP_REC       0x1D   // Receive Error Counter
#define MCP_EFLG      0x2D   // Error Flag

// Interrupt registers. The INT pin is not wired, but the flags are polled.
#define MCP_CANINTE   0x2B
#define MCP_CANINTF   0x2C

//Define Configuration Mode Registers
#define MCP_CNF1      0x2A
#define MCP_CNF2      0x29
#define MCP_CNF3      0x28

typedef struct
{
    uint8_t cnf1;
    uint8_t cnf2;
    uint8_t cnf3;
} MCP2515_BitTiming_t;


/* TX Buffer 0 Registers */
#define MCP_TXB0CTRL      0x30
#define MCP_TXB0SIDH      0x31
#define MCP_TXB0SIDL      0x32
#define MCP_TXB0EID8      0x33
#define MCP_TXB0EID0      0x34
#define MCP_TXB0DLC       0x35
#define MCP_TXB0D0        0x36

/* RX Buffer 0 Registers */
#define MCP_RXB0CTRL    0x60
#define MCP_RXB0SIDH    0x61
#define MCP_RXB0SIDL    0x62
#define MCP_RXB0EID8    0x63
#define MCP_RXB0EID0    0x64
#define MCP_RXB0DLC     0x65
#define MCP_RXB0D0      0x66

/* RX Buffer 1 Registers - a frame arriving while RXB0 is full rolls over here */
#define MCP_RXB1CTRL    0x70
#define MCP_RXB1SIDH    0x71
#define MCP_RXB1SIDL    0x72
#define MCP_RXB1EID8    0x73
#define MCP_RXB1EID0    0x74
#define MCP_RXB1DLC     0x75
#define MCP_RXB1D0      0x76

/* CANINTF bits */
#define CANINTF_RX0IF   0x01
#define CANINTF_RX1IF   0x02

/* EFLG overflow bits. The chip sets these when a frame arrives with no free
   receive buffer to put it in - in other words, they are the hardware's own
   count of frames this node lost. They are sticky and must be cleared. */
#define EFLG_RX0OVR     0x40
#define EFLG_RX1OVR     0x80

/* RXBnCTRL bits */
#define RXBCTRL_RXM_ANY 0x60   // masks/filters off: accept every valid frame
#define RXB0CTRL_BUKT   0x04   // if RXB0 is full, put the next frame in RXB1

typedef struct
{
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];

    /* Which receive buffer the frame was taken from, 0 or 1. Not part of the
       CAN frame itself; kept so reception behaviour under load can be
       measured rather than inferred. */
    uint8_t buffer;

} MCP2515_Frame_t;

/* Order in which MCP2515_Receive services the two receive buffers when both
   hold a frame. 0 = RXB0 first (the natural order), 1 = RXB1 first.

   This exists for one experiment: under saturation one message type survives
   far more often than the other, and the suspected cause is this servicing
   order. If flipping it flips the bias, the mechanism is confirmed. */
#define MCP2515_SERVICE_RXB1_FIRST  0
//Define Mode Values
#define MODE_NORMAL   0x00
#define MODE_SLEEP    0x20
#define MODE_LOOPBACK 0x40
#define MODE_LISTEN   0x60
#define MODE_CONFIG   0x80

//Define Mode Masks
#define CANCTRL_REQOP_MASK   0xE0

//MCP2515 Pin Definitions
#define MCP2515_CS_PORT GPIOA
#define MCP2515_CS_PIN  GPIO_PIN_4

//MCP2515 Crystal Oscillator Definition
#define MCP2515_OSC_8MHZ      8000000UL
#define MCP2515_OSC_16MHZ     16000000UL
#define MCP2515_OSC_FREQUENCY MCP2515_OSC_8MHZ

//MCP2515 CAN BIT SPEEDS
#define MCP2515_CAN_125KBPS   125000UL
#define MCP2515_CAN_250KBPS   250000UL
#define MCP2515_CAN_500KBPS   500000UL

typedef enum
{
    MCP2515_BITRATE_125KBPS = 0,
    MCP2515_BITRATE_250KBPS,
    MCP2515_BITRATE_500KBPS
} MCP2515_Bitrate_t;

/* Whole bring-up sequence in one call: raise CS -> reset -> confirm the chip
   answers -> bit timing -> RX buffer setup -> Normal mode. On false, the
   caller should dump CANSTAT to see how far it got. */
bool MCP2515_Init(MCP2515_Bitrate_t bitrate);

void MCP2515_Reset(void);

uint8_t MCP2515_Read(uint8_t address);

uint8_t MCP2515_ReadStatus(void);

void MCP2515_Write(uint8_t address, uint8_t data);

void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t data);

/* A mode change is not instantaneous; this waits until CANSTAT really shows
   the requested mode, or times out. */
bool MCP2515_SetMode(uint8_t mode);

bool MCP2515_SetLoopbackMode(void);

bool MCP2515_SetNormalMode(void);

bool MCP2515_SetConfigurationMode(void);

bool MCP2515_SetBitrate(MCP2515_Bitrate_t bitrate);

bool MCP2515_LoadTXBuffer(uint16_t id,uint8_t length, const uint8_t *data);

void MCP2515_RequestToSend(void);

/* Non-blocking. Returns true only if a frame was actually received; it then
   fills *frame and clears the matching RXnIF flag. Otherwise false. */
bool MCP2515_Receive(MCP2515_Frame_t *frame);

/* Returns the EFLG_RXnOVR bits that were set since the last call, and clears
   them. Non-zero means the receive buffers were full when a frame arrived and
   that frame was dropped - the direct, hardware-reported measure of whether
   this node is draining the controller fast enough. */
uint8_t MCP2515_ReadAndClearOverflow(void);

#endif /* DRIVERS_MCP2515_INC_MCP2515_H_ */
