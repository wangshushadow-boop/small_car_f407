# 树莓派部署

## 当前部署

- 板卡：Raspberry Pi 5，aarch64
- 地址：`ubuntu@192.168.3.85`
- 宿主系统：Debian 13
- 容器：ROS 2 Kilted
- 镜像：`small-car-ros2:kilted`
- Compose 服务：`small_car_ros2`
- 串口：`/dev/serial/by-id/usb-1a86_USB_Single_Serial_5C2C059301-if00`

镜像同时安装底盘依赖与 Nav2，容器内运行两个核心进程：

1. `small_car_base_node`：独占 MCU 串口，发布遥测并订阅最终 `/cmd_vel`。
2. `nav2_container`：承载 Nav2 和 `robot_state_publisher` 组件。

## 一键部署

在 Windows 仓库根目录执行：

```powershell
.\scripts\sync_rpi_host.ps1
```

脚本会上传 `rpi_host`、构建并测试宿主机工具、重建镜像，然后启动 Compose。
宿主机工具使用 `build-host`；容器 colcon 使用 `build-ros`、`install-ros` 和
`log-ros`，避免缓存格式和文件所有权冲突。

## 验收

```bash
ssh ubuntu@192.168.3.85
cd ~/small_car_f407/rpi_host/ros2
docker compose ps
docker compose logs -f small_car_ros2
```

正常日志包括：

```text
applied and verified 23 chassis parameters
small car base ready: /dev/small_car_mcu @ 115200
Managed nodes are active
```

进入容器检查：

```bash
docker compose exec small_car_ros2 bash
source /opt/ros/kilted/setup.bash
source /workspace/rpi_host/install-ros/setup.bash
ros2 lifecycle get /controller_server
ros2 lifecycle get /planner_server
ros2 topic info /cmd_vel -v
ros2 topic echo /diagnostics
```

`/cmd_vel` 应为 `geometry_msgs/msg/TwistStamped`，订阅者应只有
`small_car_base`。MCU 诊断中的 `serial_write` 应为 `ok`。

## 当前限制

当前没有地图定位，Nav2 使用 odom-only 滚动代价地图。正式导航前需要接入
SLAM 或 Map Server + AMCL，并切换到 `map -> odom` 配置。
