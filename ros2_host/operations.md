# 树莓派操作文档

本文只保留常用命令。默认树莓派地址：

```text
ubuntu@192.168.3.85
```

## 一键同步

在 Windows 仓库根目录执行：

```powershell
.\scripts\sync_rpi_host.ps1
```

脚本会完成：

| 步骤 | 内容 |
| --- | --- |
| 1 | 打包本地 `rpi_host` 源码。 |
| 2 | 上传到树莓派 `/tmp`。 |
| 3 | 替换树莓派上的源码目录。 |
| 4 | 重新构建 C++ 工具并运行协议测试。 |
| 5 | 重建并强制重启包含底盘与 Nav2 的 ROS2 容器。 |
| 6 | 启动 `small_car_base_node` 与 `nav2_container` 两个核心进程。 |

YAML 参数会在底盘节点重启后重新读取并下发到 MCU。

## 树莓派本地构建

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## ROS2 与 Nav2

ROS2 运行在树莓派，`small_car_base_node` 独占 MCU 串口并提供标准底盘接口。
Nav2 已安装在同一镜像中，并通过独立的 `nav2_container` 进程运行官方组件。
当前为 odom-only 部署，正式地图导航前仍需接入定位。模块和接口定义见
`rpi_host/docs`。

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose up --build -d
docker compose logs -f
docker compose down
```

进入容器：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose exec small_car_ros2 bash
source /opt/ros/kilted/setup.bash
source /workspace/rpi_host/install-ros/setup.bash
```

常用 ROS2 命令：

```bash
ros2 topic list
ros2 topic echo /wheel/odom_raw
ros2 topic echo /imu/data_raw
ros2 topic echo /odom
ros2 topic echo /ultrasonic/front
ros2 run tf2_ros tf2_echo odom base_link
```

持续发送低速前进，结束后按 `Ctrl+C`：

```bash
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: auto, twist: {linear: {x: 0.1}, angular: {z: 0.0}}}"
```

发送停车命令：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: auto, twist: {linear: {x: 0.0}, angular: {z: 0.0}}}"
```

ROS2 话题：

| 名称 | 类型 | 方向 | 内容 |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/TwistStamped` | 订阅 | 唯一底盘速度入口，必须携带有效时间戳。 |
| `/wheel/odom_raw` | `nav_msgs/msg/Odometry` | 发布 | 编码器计算的未融合底盘速度。 |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | 发布 | MCU 原始加速度和角速度，不含姿态。 |
| `/odom` | `nav_msgs/msg/Odometry` | 发布 | `robot_localization` 的统一融合输出。 |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | 发布 | 前方超声距离。 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 可选发布 | 四轮角速度和两路舵机位置，默认 20Hz。 |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 发布 | 手柄、IMU、超声和 MCU 错误状态。 |
| `/servo_controller/joint_trajectory` | `trajectory_msgs/msg/JointTrajectory` | 订阅 | `upper_servo_joint`、`lower_servo_joint` 目标位置。 |

ROS2 服务与坐标系：

| 名称 | 类型/关系 | 说明 |
| --- | --- | --- |
| `/drive_on_heading` | `nav2_msgs/action/DriveOnHeading` | 由 Nav2 Behavior Server 提供。 |
| `/spin` | `nav2_msgs/action/Spin` | 由 Nav2 Behavior Server 提供。 |
| 动态 TF | `odom -> base_link` | 由 `robot_localization` 唯一发布。 |
| URDF 固定关节 | `base_link -> imu_link` | IMU 安装关系，由 `robot_state_publisher` 发布。 |
| URDF 固定关节 | `base_link -> ultrasonic_link` | 超声安装关系，由 `robot_state_publisher` 发布。 |

ROS2 相关参数：

| 文件 | 作用 |
| --- | --- |
| `rpi_host/src/small_car_base/config/chassis.yaml` | ROS 与 MCU 共用的底盘物理约束、里程计和闭环标定参数；启动时整组下发并回读校验。 |
| `rpi_host/src/small_car_base/config/base.yaml` | 串口、命令时序、坐标系、传感器描述、协方差和云台映射参数。 |
| `rpi_host/src/small_car_base/config/ekf.yaml` | 轮式里程计和 IMU 的 EKF 融合参数。 |
| `rpi_host/src/small_car_nav2/config/nav2.yaml` | Nav2 Planner、Controller、Behavior 和速度链参数。 |
| `rpi_host/src/small_car_description/urdf/robot_geometry.xacro` | 硬件安装坐标。 |

`/cmd_vel` 使用 ROS2 标准单位和 `TwistStamped`。Nav2 完成正常速度平滑及
碰撞监控，底盘节点执行最终硬限幅和失效保护，然后转换为 `mm/s`、`mrad/s`
通过协议 v3 下发。

## WSL 远程连接 ROS2

Windows 和树莓派连接同一局域网后，在 WSL 中执行：

```bash
bash /mnt/d/stm32/demo/small_car_f407/scripts/setup_wsl_ros_env.sh
```

脚本不修改 `~/.bashrc`，而是进入一个已经配置好的 ROS2 终端；输入 `exit` 可返回原终端。`fastdds_wsl.xml` 将 Fast DDS 固定到 PC 局域网地址；PC 地址变化后，需要同步修改该文件中的地址。

若 WSL 中执行 `ip -4 -brief address` 只显示 `lo`，请在 PowerShell 中执行 `wsl --shutdown`，然后重新进入 WSL。

确认树莓派底盘节点已被发现：

```bash
ros2 topic info /cmd_vel --verbose
```

