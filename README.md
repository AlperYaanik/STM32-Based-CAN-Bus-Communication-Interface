# STM32 CAN Bus Communication Interface

A bare-metal, two-node CAN network built from scratch on STM32. An **MPU6050** IMU is
sampled over I²C by an **STM32F103**, published on a **500 kbit/s CAN bus** through the
MCU's native bxCAN peripheral, and received by an **STM32F401** using an external
**MCP2515** CAN controller over SPI.

The repository is organised as four incremental STM32CubeIDE projects. Each one
validates a single layer in isolation before the next layer is added — the two
loopback projects are kept on purpose, because they are the reason the final
integration came up quickly.

The sender is bare-metal. The receiver runs FreeRTOS; its bare-metal predecessor is
preserved at git tag `v1-bare-metal`, and the measurements below are what motivated the
change. Otherwise no middleware: HAL plus hand-written drivers.

---

## Result

The link is verified end to end on real hardware. The receiver prints decoded frames
as they arrive:

```
=== f401 MCP2515 receiver node ===
MCP2515 ready - 500 kbit/s, normal mode, listening for 0x101/0x102
ACCEL  x=  1200  y=    20  z= 17880
GYRO   x=   -20  y=   -28  z=   -34
```

The values are physically consistent — with the board flat the Z axis carries gravity,
X and Y sit near zero, and the gyroscope reads zero at rest once its bias is
calibrated — which confirms the I²C burst read, the big-endian packing, the CAN
transport and the MCP2515 reception together.

**A correction worth keeping visible.** An early check measured the accelerometer
vector at 0.9985 g and was taken as proof of a calibrated sensor. It was orientation
luck: the board was tilted, so the Z axis carried only a quarter of the vector.
Re-measured flat and rigidly mounted, the magnitude is **1.099 g** — a ~10 % error
isolated to Z, confirmed to be the sensor rather than mounting stress. A three-axis
sensor cannot be validated from a single orientation. Accelerometer calibration is
deliberately out of scope for this stage; the data path it travels through is not
affected.

---

## Measured performance

Both nodes report once a second what they actually achieved. The receiver reads the
MCP2515's own overflow flags, so dropped frames are reported by the hardware rather
than estimated. Figures below are from hardware runs.

### Where the bare-metal design breaks

| Run | Sender config | Sender achieved | Receiver handled | Loss |
|---|---|---|---|---|
| D1 | 100 ms period, logging on | 10 Hz | all | 0 % |
| D2 | 10 ms period, logging on | 70.8 Hz of 100 | all | 0 % |
| D3 | 10 ms period, logging off | 92.3 Hz of 100 | all | 0 % |
| D4 | free-running, logging on both nodes | 500 frames/s | 187 frames/s | **63 %** |
| D5 | free-running, logging off both nodes | 2019 frames/s | 547 frames/s | **73 %** |

D2 against D3 isolates the cost of one `printf` line in the sampling path: **21.5 Hz,
about 30 % of the requested rate.** Neither the sensor, nor CAN, nor the CPU was the
constraint.

### Two different bottlenecks, two different fixes

D5 showed the receiver still overflowing with all printing disabled. The remaining
cost was the MCP2515 driver: about 40 single-byte `HAL_SPI_TransmitReceive` calls per
frame, 1.83 ms, of which only 82 µs was SPI. Batching each instruction into one
transfer and using `READ STATUS` / `READ RX BUFFER` cut that to 3 calls:

| Run | Before driver fix | After driver fix |
|---|---|---|
| D4 (logging on) | 187 frames/s, 63 % loss | 249.5 frames/s, 50 % loss |
| D5 (logging off) | 547 frames/s, 73 % loss | **2019 frames/s, 0 % loss** |

This is the distinction the measurements were built to draw: **the driver bottleneck
is fixed by a better driver, not by an RTOS.** What remains in D4 — logging sitting in
the data path — is the part an RTOS addresses. The target for the RTOS version is D5's
2019 frames/s *with* logging enabled, about 8× today's figure. The CAN bus itself was
44 % loaded at the highest rate reached and never the limiting factor.

### Loss under saturation is not random

With the receiver saturated (D4), frames were broken down by identifier and by receive
buffer. The sender emits accel and gyro in equal numbers and reports no transmit
failures, yet:

