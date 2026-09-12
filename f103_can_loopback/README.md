# f103_can_loopback

**Target:** STM32F103C8T6 · **Role:** bxCAN bring-up, step 1 of 4

The first project in the series. It exercises the STM32F103's native CAN controller
(bxCAN) in **internal loopback mode**, where the peripheral routes its own transmitted
frames back into its receive FIFO without ever driving the CAN pins.

This deliberately needs **no transceiver, no bus and no second node** — the point is to
get the peripheral configuration right while the hardware variables are still removed
from the picture. Every mistake found here is guaranteed to be a firmware mistake.

## What it does

Each cycle the firmware transmits an 8-byte frame, waits for it to come back through
the acceptance filter into FIFO 0, reads it out and prints the payload over UART. The
first data byte is incremented every pass, so a stalled or repeated frame is visible
immediately rather than looking like success.

Both the transmit and receive waits are bounded by a 100 ms timeout, so a
misconfiguration halts in `Error_Handler` instead of hanging silently.

## Configuration

| Item | Value |
|---|---|
| System clock | HSI 8 MHz, no PLL |
| CAN mode | `CAN_MODE_LOOPBACK` |
| Bit timing | Prescaler 1, BS1 = 13 tq, BS2 = 2 tq → 16 tq → **500 kbit/s** |
| Acceptance filter | Bank 0, ID-mask mode, 32-bit scale, mask 0 (accept all), FIFO 0 |
| UART | USART1, 115200 8N1 |

### Pin map

| Pin | Function |
|---|---|
| PA11 | CAN_RX (not driven externally in loopback) |
| PA12 | CAN_TX (not driven externally in loopback) |
| PA9 | USART1_TX |
| PA10 | USART1_RX |
| PC13 | LED |

## Expected output

```
Received Data: 1 2 3 4 5 6 7 8
Received Data: 2 2 3 4 5 6 7 8
Received Data: 3 2 3 4 5 6 7 8
```

One line every two seconds, with the first byte counting up.

## What carries forward

The filter configuration, the mailbox-free polling pattern and the 500 kbit/s bit
timing from this project are reused unchanged in
[`f103_node_sensor`](../f103_node_sensor), which is the same peripheral driving a real
bus.
