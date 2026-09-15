# f401_mcp2515_node

**Target:** STM32F401CCU6 · **Role:** receiver node, step 4 of 4

Receives the IMU frames published by [`f103_node_sensor`](../f103_node_sensor) from a
real 500 kbit/s CAN bus, using an external **MCP2515** controller over SPI, and reports
them over UART.

Because the node runs in normal mode rather than listen-only, it also **acknowledges**
frames on the bus. That matters: with no acknowledging node present, the sender cannot
count any frame as transmitted.

## What it does

The node runs on **FreeRTOS**. The bare-metal version it replaced is preserved at git
tag `v1-bare-metal`, and its measured limits are in the root README.

Bring-up still runs once in `main()`, before the scheduler starts: raise CS, reset,
confirm the chip answers, program bit timing, configure both receive buffers, enter
normal mode. From then on the work is split between two tasks:

```
MCP2515 INT --> PB0 / EXTI0 ISR --notify--> CanRxTask   (high priority)
                                              |  drain both buffers, count every frame
                                              |  xQueueSend(logQueue, frame, 0)
                                              v
                                            LogTask     (low priority)
                                               print frames, 1 s statistics report
```

- **CanRxTask** is the only code that talks to the MCP2515. It sleeps until the INT pin
  wakes it, reads until both receive buffers are empty, counts every frame, and hands
  a copy to the log queue without ever waiting on it.
- **LogTask** is the only code that touches the UART. It prints queued frames and,
  independently of the queue, a statistics report once a second.
- The **ISR** does no SPI and no printing; it only wakes CanRxTask.

Each received frame also toggles the PC13 LED, so the link can be seen to be alive
without a terminal attached.

## Modules

| File | Purpose |
|---|---|
| `Core/Src/freertos.c` | CanRxTask, LogTask, the INT pin ISR callback and the statistics |
| `Drivers/MCP2515/Src/mcp2515.c` | MCP2515 driver — SPI commands, modes, bit timing, reception, RX interrupt enable |
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
| UART | USART1, 115200 8N1, used only by LogTask |
| RTOS | FreeRTOS via CMSIS-RTOS2 (native API in application code), heap_4 16 KB, tick 1 kHz |
| Tasks | CanRxTask `osPriorityHigh` 512 words · LogTask `osPriorityLow` 1024 words |
| HAL timebase | TIM11 (SysTick belongs to FreeRTOS) |
| MCP2515 INT | PB0, EXTI0 falling edge with pull-up, NVIC priority 6 |

### Pin map

| Pin | Function |
|---|---|
| PA4 | MCP2515 CS (software-controlled) |
| PA5 | SPI1_SCK |
| PA6 | SPI1_MISO |
| PA7 | SPI1_MOSI |
| PA9 | USART1_TX → adapter RX |
| PA10 | USART1_RX → adapter TX |
| PB0 | MCP2515 INT (active low) |
| PC13 | LED, toggled per received frame |

## Expected output

```
=== f401 MCP2515 receiver node (FreeRTOS) ===
MCP2515 ready - 500 kbit/s, normal mode, listening for 0x101/0x102
ACCEL  x=  1200  y=    20  z= 17880
GYRO   x=   -20  y=   -28  z=   -34
[rx] 20.0 f/s accel=10 gyro=10 other=0 rxb0=20 rxb1=0 ovf0=0 ovf1=0
[os] logdrop=0 wakeups=20 heapmin=6120 stack_rx=310 stack_log=600
```

| Line | Meaning |
|---|---|
| `ACCEL` / `GYRO` | A frame matching the contract, decoded |
| `RAW ID=... DLC=...` | A frame arrived but the ID or DLC was unexpected — a protocol mismatch |
| `[rx] ... f/s ...` | Frames received in the last second, by identifier and by receive buffer, plus overflow detections |
| `[rx] idle CANSTAT=.. EFLG=.. TEC=.. REC=.. CNF2=.. spi=..` | No frame in the last second; the controller's state |
| `[os] logdrop=.. wakeups=.. heapmin=.. stack_rx=.. stack_log=..` | Health of the RTOS: frames received but not printed, INT wake-ups, lowest free heap ever (bytes), fewest stack words ever left unused per task |