输出中的 `Subscription count` 应为 `1`。Fast DDS 跨 WSL 时节点名可能显示为 `NODE_NAME_UNKNOWN`，不影响话题收发。

MCU 已使用左右轮速度闭环执行树莓派下发的物理速度。起步 PWM、PI、加速度限制和
左右轮补偿统一在 `rpi_host/src/small_car_base/config/chassis.yaml` 中调整。

## Hermes 语音控制

语音运动程序调用 Nav2 标准 Action，因此要等 Nav2 安装并启动后再启用。
语音程序只执行本地规则明确识别出的动作：
定距命令调用 `/drive_on_heading`，定角命令调用 `/spin`，停止命令发布零速度。

| 语音 | 动作 |
| --- | --- |
| “小车前进” | 默认通过里程计闭环前进 `0.5m`。 |
| “小车后退” | 默认通过里程计闭环后退 `0.5m`。 |
| “小车左转” | 默认通过里程计闭环左转 `90°`。 |
| “小车右转” | 默认通过里程计闭环右转 `90°`。 |
| “小车前进一米” | 解析距离后调用定距 Action。 |
| “小车右转四十五度” | 解析角度后调用定角 Action。 |
| “小车停止”（对话中也可说“停止”或“停车”） | 立即发送零速度。 |

查看语音识别、意图和运动发布日志：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose logs -f | grep -E "Voice daemon|Transcript|Intent|Motion"
```

不经过麦克风和语音识别，直接验证“本地意图规则 → Nav2 Action → `/cmd_vel` → MCU →
编码器”链路：

```bash
docker compose exec small_car_ros2 \
  python3 /workspace/rpi_host/tools/hermes_voice_daemon.py --test-motion 小车前进
```

该命令使用正式语音解析和运动 Action，完成、取消或超时后都会停车。动作速度、
减速、容差和超时由 `small_car_nav2/config/nav2.yaml` 管理；语音模块只提供目标距离或角度。

## 参数调试

修改：

```text
rpi_host/src/small_car_base/config/chassis.yaml
```

同步并生效：

```powershell
.\scripts\sync_rpi_host.ps1
```

查看底盘节点是否下发成功：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose logs -f
```

看到类似输出表示成功：

```text
applied and verified 23 chassis parameters
```

## 独立串口工具

使用前先停止 ROS2 底盘：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose down
```

查看传感器：

```bash
cd ~/small_car_f407/rpi_host
./build-host/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra --odom
./build-host/sensor_monitor --port /dev/ttyACM0 --all --interval-ms 300
```

`sensor_monitor` 默认会尝试下发 `src/small_car_base/config/chassis.yaml`，如果参数校验失败只打印警告并继续显示数据。需要把参数失败当作错误时，追加：

```bash
--strict-config
```

控制命令：

```bash
./build-host/small_car_host_cli --port /dev/ttyACM0 stop
./build-host/small_car_host_cli --port /dev/ttyACM0 drive 200 0
./build-host/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
./build-host/small_car_host_cli --port /dev/ttyACM0 odom-reset
```

## WiFi 排查

```bash
rfkill list
sudo rfkill unblock wifi
ip link
iw dev
nmcli device
sudo nmcli radio wifi on
sudo systemctl restart NetworkManager
nmcli dev wifi list
sudo nmcli dev wifi connect "你的WiFi名称" password "你的WiFi密码"
hostname -I
journalctl -u NetworkManager -n 100 --no-pager
```

## 串口排查

```bash
ls /dev/ttyACM*
lsusb
dmesg | grep -i tty
sudo usermod -aG dialout ubuntu
minicom -D /dev/ttyACM0 -b 115200
```

自动恢复 MCU USB：

```bash
sudo install -m 0644 systemd/small-car-mcu-recovery.path /etc/systemd/system/
sudo install -m 0644 systemd/small-car-mcu-recovery.service /etc/systemd/system/
sudo chmod +x tools/recover_mcu_usb.sh
sudo systemctl daemon-reload
sudo systemctl enable --now small-car-mcu-recovery.path
systemctl status small-car-mcu-recovery.path
journalctl -u small-car-mcu-recovery.service -f
```

底盘节点收到 `/cmd_vel` 但串口写入失败时会自动创建恢复请求。宿主机随后复位
CH9102、等待串口重新枚举并重建 ROS2 容器，无需手工重启树莓派。

## 相机测试

```bash
ls /dev/video*
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
```

V4L2 抓图：

```bash
cd ~/small_car_f407/rpi_host
cmake --build build-host --target v4l2_capture
./build-host/v4l2_capture /dev/video0 mjpg ~/small_car_f407/v4l2_mjpg.jpg 1920 1080
./build-host/v4l2_capture /dev/video0 yuyv ~/small_car_f407/v4l2_yuyv.yuyv 1920 1080
```

## 音频测试

```bash
arecord -l
aplay -l
arecord -D plughw:0,0 -f S16_LE -r 16000 -d 5 -t wav test.wav
aplay -D plughw:0,0 -f S16_LE -r 16000 -c 1 test.wav
speaker-test -D hw:0,0 -t sine -f 1000 -c 2 -r 48000 -l 1
```

## STM32 烧录

树莓派安装工具：

```bash
sudo apt update
sudo apt install -y stlink-tools
```

检查 ST-LINK：

```bash
lsusb
sudo st-info --probe
```

Windows 编译固件：

```powershell
cmake --build --preset Debug
```

发送固件到树莓派：

```powershell
scp build/Debug/small_car_f407.bin ubuntu@192.168.3.85:~/small_car_f407.bin
```

树莓派烧录：

```bash
sudo st-flash --reset write ~/small_car_f407.bin 0x08000000
```
