# 小车控制框架说明

本文档描述当前 MCU 工程的软件框架。当前目标是：MCU 负责传感器采集、手柄控制、基础安全保护和执行机构输出；上位机后续可通过通信接口下发更复杂的控制指令。

## 控制优先级

当前底盘运动指令按以下优先级仲裁：

| 优先级 | 控制源 | 当前行为 |
| --- | --- | --- |
| 1 | 安全保护 | 超声波检测到近距离障碍时，强制停车。 |
| 2 | USB 手柄 | 手柄在线时，左摇杆控制四轮电机。 |
| 3 | 上位机 | 当前仅保留控制入口，尚未接入运动协议。 |
| 4 | 空闲 | 没有有效输入时，底盘停车。 |

舵机目前不经过底盘仲裁：USB 手柄右摇杆以增量方式控制两路舵机，摇杆回中后舵机保持当前位置。

## 模块职责

| 模块 | 文件 | 当前职责 |
| --- | --- | --- |
| 通用控制类型 | `Core/Inc/control_types.h` | 定义控制源和运动指令结构。 |
| 控制仲裁 | `Core/Inc/control_mux.h`、`Core/Src/control_mux.c` | 按安全、手柄、上位机、空闲顺序选择底盘指令。 |
| USB 手柄 | `Core/Inc/gamepad.h`、`Core/Src/gamepad.c`、`Core/Src/gamepad_usb.c` | 接收 USB 手柄数据，左摇杆输出底盘控制量。 |
| 手柄舵机控制 | `Core/Inc/gamepad_servo.h`、`Core/Src/gamepad_servo.c` | 右摇杆增量控制两路舵机。 |
| 串口1调试命令 | `Core/Inc/host_link.h`、`Core/Src/host_link.c` | 处理串口1文本命令和日志开关。 |
| 树莓派串口3协议 | `Core/Inc/raspi_link.h`、`Core/Src/raspi_link.c` | 处理串口3二进制协议、状态上报和上位机控制命令。 |
| 串口调试 | `Core/Inc/debug_uart.h`、`Core/Src/debug_uart.c` | 提供调试输出和模块日志开关。 |
| 底盘执行 | `Core/Inc/chassis.h`、`Core/Src/chassis.c` | 将前进/转向指令混控为四路电机速度。 |
| 电机驱动 | `Core/Inc/motor.h`、`Core/Src/motor.c` | 将速度转换为 PWM 占空比和方向输出。 |
| 舵机驱动 | `Core/Inc/servo.h`、`Core/Src/servo.c` | 输出两路 PWM 舵机信号。 |
| 编码器采集 | `Core/Inc/encoder.h`、`Core/Src/encoder.c` | 采集四路电机编码器累计计数和周期增量。 |
| IMU | `Core/Inc/icm20948.h`、`Core/Src/icm20948.c` | 读取 ICM20948 加速度、角速度和温度。 |
| 超声波 | `Core/Inc/ultrasonic.h`、`Core/Src/ultrasonic.c` | 触发测距、计算距离，并提供近距离障碍判断。 |
| OLED | `Core/Inc/oled.h`、`Core/Src/oled.c` | 初始化 OLED 并显示启动信息。 |

## 运行结构

| 运行单元 | 当前职责 |
| --- | --- |
| USB Host 线程 | 由 USB Host 中间件创建，负责 USB 枚举和手柄数据接收。 |
| `defaultTask` | 周期调用手柄、舵机、串口命令、超声波、编码器和底盘控制逻辑。 |
| 中断回调 | 串口接收完成回调处理命令字节；超声波 EXTI 回调只记录边沿时间，不在中断里打印。 |

当前使用单个应用任务已经可以满足调试阶段需求。后续如果加入闭环速度控制、姿态融合或更高频控制环，再拆分独立任务会更合适。

## 数据流

```text
USB 手柄 / 上位机命令 / 超声波安全
              |
              v
          控制仲裁
              |
              v
        底盘混控与限幅
              |
              v
      四路电机 PWM + 方向输出
```

```text
USB 手柄右摇杆
      |
      v
两路舵机增量控制
      |
      v
舵机 PWM 输出
```

## 后续扩展建议

| 扩展项 | 建议 |
| --- | --- |
| 上位机控制 | 已由 `raspi_link.c` 实现串口3二进制协议，并通过 `RaspiLink_GetControlCommand()` 接入控制仲裁。 |
| 闭环速度控制 | 基于 `encoder.c` 的周期增量增加 PID 层，不建议直接塞进 `motor.c`。 |
| 多传感器融合 | IMU 原始数据保留在驱动层，姿态解算单独新增模块。 |
| 任务拆分 | 只有在控制周期、阻塞风险或调试复杂度上升后再拆分任务。 |

## 速度约定

| 项目 | 约定 |
| --- | --- |
| 电机速度范围 | `-1000` 到 `1000` |
| 正值 | 正转 |
| 负值 | 反转 |
| `0` | 停止 |