`logdrop` above zero is not data loss: those frames were received and counted, only not
printed. Loss shows up as `ovf0`/`ovf1`. With an idle `[rx]` line, `EFLG`, `TEC` and
`REC` all reading zero mean the bus is healthy and the sender is not transmitting; a
rising `REC` means frames are arriving but failing.

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

**Reception is flag-driven.** `MCP2515_Receive` asks `READ STATUS` which buffers hold a
frame before reading anything, so stale buffer contents are never reported as a fresh
frame. The flag is cleared by the `READ RX BUFFER` instruction itself as CS rises; a
flag left set would re-read the same frame forever and never free room for the next.

**Rollover (`BUKT`) helps only while both buffers are being drained.** The intent was
that a frame arriving while RXB0 is full moves on to RXB1. Measured under saturation,
that is not what happens: whichever buffer is serviced first does all the work, and
the other ends up holding a single stale frame indefinitely, so rollover has nowhere to
go and every excess frame overflows. Rollover buys headroom for a brief burst, not
capacity. Sustained load needs the receive path to keep up, or a queue behind it.

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
are serviced, to test whether that order is what decides which message type survives.
`LOG_EVERY_FRAME` at the top of `freertos.c` turns per-frame printing off.

**Reception is interrupt-driven, and the interrupt is an edge on a level.** The MCP2515's
INT output stays low for as long as any receive flag is set; the MCU's pin interrupt
fires only on the falling edge. A frame that arrives while the previous one is being
read keeps INT low without producing a new edge. CanRxTask therefore reads until
`READ STATUS` reports both buffers empty, and before going back to sleep checks that
the INT pin has actually risen — otherwise it would wait for an interrupt that never
comes while the buffers fill. Interrupts are enabled by CanRxTask itself, after its
handle exists, so the first edge always has a task to wake.

**The receive path never waits for printing.** CanRxTask hands frames to the log queue
with a zero timeout. Blocking there would let the controller's two-frame buffer
overflow while the UART caught up — rebuilding the bare-metal design's loss inside an
RTOS. A full queue drops a log line and counts it. The queue exists to absorb bursts;
no queue length could absorb a sustained 2000 frames/s against a UART that prints
about 285 lines/s, and none is meant to.

**One owner per peripheral, so no mutexes.** Only CanRxTask uses SPI; only LogTask uses
the UART. Shared counters have a single writer and are never reset — LogTask keeps its
own previous copy and reports the difference, taking the copy in a critical section so
a higher-priority update cannot land halfway through it.

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
| Banner prints, then no `[rx]` lines at all | The scheduler did not start or LogTask never ran — heap too small for the tasks, or a kernel assert. Break in with the debugger |
| `!!! stack overflow in task ...` | That task's stack in CubeMX is too small; compare with its last `stack_...` figure |
| Frames stop, `[rx] idle` with `spi=ok` while the sender is running | Missed INT edge — check the PB0 wire and that the pin reads high when idle |
| `MCP2515 init FAILED - CANSTAT=0x00` | The chip is not answering: SPI wiring, supply or crystal |
| `MCP2515 init FAILED - CANSTAT=0x80` | SPI works and the chip is alive, but it could not enter normal mode — the bus is stuck dominant, typically an unpowered transceiver or swapped CANH/CANL |
| `[rx] idle ... spi=ok` with all counters zero | The receiver is fine and the bus is quiet: the sender is unpowered, not transmitting, or not connected to the bus |
| `[rx] idle ... CNF2=0x00 spi=DEAD` | The SPI link to the MCP2515 failed after start-up — the zeros in the other fields are meaningless, check MISO, CS and the module's supply |

## Importing

Import the project file at the **project root** (`.project`).

The `.ioc` is configured with `UnderRoot=false`, so when CubeMX regenerates code it
writes build settings into the sub-project under `STM32CubeIDE/`, **not** into the root
project that is actually built. The FreeRTOS include paths and the `Middlewares` source
folder were added to the root `.cproject` by hand. If a future CubeMX change adds another
middleware, the root `.cproject` has to be updated the same way; the symptom of
forgetting is a missing-header error such as `FreeRTOS.h: No such file or directory`.

The `STM32CubeIDE/` sub-project does not reference the MCP2515 driver or the debug
module, so importing that one fails at link time.