| Run | Change | accel | gyro | Survivor |
|---|---|---|---|---|
| T1 | baseline | 17 | 233 | second frame of each pair, 93 % |
| T2 | receiver services RXB1 before RXB0 | 29 | 221 | second frame, 88 % |
| T3 | sender transmits gyro before accel | 240 | 10 | second frame, 96 % |

Reversing the receiver's buffer order (T2) only changed which buffer did the work — the
other held one stale frame indefinitely, which also disabled rollover in practice — and
left the bias intact. Reversing the transmit order (T3) inverted it. **Survival follows
a frame's position in the pair, not its identity.** In a real system this is a message
type being silently starved while the link appears healthy.

Open question: the microscopic mechanism. Sender and receiver both run at exactly
250.0 per second here, i.e. the two loops are phase-locked, and the receiver
consistently finds the pair's second frame in its one active slot. That is not fully
reconciled with the controller discarding new frames when a buffer is full. A
receive queue removes the effect regardless, which is part of what the RTOS stage
delivers.

### After moving the receiver onto FreeRTOS

Same sender at full rate (2019 frames/s), same 16 MHz clock, same SPI driver — only
the receiver's architecture changed. `logdrop` counts frames that were received and
counted but not printed.

| Run | Receiver | Per-frame logging | Received | Controller overflow | accel : gyro | logdrop |
|---|---|---|---|---|---|---|
| R0 | bare-metal | on | 249 frames/s | 248 /s | 49 : 200 | — |
| **R1** | **FreeRTOS** | **on** | **2022 frames/s** | **0** | **1 : 1** | ~2000 /s |
| R2 | FreeRTOS | off | 2023 frames/s | 0 | 1 : 1 | 0 |
| R3 | FreeRTOS, sender at 10 Hz | on | 20 frames/s | 0 | 1 : 1 | 0 |

**The receiver now takes every frame the sender emits, with logging on — 8.1× the
bare-metal figure and no controller overflow.** The loss has moved exactly where the
design put it: into log lines that are counted rather than into data. The starvation of
one message type is gone too; with both receive buffers drained on every interrupt,
frames alternate between RXB0 and RXB1 and accel and gyro arrive one-for-one.

The measurement also exposed the next limit. In R1 the once-a-second report arrived
every 4–5 seconds and only one frame line was printed per report: CanRxTask and its
interrupts were using nearly all of the 16 MHz core, leaving the low-priority LogTask
almost nothing. That is the priority scheme working as intended — the data path keeps
every frame while the printer starves — but it also means there is no CPU headroom at
this rate. Rates in R1 are still exact, because they are computed from the real elapsed
time of each report window. The clock, left untouched for this comparison, is the
obvious next variable.

Resource use measured by the RTOS itself: 7.0 KB of the 16 KB heap, 80 of 512 stack
words for CanRxTask and 205 of 1024 for LogTask.

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
| [`f401_mcp2515_node`](f401_mcp2515_node) | STM32F401CCU6 | The receiver: interrupt-driven MCP2515 reception on FreeRTOS, frame decoding and UART reporting |

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

Stage 1, bare-metal two-node communication, is complete and hardware-verified, and its
limits are measured (see *Measured performance*). Gyroscope bias calibration is done.

Planned next:

- FreeRTOS on the sender.
- Raise the receiver's clock from 16 MHz. Measured CPU load at the sender's full rate
  (`rx` ~90%, `idle` ~0%) leaves no headroom; a faster core is the remaining lever once
  the SPI-side reduction above is verified on hardware.
- Fault recovery. The sender once went silent after cabling was changed and recovered
  only on reset — either bxCAN bus-off with automatic recovery disabled, or an I²C bus
  lock-up. Neither path currently recovers on its own.
- Accelerometer calibration (six-position test), deferred.

Done and measured: FreeRTOS on the receiver (2019 of 2019 frames/s with logging on,
against 249 bare-metal), CPU load via FreeRTOS run-time statistics, and a receive-path
SPI reduction from 6 to 4 transactions per pair (pending hardware re-verification).

---

## License

[MIT](LICENSE), covering the code in this repository. Vendored third-party code
(ST's CMSIS headers and HAL drivers, the FreeRTOS kernel) keeps its own license,
noted in the same file.
