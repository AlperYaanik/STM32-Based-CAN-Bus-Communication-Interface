# f103_node_sensor

**Target:** STM32F103C8T6 · **Role:** sensor node (transmitter), step 3 of 4

Samples an **MPU6050** IMU over I²C and publishes it on a real 500 kbit/s CAN bus using
the F103's native bxCAN peripheral. Transmit-only: this node never reads from the bus.

The receiving end is [`f401_mcp2515_node`](../f401_mcp2515_node).

## What it does

At start-up the gyroscope zero-rate offset is measured over ~2.5 s with the board
held still, and subtracted from every later reading. Then, once per 100 ms cycle:

1. Read all six axes in a **single 14-byte I²C burst** starting at `ACCEL_XOUT_H`.
   One transaction rather than three means the accelerometer and gyroscope samples
   belong to the same instant.
2. Pack each axis as big-endian `int16` via `CAN_PackAxes` from the shared contract
   header.
3. Transmit two frames: `0x101` (accel) and `0x102` (gyro), DLC 6 each.
4. Log the values over UART.

## Modules

| File | Purpose |
|---|---|
| `Core/Src/mpu6050.c` | IMU driver: wake, identity check, burst read, gyro bias calibration |
| `Core/Src/debug.c` | `LOG(...)` over USART1, compiled out when `DEBUG_ENABLED` is 0 |
| `Core/Inc/can_protocol.h` | Frame format shared with the receiver — **must stay byte-identical in both projects** |

`MPU6050_Init` clears the `SLEEP` bit in `PWR_MGMT_1` — without it every register read
returns zeros — then verifies `WHO_AM_I`. Both `0x68` (MPU6050) and `0x70` (MPU6500) are
accepted, because most breakouts sold as GY-521 now carry the MPU6500, which has the
same register map.

## Configuration

| Item | Value |
|---|---|
| System clock | HSI 8 MHz → /2 → PLL ×16 → **64 MHz**, APB1 32 MHz |
| CAN bit timing | Prescaler 4, BS1 = 13 tq, BS2 = 2 tq → 16 tq → **500 kbit/s**, sample point 87.5 % |
| CAN mode | Normal, `AutoRetransmission` **disabled** (one-shot) |
| I²C | I²C1 at 400 kHz, 100 ms transaction timeout |
| Sample rate | 10 Hz |
| UART | USART1, 115200 8N1 |

### Pin map

| Pin | Function |
|---|---|
| PB6 | I²C1_SCL → MPU6050 SCL |
| PB7 | I²C1_SDA → MPU6050 SDA |
| PA11 | CAN_RX → transceiver RXD |
| PA12 | CAN_TX → transceiver TXD |
| PA9 | USART1_TX → adapter RX |
| PA10 | USART1_RX → adapter TX |

The MPU6050's `AD0` pin must be tied low, giving 7-bit address `0x68`.

## Expected output

```
MPU6050 OK
calibrating gyro - keep the board still...
  movement (peak-to-peak) = 86,113,90 LSB
  gyro bias = -486,-144,-27 LSB
accel=-12616,-9604,4032 gyro=9,-2,-1
accel=-12620,-9576,4116 gyro=-7,4,3
```

At rest the accelerometer vector magnitude should come out near 16384 LSB, which is 1 g
on the ±2 g scale. If it does not, the packing or the I²C read is wrong.

## Design notes

**One-shot transmission.** `AutoRetransmission` is disabled. If it were enabled, a
frame that goes unacknowledged — because no second node is powered — would be retried
forever, filling all three mailboxes and eventually driving the controller
error-passive and then bus-off. Here the frame is discarded instead and the node keeps
sampling. The cost is that an unacknowledged frame is lost silently.

**Finite I²C timeouts.** All `HAL_I2C_Mem_*` calls use a 100 ms timeout rather than
`HAL_MAX_DELAY`. With an infinite timeout, a disconnected sensor blocks forever and the
`read FAILED` log branch can never execute — the board simply appears dead, which is
the least useful failure mode possible.

**Gyro bias calibration, and why it can refuse.** At rest an uncalibrated MPU6050
gyro here reads about -3.7 deg/s on X. Integrated into an angle that is 218 deg of
drift per minute, so the offset has to go. `MPU6050_CalibrateGyro` averages 512
samples spaced 5 ms apart and stores the result. The catch is that whatever the
sensor reports while it runs *becomes the definition of "not moving"* - calibrating
during movement bakes that movement in permanently. The routine therefore tracks the
peak-to-peak spread of every axis and rejects the measurement if any of them moved
too far, leaving the bias at zero. An uncalibrated sensor is honest; a wrongly
calibrated one is not. A failure is logged but does not halt the node.

The peak-to-peak movement is printed on every attempt, successful or not, which
makes it a **mechanical quality meter for the setup**. Calibration is a mechanical
problem before it is a software one: a board held down by a finger, or pulled
around by the spring force of its own jumper wires, cannot be calibrated well no
matter what the firmware does. Mount the breakout to something rigid and tape the
wires down a few centimetres away so their stiffness acts on the tape rather than
on the board, then watch this number fall.

**Wake-up settle delay.** After clearing `SLEEP`, the driver waits 100 ms before
trusting readings, per the datasheet's start-up recommendation.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| No UART output at all | BOOT0 jumper left high, TX/RX not crossed, or no common ground |
| `MPU6050 init FAILED` | Wiring, or `AD0` pulled high making the address `0x69` |
| `gyro calibration FAILED` | The board was disturbed during the measurement, or an I2C read failed. Data still streams, uncorrected |
| `CAN TX timeout` repeatedly | No second node acknowledging — check that the receiver is powered and the bus is terminated |
