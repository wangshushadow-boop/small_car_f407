# 重构迁移说明

| 旧项 | 新项 |
| --- | --- |
| `rpi_host/ros2_ws/src` | `rpi_host/src` |
| `smallcar_ros_and_mcu_bridge` | `small_car_base` |
| `small_car_motion_controller` | Nav2 标准 Behavior/Controller |
| 两个自研 ROS 进程 | 一个 `small_car_base_node` |
| `/cmd_vel` `Twist` | `/cmd_vel` `TwistStamped` |
| `/cmd_vel_mcu` | 删除，改为进程内调用 |
| `/control/source` | 删除，改为进程内状态 |
| `/debug/imu/raw` | `/imu/data_raw` |
| `left_servo_joint` | `upper_servo_joint` |
| `right_servo_joint` | `lower_servo_joint` |
| `bridge.yaml` + `motion_controller.yaml` | `small_car_base/config/base.yaml` |

工作区构建入口：

```bash
cd /workspace/rpi_host
source /opt/ros/kilted/setup.bash
colcon build --symlink-install --packages-skip small_car_nav2
source install/setup.bash
ros2 launch small_car_base base.launch.py
```

Nav2 安装完成后再改用：

```bash
ros2 launch small_car_nav2 system.launch.py
```

发布速度示例：

```bash
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/TwistStamped \
  "{header: auto, twist: {linear: {x: 0.1}}}"
```
