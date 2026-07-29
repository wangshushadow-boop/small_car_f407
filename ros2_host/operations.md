# 树莓派操作文档

默认上位机为 `ubuntu@192.168.3.85`，部署目录为 `~/small_car_f407/ros2_host`。首次部署见 [部署文档](docs/deployment.md)。

## Compose

```bash
cd ~/small_car_f407/ros2_host/ros2
docker compose ps
docker compose logs -f small_car_ros2
docker compose restart small_car_ros2
docker compose down
docker compose up -d
```

仅修改源码、launch 或 YAML 时使用 `docker compose up -d --force-recreate`；修改 Dockerfile 或系统依赖后使用 `docker compose up --build -d`。

进入容器：

```bash
docker compose exec small_car_ros2 bash
source /opt/ros/kilted/setup.bash
source /workspace/ros2_host/install-ros/setup.bash
```

## ROS 2 检查

```bash
ros2 node list
ros2 topic echo /wheel/odom_raw --once
ros2 topic echo /imu/data_raw --once
ros2 topic echo /odom --once
ros2 topic echo /diagnostics --once
ros2 run tf2_ros tf2_echo odom base_link
ros2 lifecycle get /controller_server
```

Topic 类型和发布者见 [ROS 2 接口](docs/ros_interfaces.md)。

## 低速运动测试

先架空车轮或确保前方安全。持续前进，按 `Ctrl+C` 结束：

```bash
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: auto, twist: {linear: {x: 0.1}, angular: {z: 0.0}}}"
```

停车：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: auto, twist: {linear: {x: 0.0}, angular: {z: 0.0}}}"
```

## 参数

| 文件 | 内容 |
| --- | --- |
| `src/small_car_base/config/chassis.yaml` | 轮距、编码器比例、轮速闭环和安全限制 |
| `src/small_car_base/config/base.yaml` | 串口、发布频率、坐标系和协方差 |
| `src/small_car_base/config/ekf.yaml` | 轮速与 IMU 融合 |
| `src/small_car_nav2/config/nav2.yaml` | Planner、Controller、Costmap 和安全组件 |

在线调整示例：

```bash
ros2 param get /small_car_base wheel_pwm_min
ros2 param set /small_car_base wheel_pwm_min 550
```

在线修改不会写回 YAML；需要持久化时修改文件并重新部署。

## 独立串口工具

工具会独占 MCU 串口，使用前先执行 `docker compose down`。

```bash
cd ~/small_car_f407/ros2_host
./build-host/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra
./build-host/sensor_monitor --port /dev/ttyACM0 --all --interval-ms 300
./build-host/small_car_host_cli --port /dev/ttyACM0 stop
./build-host/small_car_host_cli --port /dev/ttyACM0 drive 200 0
./build-host/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
```

`sensor_monitor` 默认只警告参数下发失败；需要失败即退出时增加 `--strict-config`。

## WSL 连接

```bash
bash /mnt/d/stm32/demo/smart_car/ros2_host/scripts/setup_wsl_ros_env.sh
ros2 topic info /cmd_vel --verbose
```

PC 地址变化后同步修改 `config/fastdds_wsl.xml`；若 WSL 只有 `lo` 接口，在 PowerShell 执行 `wsl --shutdown`。

## Hermes 语音

```bash
docker compose logs -f | grep -E "Voice daemon|Transcript|Intent|Motion"
docker compose exec small_car_ros2 \
  python3 /workspace/ros2_host/tools/hermes_voice_daemon.py --test-motion 小车前进
```

## MCU USB 恢复

```bash
sudo install -m 0644 systemd/small-car-mcu-recovery.path /etc/systemd/system/
sudo install -m 0644 systemd/small-car-mcu-recovery.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now small-car-mcu-recovery.path
journalctl -u small-car-mcu-recovery.service -f
```

相机、音频、ST-LINK 和串口连接见 [硬件文档](hardware.md)。Wi-Fi 使用 `nmcli device`、`nmcli dev wifi list` 和 `hostname -I` 排查。
