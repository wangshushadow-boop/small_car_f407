# 树莓派上位机描述

`rpi_host` 是运行在树莓派上的上位机工程。它负责连接 STM32F407 MCU，
把 MCU 的传感器、里程计和状态数据转换为 ROS2 接口，并向 MCU 下发底盘、舵机和运行时参数。

正式运行链路：

```text
ROS2 算法节点 -> smallcar_ros_and_mcu_bridge -> 串口/USB -> STM32F407 MCU
```

## 核心职责

| 职责 | 说明 |
| --- | --- |
| MCU 通信 | 使用串口协议连接 STM32，收发控制命令和传感器数据。 |
| ROS2 桥接 | 发布 `/odom`、`/imu/data`、`/ultrasonic/front` 等话题，订阅 `/cmd_vel`。 |
| 参数下发 | 启动时读取 `config/chassis_params.yaml`，批量下发到 MCU 并回读校验。 |
| 调试工具 | 提供传感器监视、CLI 控制、相机抓图、音频录放等独立工具。 |
| 机器人描述 | 使用 URDF/Xacro 描述小车底盘、轮子、树莓派、MCU、超声、云台和相机。 |

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `include/small_car_host/` | C++ 对外接口。 |
| `modules/protocol/` | 串口二进制协议、CRC、帧同步。 |
| `modules/transport/` | Linux 串口读写封装。 |
| `modules/mcu_client/` | MCU 客户端，封装控制、舵机、参数、状态读取。 |
| `modules/chassis/` | 读取并下发底盘参数。 |
| `modules/audio/` | ALSA 音频录放封装。 |
| `apps/` | 独立调试工具。 |
| `config/chassis_params.yaml` | 底盘和手柄参数记录。 |
| `ros2/` | ROS2 Docker 运行环境。 |
| `ros2_ws/` | ROS2 工作区，包含 bridge 和 URDF。 |
| `tools/` | 辅助脚本，如语音和音频测试脚本。 |

## 主要模块

| 模块 | 位置 | 说明 |
| --- | --- | --- |
| ROS2 bridge | `ros2_ws/src/smallcar_ros_and_mcu_bridge/` | 连接 ROS2 与 MCU。 |
| 小车模型 | `ros2_ws/src/small_car_description/` | URDF、RViz 配置和 TF 关系。 |
| 协议模块 | `modules/protocol/` | 帧编码、解码和协议测试。 |
| MCU 客户端 | `modules/mcu_client/` | 给 CLI、sensor monitor、ROS2 bridge 复用。 |
| 参数模块 | `modules/chassis/` | 读取 YAML 并校验 MCU 参数。 |
| 音频模块 | `modules/audio/` | 用于 USB 麦克风和音响测试。 |

## ROS2 接口

| 接口 | 类型 | 方向 | 说明 |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 订阅 | 上位机控制底盘运动。 |
| `/odom` | `nav_msgs/msg/Odometry` | 发布 | MCU 融合后的里程计。 |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | 发布 | IMU 原始加速度和角速度。 |
| `/imu/data` | `sensor_msgs/msg/Imu` | 发布 | 带 MCU 姿态结果的 IMU 数据。 |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | 发布 | 前向超声距离。 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 发布 | 轮子和舵机关节状态。 |
| `/reset_odometry` | `std_srvs/srv/Empty` | 服务 | 清零里程计并重新标定陀螺仪零偏。 |

更完整的 ROS2 设计见仓库根目录的 `docs/ros2-interface.md`。

## 参数文件

底盘参数统一记录在：

```text
rpi_host/config/chassis_params.yaml
```

`smallcar_ros_and_mcu_bridge` 启动时只读取一次该文件，随后批量下发到 MCU。
修改 YAML 后，需要重启 bridge 或执行同步脚本才会生效。

## 配套文档

`rpi_host` 目录只保留三份文档：

| 文档 | 说明 |
| --- | --- |
| `README.md` | 树莓派上位机描述。 |
| `hardware.md` | 树莓派相关硬件、接口和设备说明。 |
| `operations.md` | 构建、同步、ROS2、调试和烧录命令。 |

