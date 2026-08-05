# 树莓派上位机

`ros2_host` 连接 STM32F407 底盘与 ROS 2/Nav2。MCU 负责电机闭环、传感器采集和实时安全；树莓派负责协议转换、状态估计、导航及上层应用。

## 目录

| 路径 | 内容 |
| --- | --- |
| `src/small_car_base` | MCU 通信、底盘安全、ROS 接口与 EKF 配置 |
| `src/small_car_description` | URDF、RViz 和固定 TF |
| `src/small_car_nav2` | Nav2 参数与整机启动入口 |
| `src/small_car_av` | 树莓派摄像头与 Jabra 麦克风采集 |
| `src/small_car_interfaces` | 音频帧消息定义 |
| `src/small_car_agent` | ROS 音视频缓存、VAD 与 Agent 事件发布 |
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

`small_car_agent` 在 Agent 所在的 ROS 2 进程中订阅相机压缩图像和音频帧，提供
`PerceptionTool.get_latest_perception()`。该方法输出最新 JPEG、与其时间对齐的最近音频
和标准 WAV 字节；模型适配层可通过 `to_model_input()` 取得 JPEG data URL 与元数据。
模块边界如下：

| 模块 | 职责 |
| --- | --- |
| `perception.py` | 音视频帧、WAV 和 Agent 感知快照的数据结构 |
| `perception_tool.py` | 订阅 ROS topic、缓存并按时间组装快照 |
| `voice_activity.py` | 基于音频能量检测一句话开始与结束 |
| `agent_events.py` | 在语音结束时发布 `/car/agent/speech_finished` |

LangGraph、模型路由、记忆和工具调度位于 [`llm_agent/agent`](../llm_agent/agent/README.md)。
详细启动与接入方式见 [WSL 操作](docs/wsl_operations.md)。

## 文档

- [部署与验收](docs/deployment.md)
- [树莓派运维](operations.md)
- [WSL 操作](docs/wsl_operations.md)
- [架构与数据流](docs/architecture.md)
- [ROS 2 接口](docs/ros_interfaces.md)
- [小车底盘标定](docs/chassis_calibration.md)
- [Nav2 配置](docs/nav2_integration.md)
- [模块边界](docs/modules.md)
- [硬件连接](hardware.md)
- [历史迁移](docs/migration.md)

## 快速部署

```powershell
.\ros2_host\scripts\sync_ros2_host.ps1
```

脚本上传源码、运行宿主机测试，并重建和启动 ROS 2 容器。
