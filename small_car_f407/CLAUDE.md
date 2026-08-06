# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```powershell
# Debug 构建
cmake --preset Debug
cmake --build --preset Debug

# Release 构建
cmake --preset Release
cmake --build --preset Release
```

构建产物输出到 `build/` 目录，不提交到 Git。

## 技术栈

- **MCU**: STM32F407VET6 (Cortex-M4, 168MHz)
- **工具链**: ARM GCC (`arm-none-eabi-gcc`)
- **构建系统**: CMake 3.22+ with Ninja
- **RTOS**: FreeRTOS
- **USB**: USB Host HID (支持游戏手柄)
- **HAL**: STM32CubeF4 HAL Driver

## 项目结构

```
small_car_f407/
├── Core/
│   ├── Src/              # CubeMX 生成代码 + 应用入口 main.c
│   ├── Inc/              # CubeMX 生成的头文件
│   └── Modules/          # 应用业务模块（不经过 CubeMX 管理）
├── Drivers/              # STM32 HAL 库和 CMSIS
├── Middlewares/          # FreeRTOS、USB Host 中间件
├── cmake/                # 工具链配置和 CubeMX CMake 集成
├── docs/                 # 项目文档（中文）
├── ../robot_host/         # 树莓派 ROS2 相关代码
├── CMakeLists.txt        # 用户自己的构建配置
├── CMakePresets.json     # Debug/Release preset
└── small_car_f407.ioc    # STM32CubeMX 工程文件
```

## 应用模块架构（Core/Modules/）

| 模块 | 职责 |
|---|---|
| **Chassis** | 底盘混控、速度仲裁（安全 > 手柄 > 上位机 > 空闲）、运动学模型 |
| **Motor** | 电机 PWM 输出、编码器采集、里程计、轮速闭环控制器 |
| **Actuator** | 舵机 PWM 输出、手柄增量控制舵机 |
| **Input** | USB HID 游戏手柄数据接收 |
| **Sensors** | ICM20948 IMU、超声波测距 |
| **Comm** | 串口协议：调试命令(host_link)、ROS上位机协议(raspi_link)、调试日志(debug_uart)、系统状态 |
| **Display** | OLED 显示屏驱动 |

## 关键架构说明

### 控制优先级仲裁（control_mux）
1. **安全保护** — 超声波检测障碍时强制停车
2. **USB 手柄** — 左摇杆控制底盘，右摇杆增量控制两路舵机
3. **上位机** — 串口3接收 `mm/s` 和 `mrad/s` 物理速度
4. **空闲** — 无输入时停车

### 串口分配
- **UART1** — 调试命令接口（115200 8N1），支持 `ping`/`status`/`imu on`/`enc on` 等命令
- **UART3** — 树莓派通信，运行 ROS2 协议，包含状态上报和上位机控制

### 里程计数据流
编码器脉冲 → 轮速计算 → 里程计积分 → 可通过 `odom on` 串口命令输出调试

### 代码生成注意事项
修改 `small_car_f407.ioc` 后 CubeMX 会重新生成部分文件，**自定义代码必须放在 `USER CODE BEGIN/END` 区域内**。应用模块放在 `Core/Modules/` 下，不受 CubeMX 管理。

### 速度约定
电机速度范围 `-1000` ~ `1000`，正值正转，负值反转。

## 开发注意事项

- 新增应用模块优先放在 `Core/Modules/` 下对应的子目录
- 不提交固件二进制、map 文件、目标文件和 IDE 本地配置
- 硬件参考资料（原理图、资源分配表）放在 `docs/` 目录
