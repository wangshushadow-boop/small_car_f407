# ROS 2 接口

## 稳定命令接口

| 名称 | 类型 | 方向 | Owner |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/TwistStamped` | 底盘订阅 | Nav2 Collision Monitor |
| `/servo_controller/joint_trajectory` | `trajectory_msgs/msg/JointTrajectory` | 底盘订阅 | 云台业务模块 |
| `/reset_odometry` | `std_srvs/srv/Empty` | 底盘服务 | `small_car_base_node` |

`/cmd_vel` 必须携带非零时间戳；超过 `cmd_vel_timeout_ms` 或明显来自未来的命令
会被拒绝。底盘仅使用 `twist.linear.x` 和 `twist.angular.z`。

云台 Topic 名保持兼容，关节名已改为：

- `upper_servo_joint`
- `lower_servo_joint`

## 稳定状态接口

| 名称 | 类型 | Owner |
| --- | --- | --- |
| `/odom` | `nav_msgs/msg/Odometry` | `small_car_base_node` |
| `/imu/data` | `sensor_msgs/msg/Imu` | `small_car_base_node` |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | `small_car_base_node` |
| `/joint_states` | `sensor_msgs/msg/JointState` | `small_car_base_node` |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | `small_car_base_node` |
| `/tf` | `tf2_msgs/msg/TFMessage` | 底盘和 Nav2 定位 |
| `/tf_static` | `tf2_msgs/msg/TFMessage` | `robot_state_publisher` |

## 调试接口

| 名称 | 类型 | 默认 |
| --- | --- | --- |
| `/debug/imu/raw` | `sensor_msgs/msg/Imu` | 关闭 |
| `/debug/nav2/front_stop_polygon` | Collision Monitor 可视化 | 按 Nav2 配置 |

`debug_enabled=false` 时底盘节点不创建原始 IMU publisher。调试接口不得成为
控制、定位或安全逻辑的输入。

## 已删除接口

- `/cmd_vel_mcu`
- `/control/source`
- 自研 `/spin`
- 自研 `/drive_on_heading`

`Spin` 和 `DriveOnHeading` 由 Nav2 Behavior Server 提供。
