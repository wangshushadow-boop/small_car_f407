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
#include "gamepad.h"
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
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

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
  /* USER CODE BEGIN StartDefaultTask */
  Oled_ShowBootScreen();
  DebugUart_WriteString("[RTOS] Default task started\r\n");
  osDelay(1000);

  uint32_t debug_counter = 0U;
  ControlSource last_source = CONTROL_SOURCE_NONE;

  /* Infinite loop */
  for(;;)
  {
    Gamepad_TaskStep();
    HostLink_TaskStep();
    Ultrasonic_TaskStep();

    ControlCommand command;
    (void)ControlMux_SelectCommand(&command);
    Chassis_ApplyCommand(&command);
    if (command.source != last_source)
    {
      DebugUart_Printf("[CTRL] source=%d enabled=%d\r\n", command.source, command.enabled ? 1 : 0);
      last_source = command.source;
    }

    if ((debug_counter % 5U) == 0U)
    {
      Icm20948Sample sample;
      Icm20948Status imu_status = Icm20948_ReadSample(&sample);
      if (imu_status == ICM20948_STATUS_OK)
      {
        DebugUart_Printf(
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
        DebugUart_Printf("[IMU] read failed, status=%d\r\n", imu_status);
      }
    }

    ++debug_counter;
    osDelay(100);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

