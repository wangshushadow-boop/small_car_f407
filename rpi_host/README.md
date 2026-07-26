# 树莓派上位机

`rpi_host` 是 ROS 2/Nav2 与 STM32F407 底盘之间的上位机工程。STM32 负责电机、
编码器、IMU、超声和安全实时任务；树莓派负责导航、语音、视觉、设备接入和诊断。

## 运行时边界

- `small_car_base_node`：唯一自研底盘进程，独占 MCU 串口。
- `nav2_container`：按官方 Composition 方式运行 Nav2 组件和
  `robot_state_publisher`。

底盘速度只通过 `/cmd_vel` `geometry_msgs/msg/TwistStamped` 输入。
云台继续使用 `/servo_controller/joint_trajectory`，关节名为
`upper_servo_joint` 和 `lower_servo_joint`。

## 目录

| 目录 | 说明 |
| --- | --- |
| `src/small_car_base` | 串口、协议、控制安全、云台和唯一底盘节点 |
| `src/small_car_description` | URDF/Xacro、RViz 和机器人描述 |
| `src/small_car_nav2` | Nav2 参数和官方组件化启动 |
| `apps` | 独立诊断与硬件测试工具 |
| `docs` | 架构、模块、ROS 接口和迁移文档 |
| `ros2` | ROS 2 Dockerfile 与 Compose 配置 |
| `tools` | 语音、USB 恢复和辅助脚本 |

## 文档

- [总体架构](docs/architecture.md)
- [模块边界](docs/modules.md)
- [ROS 2 接口](docs/ros_interfaces.md)
- [Nav2 集成](docs/nav2_integration.md)
- [树莓派部署](docs/deployment.md)
- [迁移说明](docs/migration.md)
- [运维命令](operations.md)
- [硬件说明](hardware.md)

## 构建和启动

```bash
cd /workspace/rpi_host
source /opt/ros/kilted/setup.bash
colcon --log-base log-ros build --base-paths src \
  --build-base build-ros --install-base install-ros --symlink-install
source install-ros/setup.bash
ros2 launch small_car_nav2 system.launch.py
```

容器部署：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose up --build -d
docker compose logs -f
```

Dockerfile 已安装 Nav2，Compose 默认启动 `small_car_base_node` 与
`nav2_container`。当前使用 odom-only 滚动代价地图；接入定位后再切换到
`map -> odom` 和静态地图配置。
