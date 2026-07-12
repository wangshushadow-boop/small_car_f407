#include "control_mux.h"

#include <stddef.h>

#include "gamepad.h"
#include "host_link.h"
#include "ultrasonic.h"

void ControlMux_Init(void)
{
  /* 当前仲裁层没有状态需要初始化，保留入口方便后续扩展。 */
}

bool ControlMux_SelectCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  /*
   * 安全保护放在最高优先级：
   * 一旦超声波判断前方距离过近，无论手柄或上位机是否有指令，都强制输出停车。
   */
  if (Ultrasonic_IsObstacleNear())
  {
    command->source = CONTROL_SOURCE_SAFETY;
    command->enabled = false;
    command->forward = 0;
    command->turn = 0;
    return true;
  }

  /* 手柄优先级高于上位机，方便现场调试时随时接管小车。 */
  if (Gamepad_GetControlCommand(command))
  {
    return true;
  }

  /* 上位机控制入口已保留，目前 HostLink 还没有实现运动协议。 */
  if (HostLink_GetControlCommand(command))
  {
    return true;
  }

  /* 没有任何有效控制源时输出空闲指令，底盘层会停车。 */
  command->source = CONTROL_SOURCE_NONE;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}
