# STM32 CAN Bus Communication Interface

A bare-metal, two-node CAN network built from scratch on STM32. An **MPU6050** IMU is
sampled over I²C by an **STM32F103**, published on a **500 kbit/s CAN bus** through the
MCU's native bxCAN peripheral, and received by an **STM32F401** using an external
**MCP2515** CAN controller over SPI.

The repository is organised as four incremental STM32CubeIDE projects. Each one
validates a single layer in isolation before the next layer is added — the two
loopback projects are kept on purpose, because they are the reason the final
integration came up quickly.

No RTOS, no third-party middleware: HAL plus hand-written drivers.

---

## Result

The link is verified end to end on real hardware. With the sender board at rest, the
receiver prints:

```
=== f401 MCP2515 receiver node ===
MCP2515 ready - 500 kbit/s, normal mode, listening for 0x101/0x102
ACCEL  x=-12616  y= -9604  z=  4032
GYRO   x=  -477  y=   -95  z=    -4
ACCEL  x=-12620  y= -9576  z=  4116
GYRO   x=  -456  y=   -50  z=     8
```

The accelerometer vector magnitude is

```
sqrt(12616² + 9604² + 4032²) = 16360 LSB
16360 / 16384 LSB-per-g      = 0.9985 g
```

which is gravity, to within 0.15 %. That single number confirms the whole chain at
once: I²C burst read, big-endian packing, CAN transport, MCP2515 reception and
unpacking all have to be correct for it to come out at 1 g. The strict `ACCEL`/`GYRO`
alternation with no gaps separately confirms that the MCP2515 rollover buffer is
doing its job.

---

## Architecture

```
        SENDER NODE                        CAN BUS                  RECEIVER NODE
      STM32F103C8T6                     500 kbit/s                 STM32F401CCU6
 ┌───────────────────────┐                                    ┌───────────────────────┐
 │                       │                                    │                       │
 │  MPU6050 ──I²C──▶ CPU │                                    │ CPU ◀──SPI── MCP2515  │
 │                    │  │                                    │  │                 │  │
 │                    ▼  │                                    │  │                 │  │
 │                 bxCAN │                                    │  │                 │  │
 └────────────────────┬──┘                                    └──┼─────────────────┼──┘
                      │                                          │                 │
                 ┌────▼─────┐        CANH ──────────────┐   ┌────▼─────┐      ┌────▼─────┐
                 │transceiver│───────CANL ─────────────┐│   │  UART1   │      │transceiver│
                 └──────────┘                          ││   │ 115200   │      └──────────┘
                                                       ││   └────┬─────┘
                      ▲                                ▼▼        │
                      └────────────── 120 Ω ───────────────┐     ▼
                                                                PC terminal
```

Data flow: IMU sample → two CAN frames (`0x101` accel, `0x102` gyro) at 10 Hz →
bus → MCP2515 receive buffers → SPI read → decoded and printed over UART.

---

## Repository layout

The projects are listed in the order they were built. Each has its own README with
the pin map and configuration details.

| Project | Target | What it proves |
|---|---|---|
| [`f103_can_loopback`](f103_can_loopback) | STM32F103C8T6 | bxCAN configuration, acceptance filters, mailboxes and FIFO handling — verified in internal loopback, with no transceiver hardware involved |
| [`f401_MCP2515_CANBUS-SPI_Loopback`](f401_MCP2515_CANBUS-SPI_Loopback) | STM32F401CCU6 | SPI transport to the MCP2515 and its register map, brought up one register at a time, ending in an internal loopback transmit/receive |
| [`f103_node_sensor`](f103_node_sensor) | STM32F103C8T6 | The sender: MPU6050 driver over I²C, and periodic CAN transmission on a real bus |
| [`f401_mcp2515_node`](f401_mcp2515_node) | STM32F401CCU6 | The receiver: MCP2515 reception on a real bus, frame decoding and UART reporting |

The two nodes share a contract file, [`can_protocol.h`](f401_mcp2515_node/Core/Inc/can_protocol.h),
duplicated byte-identically in both projects because separate CubeIDE projects cannot
share a single source file. It is the one file that must never drift.

---

## Bus and frame format

| Parameter | Value |
|---|---|
| Bitrate | 500 kbit/s |
| Identifier | 11-bit standard |
| Byte order | Big-endian (MSB first) |
| Rate | 10 Hz (both frames per sample) |
| Bus load | ~0.5 % |

