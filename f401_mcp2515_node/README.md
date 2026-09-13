# f401_mcp2515_node

**Target:** STM32F401CCU6 · **Role:** receiver node, step 4 of 4

Receives the IMU frames published by [`f103_node_sensor`](../f103_node_sensor) from a
real 500 kbit/s CAN bus, using an external **MCP2515** controller over SPI, and reports
them over UART.

Because the node runs in normal mode rather than listen-only, it also **acknowledges**
frames on the bus. That matters: with no acknowledging node present, the sender cannot
count any frame as transmitted.

## What it does

Bring-up runs once at start-up: raise CS, reset, confirm the chip answers, program bit
timing, configure both receive buffers, enter normal mode. The main loop then polls
`CANINTF`, reads whichever buffer holds a frame, clears its flag, decodes the payload
and prints it.

Each received frame also toggles the PC13 LED, so the link can be seen to be alive
without a terminal attached.

## Modules

| File | Purpose |
|---|---|
| `Drivers/MCP2515/Src/mcp2515.c` | MCP2515 driver — SPI commands, modes, bit timing, reception |
| `Core/Src/debug.c` | `LOG(...)` over USART1, compiled out when `DEBUG_ENABLED` is 0 |
| `Core/Inc/can_protocol.h` | Frame format shared with the sender — **must stay byte-identical in both projects** |

## Configuration

| Item | Value |
|---|---|
| System clock | HSI 16 MHz, no PLL |
| SPI | SPI1, master, mode 0, **4 MHz** (prescaler 4) |
| MCP2515 supply | **3V3** |
| MCP2515 oscillator | 8 MHz crystal |
| CAN bit timing | CNF1 `0x00`, CNF2 `0x90`, CNF3 `0x02` → 8 tq → **500 kbit/s**, sample point 62.5 % |
| Receive filtering | `RXM = 11` on both buffers (filters off), `BUKT` enabled |
| UART | USART1, 115200 8N1 |

### Pin map

| Pin | Function |
|---|---|
| PA4 | MCP2515 CS (software-controlled) |
| PA5 | SPI1_SCK |
| PA6 | SPI1_MISO |
| PA7 | SPI1_MOSI |
| PA9 | USART1_TX → adapter RX |
| PA10 | USART1_RX → adapter TX |
| PC13 | LED, toggled per received frame |

## Expected output

```
=== f401 MCP2515 receiver node ===
MCP2515 ready - 500 kbit/s, normal mode, listening for 0x101/0x102
ACCEL  x=-12616  y= -9604  z=  4032
GYRO   x=  -477  y=   -95  z=    -4
```

Three line types are produced, each answering a different question:

| Line | Meaning |
|---|---|
| `ACCEL` / `GYRO` | A frame matching the contract, decoded |
| `RAW ID=... DLC=...` | A frame arrived but the ID or DLC was unexpected — a protocol mismatch |
| `waiting... CANSTAT=.. EFLG=.. TEC=.. REC=..` | No frame for one second; the bus state is dumped |

In the last case, `EFLG`, `TEC` and `REC` all reading zero means the bus is healthy and
the sender simply is not transmitting. A rising `REC` means frames are arriving but
failing — bit timing, termination or noise.

## Design notes

**3V3 supply and the 4 MHz SPI clock.** The MCP2515 is powered at 3V3 rather than 5 V
on purpose: at 5 V its logic-high threshold is `0.7 × VDD = 3.5 V`, which the F401's
3.3 V SPI outputs cannot reliably reach. The consequence is that below 4.5 V the
MCP2515's maximum SPI clock drops from 10 MHz to 5 MHz, so SPI1 runs at 4 MHz. Running
8 MHz here would be out of specification — and out-of-specification SPI usually means
"works, but drops a bit occasionally", which is far harder to diagnose than an outright
failure.

**One SPI transaction per frame, not forty.** Every register access goes through a
single multi-byte `HAL_SPI_TransmitReceive` inside one CS-low window. This matters
more than it sounds: the earlier byte-at-a-time driver spent **1.83 ms** reading one
frame, of which only **82 µs** was the SPI transfer — the other 95 % was per-call HAL
overhead on a 16 MHz core. Reception now uses `READ STATUS` (2 bytes) to find out
whether anything is waiting, then `READ RX BUFFER` (14 bytes) to take the whole frame
in one go. That instruction also clears the matching `RXnIF` flag as CS rises, so the
read and the acknowledgement are a single operation rather than ten.

**Reception is flag-driven.** `MCP2515_Receive` checks the `RX0IF`/`RX1IF` bits in
`CANINTF` before reading anything, and clears the flag afterwards with a bit-modify so
the other flags are untouched. Reading a buffer unconditionally would report stale
contents as a fresh frame; failing to clear the flag would re-read the same frame
forever and never free space for the next one.

**Rollover into RXB1 is required, not optional.** The sender emits the accel and gyro
frames roughly 300 µs apart, while this node is busy pushing a line out of the UART.
Without `BUKT` set in `RXB0CTRL`, the second frame is lost to an overflow and only
`ACCEL` lines appear.

**Measuring lost frames from the hardware, not by guessing.** The MCP2515 sets
`EFLG_RX0OVR`/`RX1OVR` when a frame arrives and both receive buffers are already full.
Those bits are the controller's own record of frames this node dropped, so
`MCP2515_ReadAndClearOverflow` reads and clears them and the loop reports once a
second:

```
[rx] 20.0 f/s accel=10 gyro=10 other=0 rxb0=20 rxb1=0 ovf0=0 ovf1=0
```

Frames are broken down by identifier and by the receive buffer they came from, and the
two overflow bits are counted separately. The breakdown exists because the total alone
hid something: under saturation one message type survived several times more often
than the other, which a single "frames per second" figure cannot show. Comparing the
total against the sender's `[tx]` line gives the loss; overflow counts are sticky-flag
detections rather than frames lost, since several drops between two checks count once.

`MCP2515_SERVICE_RXB1_FIRST` in `mcp2515.h` reverses the order in which the two buffers
are serviced, to test whether that order is what decides which message type survives. `LOG_EVERY_FRAME` at the top of `main.c` turns
the per-frame printing off so the same traffic can be run without it.

**Mode changes are polled, not assumed.** Entering normal mode does not complete until
the chip has seen 11 consecutive recessive bits on the bus, so `MCP2515_SetMode` polls
`CANSTAT` with a timeout rather than reading it once.

**Bring-up runs once.** It previously sat inside the main loop, which meant resetting
the chip on every pass — losing any frame arriving at that moment and leaving the bus
unacknowledged for ~10 ms at a time.

## Troubleshooting

| Symptom | Meaning |
|---|---|
| No UART output at all | UART problem, not CAN — the banner prints before any MCP2515 access |
| `MCP2515 init FAILED - CANSTAT=0x00` | The chip is not answering: SPI wiring, supply or crystal |
| `MCP2515 init FAILED - CANSTAT=0x80` | SPI works and the chip is alive, but it could not enter normal mode — the bus is stuck dominant, typically an unpowered transceiver or swapped CANH/CANL |
| `waiting... ... spi=ok` with all counters zero | The receiver is fine and the bus is quiet: the sender is unpowered, not transmitting, or not connected to the bus |
| `waiting... ... CNF2=0x00 spi=DEAD` | The SPI link to the MCP2515 failed after start-up — the zeros in the other fields are meaningless, check MISO, CS and the module's supply |

## Importing

Import the project file at the **project root** (`.project`). A second, stale skeleton
exists under `STM32CubeIDE/`; it predates the MCP2515 and debug modules and does not
reference them, so importing that one fails at link time.
