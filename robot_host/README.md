# 树莓派上位机

`robot_host` 连接 STM32F407 底盘与 ROS 2/Nav2。MCU 负责电机闭环、传感器采集和实时安全；树莓派负责协议转换、状态估计、导航及上层应用。

## 目录

| 路径 | 内容 |
| --- | --- |
| `src/small_car_base` | MCU 通信、底盘安全、ROS 接口与 EKF 配置 |
| `src/small_car_description` | URDF、RViz 和固定 TF |
| `src/small_car_nav2` | Nav2 参数与整机启动入口 |
| `src/small_car_av` | 树莓派摄像头与 Jabra 麦克风采集 |
| `src/small_car_interfaces` | 音频帧消息定义 |
| `apps` | 无 ROS 依赖的诊断工具 |
| `ros2` | Dockerfile 与 Compose |
| `scripts` | WSL 环境和一键部署脚本 |
| `tools`、`systemd` | 语音及 MCU USB 恢复工具 |

## 启动入口

- `small_car_base/launch/base.launch.py`：启动底盘节点、EKF 和机器人模型。
- `small_car_nav2/launch/system.launch.py`：包含 `base.launch.py`，再启动 Nav2。

容器启动 Nav2 时会同时启动真实音视频采集。当前对外使用的 topic 为：

| Topic | 类型 | 来源 |
| --- | --- | --- |
| `/car/camera/image/compressed` | `sensor_msgs/msg/CompressedImage` | `/dev/video0` 摄像头 |
| `/car/camera/camera_info` | `sensor_msgs/msg/CameraInfo` | 摄像头参数 |
| `/car/audio/input` | `small_car_interfaces/msg/AudioFrame` | Jabra 麦克风 |

## 音视频运行条件

树莓派需将 `/dev/video0` 和 `/dev/snd` 映射进 ROS 容器；容器使用 host 网络、
`ROS_DOMAIN_ID=0` 与 Cyclone DDS。WSL 订阅时使用相同的 Domain ID 和
`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`。录包示例：

```bash
ros2 bag record --topics /car/camera/image/compressed /car/camera/camera_info /car/audio/input
```

## Agent 感知输入

Agent 进程内的音视频缓存、VAD 与语音结束事件由
[`llm_agent/input/ros_perception.py`](../llm_agent/input/ros_perception.py) 直接订阅
`/car/camera/image/compressed` 与 `/car/audio/input` 完成，并在语音结束事件里组装
WAV 与 JPEG data URL 交给 LangGraph。LangGraph、模型路由、记忆和工具调度位于
[`llm_agent/agent`](../llm_agent/agent/README.md)。

## 文档

- [部署与验收](docs/deployment.md)
- [树莓派运维](operations.md)
- [架构与数据流](docs/architecture.md)
- [ROS 2 接口](docs/ros_interfaces.md)
- [小车底盘标定](docs/chassis_calibration.md)
- [Nav2 配置](docs/nav2_integration.md)
- [模块边界](docs/modules.md)
- [硬件连接](hardware.md)
- [历史迁移](docs/migration.md)

## 快速部署

```powershell
.\robot_host\scripts\sync_ros2_host.ps1
```

脚本上传源码、运行宿主机测试，并重建和启动 ROS 2 容器。
