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

// Define Register Address
#define MCP_CANSTAT   0x0E
#define MCP_CANCTRL   0x0F

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

typedef struct
{
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];

} MCP2515_Frame_t;
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

void MCP2515_Reset(void);

uint8_t MCP2515_Read(uint8_t address);

uint8_t MCP2515_ReadStatus(void);

void MCP2515_Write(uint8_t address, uint8_t data);

void MCP2515_BitModify(uint8_t address, uint8_t mask, uint8_t data);

bool MCP2515_SetLoopbackMode(void);

bool MCP2515_SetConfigurationMode(void);

bool MCP2515_SetBitrate(MCP2515_Bitrate_t bitrate);

bool MCP2515_LoadTXBuffer(uint16_t id,uint8_t length, const uint8_t *data);

void MCP2515_RequestToSend(void);

bool MCP2515_ReadRXBuffer(MCP2515_Frame_t *frame);
#endif /* DRIVERS_MCP2515_INC_MCP2515_H_ */
