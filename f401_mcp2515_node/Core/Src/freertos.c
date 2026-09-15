/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <string.h>
#include "queue.h"
#include "usart.h"
#include "mcp2515.h"
#include "can_protocol.h"
#include "debug.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* Running totals since start-up. CanRxTask is the only writer and never resets
   them; LogTask keeps its own previous copy and reports the difference. With a
   single writer and no resets there is nothing for the two tasks to race over
   except the copy itself, which is taken in a critical section. */
typedef struct
{
  uint32_t frames;
  uint32_t accel;
  uint32_t gyro;
  uint32_t other;
  uint32_t fromRxb0;
  uint32_t fromRxb1;
  uint32_t overflowRxb0;   /* sticky-flag detections, not frames lost */
  uint32_t overflowRxb1;
  uint32_t logDropped;     /* frames received fine but not printed */
  uint32_t wakeups;        /* times CanRxTask was woken by the INT pin */
} RxCounters_t;

/* Controller status, captured by CanRxTask - the only task allowed to talk to
   the MCP2515 - after a second with no frames, and printed by LogTask. */
typedef struct
{
  uint8_t canstat;
  uint8_t eflg;
  uint8_t tec;
  uint8_t rec;
  uint8_t cnf2;
  bool valid;
} BusStatus_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Print every received frame. This no longer slows reception: printing happens
   in LogTask, and CanRxTask only hands frames over through a queue it never
   waits on. When the UART cannot keep up, frames that do not fit in the queue
   are still received and counted - they are just not printed, and logDropped
   says how many. Set to 0 to skip the queue entirely. */
#define LOG_EVERY_FRAME      1

#define STATS_PERIOD_MS      1000u

/* The queue absorbs bursts, not a sustained rate difference: at ~2000
   frames/s against a UART that prints ~285 lines/s, no length would be enough.
   32 covers the ~7 frames that arrive while a single line is being printed. */
#define LOG_QUEUE_LENGTH     32u

/* How long CanRxTask sleeps without an INT edge before it checks the
   controller's status registers, so a silent bus is still diagnosable. */
#define RX_IDLE_TIMEOUT_MS   1000u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static QueueHandle_t logQueue;

/* Native FreeRTOS handles, taken by each task for itself. The ISR reads
   canRxTask, so it is volatile; it stays NULL until the task exists, and the
   ISR refuses to notify before then. */
static TaskHandle_t volatile canRxTask = NULL;
static TaskHandle_t logTask = NULL;

static RxCounters_t rxCounters;   /* written only by CanRxTask */
static BusStatus_t busStatus;     /* written only by CanRxTask */

