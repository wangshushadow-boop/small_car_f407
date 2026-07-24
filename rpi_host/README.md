# 树莓派上位机描述

本文只说明树莓派上位机部分是什么，不记录具体操作命令。构建、同步、调试、ROS2 启动和烧录步骤统一放在 operations.md；硬件连接和设备说明统一放在 hardware.md。

## 是什么

rpi_host 是小车项目中运行在树莓派上的上位机工程，处在 ROS2 算法和 STM32F407 MCU 之间。

STM32F407 MCU 负责电机、舵机、编码器、IMU、超声等底层实时任务。树莓派负责 ROS2、视觉、语音、调试工具和后续算法集成。rpi_host 的作用就是把这两部分连接起来，让上层算法可以通过统一接口读取小车状态并下发控制指令。

## 负责什么

| 部分 | 说明 |
| --- | --- |
| MCU 通信 | 封装树莓派与 STM32 之间的数据收发。 |
| 调试工具 | 提供传感器查看、底盘控制、相机抓图和音频测试能力。 |
| ROS2 接入 | 将 MCU 数据转换为 ROS2 话题、服务和 TF。 |
| 机器人描述 | 描述小车底盘、轮子、树莓派、MCU、超声、云台和相机的安装关系。 |
| 参数管理 | 保存底盘标定参数，并在上位机启动时同步给 MCU。 |

## 目录定位

| 目录 | 说明 |
| --- | --- |
| include | 对外头文件。 |
| modules | 可复用 C++ 模块。 |
| apps | 独立调试程序。 |
| config | 参数配置。 |
| ros2 | ROS2 运行环境。 |
| ros2_ws | ROS2 工作区。 |
| tools | 辅助脚本。 |

## 主要模块

| 模块 | 说明 |
| --- | --- |
| protocol | 负责 MCU 二进制协议的帧编码、帧解析、CRC 校验和消息类型转换。 |
| transport | 负责 Linux 串口设备打开、配置、读取和写入。 |
| mcu_client | 基于 protocol 和 transport 封装 MCU 客户端，提供控制、舵机、参数和传感器读取接口。 |
| chassis | 负责读取底盘标定参数，并将参数批量下发到 MCU。 |
| audio | 封装 USB 麦克风和音响的基础录放能力。 |
| smallcar_ros_and_mcu_bridge | ROS2 与 MCU 的桥接节点，负责话题、服务、TF 和串口协议之间的转换。 |
| small_car_motion_controller | 统一处理直接速度、定距和定角控制，再将唯一的最终速度发送给 MCU bridge。 |
| small_car_description | 小车 URDF/Xacro 描述包，负责 RViz 模型显示和固定 TF 关系。 |

## 调试工具

| 工具 | 说明 |
| --- | --- |
| sensor_monitor | 查看 MCU 上传的 IMU、编码器、超声、里程计和设备状态。 |
| small_car_host_cli | 发送简单的底盘、舵机和里程计控制指令。 |
| camera_capture | 使用 OpenCV 获取 USB 摄像头图片。 |
| v4l2_capture | 使用 V4L2 获取 USB 摄像头图片，支持 MJPG 和 YUYV。 |
| audio_loopback | 测试 USB 麦克风录音和音响播放。 |
| jabra_record_playback | 针对 Jabra USB 音频设备的录放测试脚本。 |

## ROS2 接口

| 接口 | 类型 | 方向 | 说明 |
| --- | --- | --- | --- |
| /cmd_vel | geometry_msgs/msg/Twist | 订阅 | 运动控制入口；接收遥控、导航或算法节点的速度命令。 |
| /cmd_vel_mcu | geometry_msgs/msg/Twist | 内部 | 运动控制器仲裁并平滑后的唯一 MCU 速度入口。 |
| /odom | nav_msgs/msg/Odometry | 发布 | 发布 MCU 融合后的三维里程计。 |
| /imu/data | sensor_msgs/msg/Imu | 发布 | 发布带 MCU 姿态结果的 IMU 数据。 |
| /imu/data_raw | sensor_msgs/msg/Imu | 可选发布 | 原始加速度和角速度；默认关闭，标定 IMU 时开启。 |
| /ultrasonic/front | sensor_msgs/msg/Range | 发布 | 发布前向超声距离。 |
| /joint_states | sensor_msgs/msg/JointState | 可选发布 | 四轮和两路舵机状态；关闭后 MCU 不上传轮速调试流。 |
| /diagnostics | diagnostic_msgs/msg/DiagnosticArray | 发布 | 发布手柄、IMU、超声和 MCU 状态诊断。 |
| /control/source | std_msgs/msg/UInt8 | 内部发布 | 当前控制源发生变化时发布，用于手柄优先级仲裁。 |
| /servo_controller/joint_trajectory | trajectory_msgs/msg/JointTrajectory | 订阅 | 接收两路舵机的目标位置。 |
| /reset_odometry | std_srvs/srv/Empty | 服务 | 清零 MCU 里程计。 |
| /drive_on_heading | nav2_msgs/action/DriveOnHeading | Action | 按里程计闭环行驶指定距离。 |
| /spin | nav2_msgs/action/Spin | Action | 按里程计闭环旋转指定角度。 |

ROS2 底盘命令固定沿 `/cmd_vel -> small_car_motion_controller -> /cmd_vel_mcu -> bridge -> MCU`
流动，算法节点不得直接向 `/cmd_vel_mcu` 发布。
