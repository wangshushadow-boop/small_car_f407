# 树莓派上位机

`ros2_host` 连接 STM32F407 底盘与 ROS 2/Nav2。MCU 负责电机闭环、传感器采集和实时安全；树莓派负责协议转换、状态估计、导航及上层应用。

## 目录

| 路径 | 内容 |
| --- | --- |
| `src/small_car_base` | MCU 通信、底盘安全、ROS 接口与 EKF 配置 |
| `src/small_car_description` | URDF、RViz 和固定 TF |
| `src/small_car_nav2` | Nav2 参数与整机启动入口 |
| `apps` | 无 ROS 依赖的诊断工具 |
| `ros2` | Dockerfile 与 Compose |
| `scripts` | WSL 环境和一键部署脚本 |
| `tools`、`systemd` | 语音及 MCU USB 恢复工具 |

## 启动入口

- `small_car_base/launch/base.launch.py`：启动底盘节点、EKF 和机器人模型。
- `small_car_nav2/launch/system.launch.py`：包含 `base.launch.py`，再启动 Nav2。

容器默认执行 `ros2 launch small_car_nav2 system.launch.py`。

## 文档

- [部署与验收](docs/deployment.md)
- [日常运维](operations.md)
- [架构与数据流](docs/architecture.md)
- [ROS 2 接口](docs/ros_interfaces.md)
- [Nav2 配置](docs/nav2_integration.md)
- [模块边界](docs/modules.md)
- [硬件连接](hardware.md)
- [历史迁移](docs/migration.md)

## 快速部署

```powershell
.\ros2_host\scripts\sync_ros2_host.ps1
```

脚本上传源码、运行宿主机测试，并重建和启动 ROS 2 容器。