/* USER CODE END Variables */
/* Definitions for LogTask */
osThreadId_t LogTaskHandle;
const osThreadAttr_t LogTask_attributes = {
  .name = "LogTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for CanRxTask */
osThreadId_t CanRxTaskHandle;
const osThreadAttr_t CanRxTask_attributes = {
  .name = "CanRxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void DrainReceiveBuffers(void);
static void CaptureBusStatus(void);
static void PrintReport(uint32_t elapsedMs, const RxCounters_t *now,
                        const RxCounters_t *before);
#if LOG_EVERY_FRAME
static void PrintFrame(const MCP2515_Frame_t *frame);
#endif

/* USER CODE END FunctionPrototypes */

void StartLogTask(void *argument);
void StartCanRxTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  (void)xTask;

  /* Called from the context switch, after the kernel has seen a task write
     past the end of its stack. Memory is already corrupt, so nothing elaborate
     can be trusted here - least of all LOG, which formats on a stack. Name the
     task straight to the UART as a best effort (if LogTask was mid-transmit
     the HAL reports busy and this prints nothing), then stop for good. */
  static const char prefix[] = "\r\n!!! stack overflow in task ";

  HAL_UART_Transmit(&huart1, (uint8_t *)prefix, (uint16_t)(sizeof(prefix) - 1u), 50u);
  HAL_UART_Transmit(&huart1, (uint8_t *)pcTaskName,
                    (uint16_t)strlen((const char *)pcTaskName), 50u);

  Error_Handler();
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* No mutexes: each peripheral has exactly one owning task. */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* Created here rather than in CubeMX because the element type belongs to
     the MCP2515 driver. Frames are copied in by value, 12 bytes each. */
  logQueue = xQueueCreate(LOG_QUEUE_LENGTH, sizeof(MCP2515_Frame_t));
  if (logQueue == NULL)
  {
    Error_Handler();   /* heap too small */
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of LogTask */
  LogTaskHandle = osThreadNew(StartLogTask, NULL, &LogTask_attributes);

  /* creation of CanRxTask */
  CanRxTaskHandle = osThreadNew(StartCanRxTask, NULL, &CanRxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* osThreadNew returns NULL when the heap cannot hold the task's stack. That
     failure is otherwise silent - the task simply never runs. */
  if ((LogTaskHandle == NULL) || (CanRxTaskHandle == NULL))
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartLogTask */
/**
  * @brief  Sole owner of the UART. Prints frames handed over by CanRxTask and
  *         a statistics report once per STATS_PERIOD_MS.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartLogTask */
void StartLogTask(void *argument)
{
  /* USER CODE BEGIN StartLogTask */
  (void)argument;
  logTask = xTaskGetCurrentTaskHandle();

  const TickType_t reportPeriod = pdMS_TO_TICKS(STATS_PERIOD_MS);
  TickType_t lastReport = xTaskGetTickCount();
  RxCounters_t before;

  taskENTER_CRITICAL();
  before = rxCounters;
  taskEXIT_CRITICAL();

  for (;;)
  {
    /* Wait for a frame, but never past the next report deadline. This is how
       one task serves both jobs: even if the queue stays full and printing
       falls behind, the report still comes out once a second. */
    TickType_t sinceReport = xTaskGetTickCount() - lastReport;
    TickType_t wait = (sinceReport < reportPeriod) ? (reportPeriod - sinceReport) : 0;

    MCP2515_Frame_t frame;
    if (xQueueReceive(logQueue, &frame, wait) == pdPASS)
    {
#if LOG_EVERY_FRAME
      PrintFrame(&frame);
#endif
    }

    TickType_t now = xTaskGetTickCount();
    if ((now - lastReport) >= reportPeriod)
    {
      RxCounters_t current;

      /* CanRxTask has higher priority and could preempt this copy halfway,
         leaving some fields from before an update and some from after.
         A critical section makes the copy a single instant. It holds off
         interrupts for the duration of a 40-byte copy - microseconds. */
      taskENTER_CRITICAL();
      current = rxCounters;
      taskEXIT_CRITICAL();

      PrintReport((uint32_t)((now - lastReport) * portTICK_PERIOD_MS), &current, &before);

      before = current;
      lastReport = now;
    }
  }
  /* USER CODE END StartLogTask */
}

/* USER CODE BEGIN Header_StartCanRxTask */
/**
* @brief Sole owner of the MCP2515. Sleeps until the INT pin wakes it, then
*        drains both receive buffers and hands frames to LogTask.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCanRxTask */
void StartCanRxTask(void *argument)
{
  /* USER CODE BEGIN StartCanRxTask */
  (void)argument;

  /* Order matters. The handle must exist before the controller may raise INT,
     otherwise the first edge reaches an ISR that has no task to wake. */
  canRxTask = xTaskGetCurrentTaskHandle();
  MCP2515_EnableRxInterrupts();

  for (;;)
  {
    /* Drain first, sleep second - and keep draining while INT is still low.
       INT stays low as long as any frame is waiting, but the pin interrupt
       fires only on a falling edge. A frame that lands while the previous one
       is being read keeps INT low with no new edge; sleeping at that point
       would wait for an interrupt that never comes while the buffers fill.
       Checking the pin level closes that gap. Frames that arrived before
       interrupts were enabled are picked up by the same first pass. */
    do
    {
      DrainReceiveBuffers();
    } while (HAL_GPIO_ReadPin(MCP2515_INT_GPIO_Port, MCP2515_INT_Pin) == GPIO_PIN_RESET);

    /* Notifications latch: an edge between the pin check above and this call
       is not lost, the call simply returns at once. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(RX_IDLE_TIMEOUT_MS)) == 0u)
    {
      CaptureBusStatus();
    }
    else
    {
      rxCounters.wakeups++;
    }
  }
  /* USER CODE END StartCanRxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* EXTI0 runs at NVIC priority 6, which is what makes calling a FromISR API
   legal here (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY is 5). The ISR does
   no SPI and no printing - it only wakes the task that does. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if ((GPIO_Pin == MCP2515_INT_Pin) && (canRxTask != NULL))
  {
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    vTaskNotifyGiveFromISR(canRxTask, &higherPriorityTaskWoken);

    /* If the woken task outranks whatever was interrupted - it always does
       unless CanRxTask itself was running - switch to it on ISR exit rather
       than at the next tick, up to a millisecond later. */
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
  }
}

static void DrainReceiveBuffers(void)
{
  MCP2515_Frame_t frame = {0};

  while (MCP2515_Receive(&frame))
  {
    rxCounters.frames++;

    if (frame.id == CAN_ID_ACCEL)
    {
      rxCounters.accel++;
    }
    else if (frame.id == CAN_ID_GYRO)
    {
      rxCounters.gyro++;
    }
    else
    {
      rxCounters.other++;
    }

    if (frame.buffer == 0u)
    {
      rxCounters.fromRxb0++;
    }
    else
    {
      rxCounters.fromRxb1++;
    }

    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

#if LOG_EVERY_FRAME
    /* Timeout 0: never wait for LogTask. If this task blocked here the
       controller's two-frame buffer would overflow while the UART caught up,
       rebuilding exactly the loss the bare-metal loop had. Better to drop a
       log line and count it. */
    if (xQueueSend(logQueue, &frame, 0) != pdPASS)
    {
      rxCounters.logDropped++;
    }
#endif

    /* Only dlc bytes of data[] are written per frame; clear the rest so a RAW
       dump never shows bytes left over from the previous frame. */
    memset(&frame, 0, sizeof(frame));
  }

  uint8_t overflow = MCP2515_ReadAndClearOverflow();
  if ((overflow & EFLG_RX0OVR) != 0u)
  {
    rxCounters.overflowRxb0++;
  }
  if ((overflow & EFLG_RX1OVR) != 0u)
  {
    rxCounters.overflowRxb1++;
  }
}

static void CaptureBusStatus(void)
{
  BusStatus_t status;

  status.canstat = MCP2515_Read(MCP_CANSTAT);
  status.eflg    = MCP2515_Read(MCP_EFLG);
  status.tec     = MCP2515_Read(MCP_TEC);
  status.rec     = MCP2515_Read(MCP_REC);
  /* A known non-zero value written at bring-up; reading it back tells a quiet
     bus (all the zeros above are real) from a dead SPI link (they are not). */
  status.cnf2    = MCP2515_Read(MCP_CNF2);
  status.valid   = true;

  taskENTER_CRITICAL();
  busStatus = status;
  taskEXIT_CRITICAL();
}

static void PrintReport(uint32_t elapsedMs, const RxCounters_t *now,
                        const RxCounters_t *before)
{
  uint32_t frames = now->frames - before->frames;
  uint32_t frameTenths = (elapsedMs > 0u) ? (frames * 10000u) / elapsedMs : 0u;

  if (frames == 0u)
  {
    BusStatus_t status;

    taskENTER_CRITICAL();
    status = busStatus;
    taskEXIT_CRITICAL();

    if (status.valid)
    {
      bool spiAlive = (status.cnf2 != 0x00u) && (status.cnf2 != 0xFFu);

      LOG("[rx] idle CANSTAT=0x%02X EFLG=0x%02X TEC=%u REC=%u CNF2=0x%02X spi=%s\r\n",
          (unsigned int)status.canstat, (unsigned int)status.eflg,
          (unsigned int)status.tec, (unsigned int)status.rec,
          (unsigned int)status.cnf2, spiAlive ? "ok" : "DEAD");
    }
    else
    {
      LOG("[rx] idle\r\n");
    }
  }
  else
  {
    LOG("[rx] %u.%u f/s accel=%u gyro=%u other=%u rxb0=%u rxb1=%u ovf0=%u ovf1=%u\r\n",
        (unsigned int)(frameTenths / 10u), (unsigned int)(frameTenths % 10u),
        (unsigned int)(now->accel - before->accel),
        (unsigned int)(now->gyro - before->gyro),
        (unsigned int)(now->other - before->other),
        (unsigned int)(now->fromRxb0 - before->fromRxb0),
        (unsigned int)(now->fromRxb1 - before->fromRxb1),
        (unsigned int)(now->overflowRxb0 - before->overflowRxb0),
        (unsigned int)(now->overflowRxb1 - before->overflowRxb1));
  }

  /* Health of the RTOS itself. heapmin is the lowest free heap ever seen, in
     bytes; the stack figures are the fewest words ever left unused. These are
     what turn the stack and heap sizes chosen in CubeMX from estimates into
     measurements. */
  LOG("[os] logdrop=%u wakeups=%u heapmin=%u stack_rx=%u stack_log=%u\r\n",
      (unsigned int)(now->logDropped - before->logDropped),
      (unsigned int)(now->wakeups - before->wakeups),
      (unsigned int)xPortGetMinimumEverFreeHeapSize(),
      (unsigned int)((canRxTask != NULL) ? uxTaskGetStackHighWaterMark(canRxTask) : 0u),
      (unsigned int)((logTask != NULL) ? uxTaskGetStackHighWaterMark(logTask) : 0u));
}

#if LOG_EVERY_FRAME
static void PrintFrame(const MCP2515_Frame_t *frame)
{
  int16_t axes[3];

  /* Does the frame match the contract? A known ID carrying an unexpected DLC
     means the sender changed; print that distinguishably instead of silently
     decoding it wrong. */
  if (frame->dlc == CAN_AXES_DLC &&
      (frame->id == CAN_ID_ACCEL || frame->id == CAN_ID_GYRO))
  {
    CAN_UnpackAxes(frame->data, axes);

    LOG("%s  x=%6d  y=%6d  z=%6d\r\n",
        (frame->id == CAN_ID_ACCEL) ? "ACCEL" : "GYRO ",
        axes[0], axes[1], axes[2]);
  }
  else
  {
    LOG("RAW   ID=0x%03X DLC=%u  %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
        (unsigned int)frame->id, (unsigned int)frame->dlc,
        frame->data[0], frame->data[1], frame->data[2], frame->data[3],
        frame->data[4], frame->data[5], frame->data[6], frame->data[7]);
  }
}
#endif

/* USER CODE END Application */

