/*
 * mcp2515.c
 *
 *  Created on: Jul 26, 2026
 *      Author: alper
 */

#include "mcp2515.h"

extern SPI_HandleTypeDef hspi1;

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

//Switch to Loopback Mode
void MCP2515_SetLoopbackMode()
{
	MCP2515_BitModify(
			MCP_CANCTRL,
			CANCTRL_REQOP_MASK,
			MODE_LOOPBACK
			);
	uint8_t status = MCP2515_Read(MCP_CANSTAT);

    return ((status & CANCTRL_REQOP_MASK) == MODE_LOOPBACK);

}

//Switch to Configuration Mode
bool MCP2515_SetConfigurationMode(void)
{
    MCP2515_BitModify(
        MCP_CANCTRL,
        CANCTRL_REQOP_MASK,
        MODE_CONFIG
    );

    uint8_t status = MCP2515_Read(MCP_CANSTAT);

    return ((status & CANCTRL_REQOP_MASK) == MODE_CONFIG);
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

    return true;
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

bool MCP2515_ReadRXBuffer(MCP2515_Frame_t *frame)
{
    uint8_t sidh;
    uint8_t sidl;

    /* Read Standard ID */
    sidh = MCP2515_Read(MCP_RXB0SIDH);
    sidl = MCP2515_Read(MCP_RXB0SIDL);

    frame->id = ((uint16_t)sidh << 3);
    frame->id |= (sidl >> 5);

    /* Read Data Length Code */
    frame->dlc = MCP2515_Read(MCP_RXB0DLC) & 0x0F;

    /* Read Data Bytes */
    for(uint8_t i = 0; i < frame->dlc; i++)
    {
        frame->data[i] = MCP2515_Read(MCP_RXB0D0 + i);
    }

    return true;
}
