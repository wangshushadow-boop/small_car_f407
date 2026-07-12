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
#include "chassis.h"
#include "control_mux.h"
#include "debug_uart.h"
#include "encoder.h"
#include "gamepad.h"
#include "gamepad_servo.h"
#include "host_link.h"
#include "icm20948.h"
#include "oled.h"
#include "servo.h"
#include "ultrasonic.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_HOST_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  if (defaultTaskHandle == NULL)
  {
    /* 任务创建失败通常是堆或栈配置问题。 */
    DebugUart_WriteStringIf(DEBUG_LOG_RTOS, "[RTOS] defaultTask create failed\r\n");
  }
  else
  {
    DebugUart_WriteStringIf(DEBUG_LOG_RTOS, "[RTOS] defaultTask created\r\n");
  }
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN StartDefaultTask */
  /* FreeRTOS 启动后显示 OLED 启动画面，确认任务已经开始运行。 */
  Oled_ShowBootScreen();
  DebugUart_WriteStringIf(DEBUG_LOG_RTOS, "[RTOS] Default task started\r\n");
  osDelay(1000);

  uint32_t debug_counter = 0U;
  ControlSource last_source = CONTROL_SOURCE_NONE;

  /* Infinite loop */
  for(;;)
  {
    /*
     * 当前调试阶段使用一个应用任务顺序调度各模块：
     * 每个 TaskStep 都应尽量短小、非阻塞，避免影响 100ms 主循环节奏。
     */
    Gamepad_TaskStep();
    GamepadServo_TaskStep();
    HostLink_TaskStep();
    Ultrasonic_TaskStep();
    Encoder_TaskStep();

    ControlCommand command;
    /* 控制仲裁决定底盘最终听谁的命令。 */
    (void)ControlMux_SelectCommand(&command);
    /* 底盘层只关心最终命令，不需要知道命令来自手柄还是上位机。 */
    Chassis_ApplyCommand(&command);
    if (command.source != last_source)
    {
      /* 控制源变化时打印一次，便于判断是否被安全保护或手柄接管。 */
      DebugUart_PrintfIf(DEBUG_LOG_CONTROL,
                         "[CTRL] source=%d enabled=%d\r\n",
                         command.source,
                         command.enabled ? 1 : 0);
      last_source = command.source;
    }

    if ((debug_counter % 5U) == 0U)
    {
      /* 主循环 100ms 一次，这里每 5 次打印一次，也就是约 500ms。 */
      Icm20948Sample sample;
      Icm20948Status imu_status = Icm20948_ReadSample(&sample);
      if (imu_status == ICM20948_STATUS_OK)
      {
        DebugUart_PrintfIf(
            DEBUG_LOG_IMU,
            "[IMU] ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp=%d\r\n",
            sample.accel_x,
            sample.accel_y,
            sample.accel_z,
            sample.gyro_x,
            sample.gyro_y,
            sample.gyro_z,
            sample.temperature);
      }
      else
      {
        DebugUart_PrintfIf(DEBUG_LOG_IMU, "[IMU] read failed, status=%d\r\n", imu_status);
      }

      EncoderSample encoder_a = Encoder_GetSample(MOTOR_A);
      EncoderSample encoder_b = Encoder_GetSample(MOTOR_B);
      EncoderSample encoder_c = Encoder_GetSample(MOTOR_C);
      EncoderSample encoder_d = Encoder_GetSample(MOTOR_D);
      DebugUart_PrintfIf(
          DEBUG_LOG_ENCODER,
          "[ENC] A=%ld/%d B=%ld/%d C=%ld/%d D=%ld/%d\r\n",
          encoder_a.count,
          encoder_a.delta,
          encoder_b.count,
          encoder_b.delta,
          encoder_c.count,
          encoder_c.delta,
          encoder_d.count,
          encoder_d.delta);
    }

    ++debug_counter;
    /* 主循环周期。后续做闭环控制时，可以根据需要缩短周期或拆分任务。 */
    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

