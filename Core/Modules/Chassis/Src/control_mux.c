/**
 * @file control_mux.c
 * @brief 实现安全、USB 手柄和树莓派命令的固定优先级仲裁。
 */
#include "control_mux.h"

/*
 * 控制源仲裁模块。
 *
 * 手柄、树莓派和后续自动驾驶都可能产生底盘命令。
 * 这里统一选择最终生效的命令，并叠加前向超声避障限制。
 */

#include <stddef.h>

#include "gamepad.h"
#include "raspi_link.h"
#include "ultrasonic.h"

void ControlMux_Init(void)
{
  /* 当前仲裁层没有需要初始化的状态，保留入口方便后续扩展。 */
}

static void ControlMux_ApplyFrontObstacleLimit(ControlCommand *command)
{
  if ((command == NULL) || !command->enabled)
  {
    return;
  }

  /*
   * 超声雷达安装在车头，只能判断前方障碍。
   * 前方过近时只禁止继续向前，后退和原地转向仍然允许，
   * 这样小车可以从障碍前倒出来，也可以调整车头方向。
   */
  if (Ultrasonic_IsObstacleNear() && (command->forward > 0))
  {
    command->forward = 0;
  }
}

bool ControlMux_SelectCommand(ControlCommand *command)
{
  /* 调用方必须提供输出缓冲区；失败时不访问其他模块。 */
  if (command == NULL)
  {
    return false;
  }

  /* 手柄优先级高于树莓派，现场调试时可以随时接管。 */
  if (Gamepad_GetControlCommand(command))
  {
    ControlMux_ApplyFrontObstacleLimit(command);
    return true;
  }

  /* 树莓派通过 USART3 下发控制命令，超过 300ms 未更新会自动失效。 */
  if (RaspiLink_GetControlCommand(command))
  {
    ControlMux_ApplyFrontObstacleLimit(command);
    return true;
  }

  /* 没有有效控制源时输出空闲命令，底盘层会停车。 */
  command->source = CONTROL_SOURCE_NONE;
  command->value_type = CONTROL_VALUE_NORMALIZED;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}