| ID | Name | DLC | Payload |
|---|---|---|---|
| `0x101` | ACCEL | 6 | `XH XL YH YL ZH ZL`, raw `int16`, ±2 g scale (16384 LSB/g) |
| `0x102` | GYRO | 6 | `XH XL YH YL ZH ZL`, raw `int16`, ±250 °/s scale (131 LSB per °/s) |

Raw counts are transmitted deliberately. Converting to engineering units is the
receiver's job, which keeps the sender free of floating-point work and keeps the data
lossless on the wire.

---

## Hardware

- STM32F103C8T6 board ("Blue Pill") — sender
- STM32F401CCU6 board ("Black Pill") — receiver
- MPU6050 / GY-521 IMU breakout
- MCP2515 CAN controller module with an 8 MHz crystal
- Two CAN transceivers, 120 Ω termination at both ends of the bus
- ST-Link V2 for flashing, USB-UART adapter for the console

Both boards run from the internal HSI oscillator; no external crystal is required on
either MCU.

---

## Engineering notes

The findings below were the non-obvious parts of the bring-up and are documented
because they cost real debugging time.

**MCP2515 supply voltage and SPI clock.** The MCP2515 is powered at 3V3 rather than
5 V on purpose: at 5 V its logic-high input threshold is `0.7 × VDD = 3.5 V`, which the
F401's 3.3 V SPI outputs cannot reliably reach. The trade-off is that below 4.5 V the
MCP2515's maximum SPI clock drops from 10 MHz to 5 MHz, so SPI1 is configured for
4 MHz. Note that the transceiver on most MCP2515 modules (MCP2551, TJA1050) is a 5 V
part; if a module refuses to enter normal mode, that is the first thing to check.

**Entering MCP2515 normal mode is not instantaneous.** The chip will not leave
configuration mode until it has observed 11 consecutive recessive bits on the bus.
Reading `CANSTAT` once immediately after requesting the mode therefore reports a
spurious failure; the driver polls with a timeout instead. This also makes
`CANSTAT = 0x80` a precise diagnostic: SPI is working and the chip is alive, but the
bus is not idle — usually an unpowered transceiver or swapped CANH/CANL.

**Receive buffer rollover is mandatory here, not optional.** The sender emits the
accel and gyro frames back to back, roughly 300 µs apart, while the receiver is busy
pushing a line out of the UART. Without the `BUKT` bit in `RXB0CTRL`, the second frame
is lost to an overflow and only `ACCEL` lines ever appear. With it, the frame rolls
into `RXB1` and both survive.

**Sample points differ between the two controllers, and that is acceptable.** bxCAN on
the F103 samples at 87.5 % (16 time quanta), while the MCP2515 with an 8 MHz crystal
can only reach 500 kbit/s with 8 time quanta, sampling at 62.5 %. CAN requires the two
nodes to agree on the *bitrate*; the sample point affects propagation-delay and
oscillator-tolerance margin, and on a short two-node bus there is margin to spare.

**One-shot transmission is a deliberate choice.** `AutoRetransmission` is disabled on
the sender. With it enabled, a frame that goes unacknowledged — because the receiver
is powered down — would be retried indefinitely, filling all three mailboxes and
eventually driving the node error-passive and then bus-off. For a periodic sensor
stream a dropped sample is worth less than the next one, so the frame is discarded and
the node keeps running.

---

## Building

Each project is an STM32CubeIDE project. Import with
*File → Import → Existing Projects into Workspace*, select the project directory, then
build and flash over ST-Link.

For `f401_mcp2515_node`, import the project file at the **project root**
(`f401_mcp2515_node/.project`). A second, stale skeleton exists under
`f401_mcp2515_node/STM32CubeIDE/`; it predates the MCP2515 and debug modules and does
not reference them, so importing that one fails at link time.

Serial console settings for both nodes: **115200 baud, 8N1, no flow control**.

Verifying without the full setup:

- `f103_can_loopback` needs only the F103 board and a USB-UART adapter — bxCAN's
  internal loopback mode needs no transceiver and no second node.
- `f401_MCP2515_CANBUS-SPI_Loopback` needs the F401 board and the MCP2515 module,
  but no bus and no second node.

---

## Status and next steps

Stage 1, bare-metal two-node communication, is complete and hardware-verified.

Planned next:

- Gyroscope bias calibration. At rest the gyro reads about −3.7 °/s on X, an
  uncalibrated zero offset well within the part's ±20 °/s specification. It is
  harmless while raw counts are being shipped, but must be removed before any angle
  integration.
- Migration to FreeRTOS. The current polling loops were deliberately structured to
  translate into tasks and queues.
