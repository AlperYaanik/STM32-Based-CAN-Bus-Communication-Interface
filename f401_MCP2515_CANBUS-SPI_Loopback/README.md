# f401_MCP2515_CANBUS-SPI_Loopback

**Target:** STM32F401CCU6 · **Role:** MCP2515 driver bring-up, step 2 of 4

The STM32F401 has no CAN controller of its own, so the second node needs an external
one: a **MCP2515** driven over SPI. This project brings that driver up from nothing, one
register at a time, and finishes with an internal loopback transmit/receive.

It needs the F401 board and the MCP2515 module, but **no CAN bus and no second node**.

## Incremental test suite

The distinguishing feature of this project is that `main.c` keeps the whole bring-up
sequence as a series of test blocks, each annotated with the output that confirmed it
passed. They are commented out rather than deleted, so the ladder used to reach a
working driver is still readable:

| Step | Test | Expected result |
|---|---|---|
| 1 | Read `CANCTRL` after power-up | `0x87` — the chip answers at all, so SPI framing and CS are correct |
| 2 | Write `0x80` to `CANCTRL`, read it back | `0x80` — writes actually land |
| 3 | `READ STATUS` command | `0x00` — multi-byte command sequences work |
| 4 | Request configuration mode, read `CANSTAT` | Mode bits report configuration |
| 5 | Write `CNF1`/`CNF2`/`CNF3` | Bit timing accepted in configuration mode |
| 6 | Loopback: load TX buffer, request to send, read RX buffer | The transmitted ID, DLC and data come back |

The ordering matters: a failure at step 1 is wiring, at step 2 is the write command
encoding, at step 6 is bit timing or buffer handling. Each step removes a class of
cause before the next one is attempted.

## Configuration

| Item | Value |
|---|---|
| System clock | HSI 16 MHz, no PLL |
| SPI | SPI1, master, mode 0 (CPOL 0 / CPHA 0), 8-bit, prescaler 32 → **500 kHz** |
| MCP2515 oscillator | 8 MHz crystal |
| UART | USART1, 115200 8N1 |

The deliberately slow 500 kHz SPI clock is a bring-up choice: signal-integrity
problems are removed from the list of suspects while the register-level logic is
being proven.

### Pin map

| Pin | Function |
|---|---|
| PA4 | MCP2515 CS (software-controlled) |
| PA5 | SPI1_SCK |
| PA6 | SPI1_MISO |
| PA7 | SPI1_MOSI |
| PA9 | USART1_TX |
| PA10 | USART1_RX |

## What carries forward

The driver developed here — the SPI command encodings, the mode-switch helpers and the
bit-timing tables — becomes the starting point for
[`f401_mcp2515_node`](../f401_mcp2515_node). That project extends it for real
reception: interrupt-flag polling, the second receive buffer, and a mode-change routine
that waits instead of assuming.

## Note

This is a bring-up scratchpad, and it reads like one. It is kept in the repository
because the sequence above is the useful part, not the final state of the code.
