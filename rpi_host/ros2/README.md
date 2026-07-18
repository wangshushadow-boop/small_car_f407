# ROS2 运行说明

树莓派系统为 Debian Trixie，ROS2 Kilted 使用官方 ARM64 容器运行。STM32 继续负责实时驱动，`small_car_bridge` 负责将串口协议转换成标准 ROS2 接口。

## 启动

```bash
cd ~/small_car_f407/rpi_host/ros2
sudo docker compose up --build
```

后台运行：

```bash
sudo docker compose up --build -d
sudo docker compose logs -f
```

停止：

```bash
sudo docker compose down
```

## 调试

进入已启动的容器：

```bash
sudo docker compose exec small_car_ros2 bash
source /opt/ros/kilted/setup.bash
source /workspace/rpi_host/ros2_ws/install/setup.bash
```

查看话题和 TF：

```bash
ros2 topic list
ros2 topic echo /odom
ros2 topic echo /imu/data
ros2 topic echo /ultrasonic/front
ros2 run tf2_ros tf2_echo odom base_link
```

发送低速前进命令：

```bash
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.1}, angular: {z: 0.0}}"
```

复位里程计：

```bash
ros2 service call /reset_odometry std_srvs/srv/Empty "{}"
```

控制舵机：

```bash
ros2 topic pub --once /servo_controller/joint_trajectory \
  trajectory_msgs/msg/JointTrajectory \
  "{joint_names: [left_servo_joint, right_servo_joint], points: [{positions: [0.2, -0.2]}]}"
```

## 注意

- `/cmd_vel` 超过 500 ms 未更新时，桥接节点自动发送停车指令。
- STM32 内部继续保持 USB 手柄高于树莓派指令的控制优先级。
- 当前 ROS 时间戳为树莓派收到串口帧的时间；MCU 时间同步留待下一阶段实现。
- `imu_link` 和 `ultrasonic_link` 的实际安装偏移尚未测量，launch 中暂为零。
