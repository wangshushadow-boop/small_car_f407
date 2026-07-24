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
#include "oled.h"
#include "raspi_link.h"
#include "servo.h"
#include "system_status.h"
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
    /* 浠诲姟鍒涘缓澶辫触閫氬父鏄爢鎴栨爤閰嶇疆闂銆?*/
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
  /* FreeRTOS 鍚姩鍚庢樉绀?OLED 鍚姩鐢婚潰锛岀‘璁や换鍔″凡缁忓紑濮嬭繍琛屻€?*/
  Oled_ShowBootScreen();
  DebugUart_WriteStringIf(DEBUG_LOG_RTOS, "[RTOS] Default task started\r\n");
  osDelay(1000);

  ControlSource last_source = CONTROL_SOURCE_NONE;
  SystemStatus_Init();

  /* Infinite loop */
  for(;;)
  {
    /*
     * 褰撳墠璋冭瘯闃舵浣跨敤涓€涓簲鐢ㄤ换鍔￠『搴忚皟搴﹀悇妯″潡锛?
     * 姣忎釜 TaskStep 閮藉簲灏介噺鐭皬銆侀潪闃诲锛岄伩鍏嶅奖鍝?100ms 涓诲惊鐜妭濂忋€?
     */
    Gamepad_TaskStep();
    GamepadServo_TaskStep();
    HostLink_TaskStep();
    RaspiLink_TaskStep();
    Ultrasonic_TaskStep();
    Encoder_TaskStep();

    ControlCommand command;
    /* 鎺у埗浠茶鍐冲畾搴曠洏鏈€缁堝惉璋佺殑鍛戒护銆?*/
    (void)ControlMux_SelectCommand(&command);
    /* 搴曠洏灞傚彧鍏冲績鏈€缁堝懡浠わ紝涓嶉渶瑕佺煡閬撳懡浠ゆ潵鑷墜鏌勮繕鏄笂浣嶆満銆?*/
    Chassis_ApplyCommand(&command);
    /* ROS 物理速度命令执行编码器闭环；手柄仍沿用原有开环路径。 */
    Chassis_TaskStep(20U);
    SystemStatus_TaskStep(&command);
    if (command.source != last_source)
    {
      /* 鎺у埗婧愬彉鍖栨椂鎵撳嵃涓€娆★紝渚夸簬鍒ゆ柇鏄惁琚畨鍏ㄤ繚鎶ゆ垨鎵嬫焺鎺ョ銆?*/
      DebugUart_PrintfIf(DEBUG_LOG_CONTROL,
                         "[CTRL] source=%d enabled=%d\r\n",
                         command.source,
                         command.enabled ? 1 : 0);
      last_source = command.source;
    }

    /* 涓诲惊鐜懆鏈熴€傚悗缁仛闂幆鎺у埗鏃讹紝鍙互鏍规嵁闇€瑕佺缉鐭懆鏈熸垨鎷嗗垎浠诲姟銆?*/
    osDelay(20);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

