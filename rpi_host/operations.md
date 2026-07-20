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

发送低速前进：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.1}, angular: {z: 0.0}}"
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

`/cmd_vel` 当前按配置的最大线速度和角速度线性换算为 MCU 的 `[-1000, 1000]` 控制量。电机闭环速度控制完成前，消息单位符合 ROS2 约定，但实际速度精度仍取决于底盘标定、负载和地面摩擦。

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
applied and verified 13 chassis parameters
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
