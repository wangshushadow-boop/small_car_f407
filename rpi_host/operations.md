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
| 5 | 重建并强制重启 ROS2 bridge 容器。 |
| 6 | 在同一容器中启动 Hermes 语音控制，并停用旧宿主机语音服务。 |

YAML 参数会在 bridge 重启后重新读取并下发到 MCU。

## 树莓派本地构建

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## ROS2 bridge

ROS2 运行在树莓派，STM32 负责传感器采集、电机控制、安全逻辑和实时里程计。`smallcar_ros_and_mcu_bridge` 将 MCU 串口协议转换为标准 ROS2 接口，算法节点不需要直接解析 MCU 协议。

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
source /workspace/rpi_host/ros2_ws/install/setup.bash
```

常用 ROS2 命令：

```bash
ros2 topic list
ros2 topic echo /odom
ros2 topic echo /imu/data
ros2 topic echo /ultrasonic/front
ros2 run tf2_ros tf2_echo odom base_link
ros2 service call /reset_odometry std_srvs/srv/Empty "{}"
```

持续发送低速前进，结束后按 `Ctrl+C`：

```bash
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.5}, angular: {z: 0.0}}"
```

发送停车命令：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.0}, angular: {z: 0.0}}"
```

ROS2 话题：

| 名称 | 类型 | 方向 | 内容 |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 订阅 | `linear.x` 前进速度，`angular.z` 左转角速度。 |
| `/odom` | `nav_msgs/msg/Odometry` | 发布 | 三维位置、姿态、前向速度和航向角速度。 |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | 发布 | ICM20948 原始加速度和角速度，不含姿态。 |
| `/imu/data` | `sensor_msgs/msg/Imu` | 发布 | MCU 融合姿态以及原始加速度、角速度。 |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | 发布 | 前方超声距离。 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 发布 | 四轮角速度和两路舵机指令位置。 |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 发布 | 手柄、IMU、超声和 MCU 错误状态。 |
| `/servo_controller/joint_trajectory` | `trajectory_msgs/msg/JointTrajectory` | 订阅 | `left_servo_joint`、`right_servo_joint` 目标位置。 |

ROS2 服务与坐标系：

| 名称 | 类型/关系 | 说明 |
| --- | --- | --- |
| `/reset_odometry` | `std_srvs/srv/Empty` | 清零 MCU 里程计。 |
| 动态 TF | `odom -> base_link` | 小车三维位姿。 |
| URDF 固定关节 | `base_link -> imu_link` | IMU 安装关系，由 `robot_state_publisher` 发布。 |
| URDF 固定关节 | `base_link -> ultrasonic_link` | 超声安装关系，由 `robot_state_publisher` 发布。 |

ROS2 相关参数：

| 文件 | 作用 |
| --- | --- |
| `rpi_host/config/chassis_params.yaml` | MCU 标定参数，bridge 启动时整组下发并回读校验。 |
| `rpi_host/ros2_ws/src/smallcar_ros_and_mcu_bridge/config/bridge.yaml` | 串口、坐标系、速度上限、舵机映射和传感器属性。 |
| `rpi_host/ros2_ws/src/small_car_description/urdf/robot_geometry.xacro` | IMU、超声、相机、树莓派、MCU 等硬件安装坐标。 |

`/cmd_vel` 使用 ROS2 标准单位。bridge 将线速度转换为 `mm/s`、角速度转换为 `mrad/s` 后通过协议 v2 下发；MCU 当前仍按标定上限换算为开环电机输出。电机闭环完成前，实际速度精度仍取决于负载、电量和地面摩擦。

## WSL 远程连接 ROS2

Windows 和树莓派连接同一局域网后，在 WSL 中执行：

```bash
bash /mnt/d/stm32/demo/small_car_f407/scripts/setup_wsl_ros_env.sh
```

脚本不修改 `~/.bashrc`，而是进入一个已经配置好的 ROS2 终端；输入 `exit` 可返回原终端。`fastdds_wsl.xml` 将 Fast DDS 固定到 PC 局域网地址；PC 地址变化后，需要同步修改该文件中的地址。

若 WSL 中执行 `ip -4 -brief address` 只显示 `lo`，请在 PowerShell 中执行 `wsl --shutdown`，然后重新进入 WSL。

确认树莓派 bridge 已被发现：

```bash
ros2 topic info /cmd_vel --verbose
```

输出中的 `Subscription count` 应为 `1`。Fast DDS 跨 WSL 时节点名可能显示为 `NODE_NAME_UNKNOWN`，不影响话题收发。

当前开环映射中，`2.0 rad/s` 对应电机输出 `1000`。实测转向起步输出约为 `620`，因此角速度通常需要达到约 `1.24 rad/s` 才能克服静摩擦；后续接入轮速闭环后再消除该限制。

## Hermes 语音控制

语音程序和 ROS2 bridge 由同一容器管理。语音程序只把本地规则明确识别出的动作发布到 `/cmd_vel`，不会执行大模型生成的速度值。

| 语音 | 动作 |
| --- | --- |
| “小车前进” | 以 `0.55 m/s` 前进 1.5 秒后停车。 |
| “小车后退” | 以 `0.45 m/s` 后退 1.5 秒后停车。 |
| “小车左转” | 以 `1.8 rad/s` 左转 1.5 秒后停车。 |
| “小车右转” | 以 `1.8 rad/s` 右转 1.5 秒后停车。 |
| “小车停止”（对话中也可说“停止”或“停车”） | 立即发送零速度。 |

查看语音识别、意图和运动发布日志：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose logs -f | grep -E "Voice daemon|Transcript|Intent|Motion"
```

运动速度和持续时间在 `rpi_host/ros2/compose.yaml` 的 `CAR_VOICE_*` 环境变量中配置，所有值仍受 bridge 的最大速度限制和 MCU 超声避障限制。

## 参数调试

修改：

```text
rpi_host/config/chassis_params.yaml
```

同步并生效：

```powershell
.\scripts\sync_rpi_host.ps1
```

查看 bridge 是否下发成功：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose logs -f
```

看到类似输出表示成功：

```text
applied and verified 15 chassis parameters
```

## 独立串口工具

使用前先停止 ROS2 bridge：

```bash
cd ~/small_car_f407/rpi_host/ros2
docker compose down
```

查看传感器：

```bash
cd ~/small_car_f407/rpi_host
./build/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra --odom
./build/sensor_monitor --port /dev/ttyACM0 --all --interval-ms 300
```

`sensor_monitor` 默认会尝试下发 `config/chassis_params.yaml`，如果参数校验失败只打印警告并继续显示数据。需要把参数失败当作错误时，追加：

```bash
--strict-config
```

控制命令：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 stop
./build/small_car_host_cli --port /dev/ttyACM0 drive 200 0
./build/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
./build/small_car_host_cli --port /dev/ttyACM0 odom-reset
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

bridge 收到 `/cmd_vel` 但串口写入失败时会自动创建恢复请求。宿主机随后复位
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
cmake --build build --target v4l2_capture
./build/v4l2_capture /dev/video0 mjpg ~/small_car_f407/v4l2_mjpg.jpg 1920 1080
./build/v4l2_capture /dev/video0 yuyv ~/small_car_f407/v4l2_yuyv.yuyv 1920 1080
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
