#include "control_mux.h"

#include <stddef.h>

#include "gamepad.h"
#include "raspi_link.h"
#include "ultrasonic.h"

void ControlMux_Init(void)
{
  /* 当前仲裁层没有需要初始化的状态，保留入口方便后续扩展。 */
}

bool ControlMux_SelectCommand(ControlCommand *command)
{
  if (command == NULL)
  {
    return false;
  }

  /*
   * 安全保护优先级最高。
   * 一旦超声判断前方过近，无论手柄或树莓派是否有命令，都强制停车。
   */
  if (Ultrasonic_IsObstacleNear())
  {
    command->source = CONTROL_SOURCE_SAFETY;
    command->enabled = false;
    command->forward = 0;
    command->turn = 0;
    return true;
  }

  /* 手柄优先级高于树莓派，现场调试时可以随时接管。 */
  if (Gamepad_GetControlCommand(command))
  {
    return true;
  }

  /* 树莓派通过 USART3 下发控制命令，超过 300ms 未更新会自动失效。 */
  if (RaspiLink_GetControlCommand(command))
  {
    return true;
  }

  /* 没有有效控制源时输出空闲命令，底盘层会停车。 */
  command->source = CONTROL_SOURCE_NONE;
  command->enabled = false;
  command->forward = 0;
  command->turn = 0;
  return false;
}
