# ROS 2 接口

## 命令

| 名称 | 类型 | 使用方 |
| --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/TwistStamped` | 底盘订阅，唯一速度入口 |
| `/servo_controller/joint_trajectory` | `trajectory_msgs/msg/JointTrajectory` | 云台订阅 |

`/cmd_vel` 必须携带有效时间戳；底盘只使用 `linear.x` 和 `angular.z`。云台关节名为 `upper_servo_joint`、`lower_servo_joint`。

## 状态

| 名称 | 类型 | 发布者 |
| --- | --- | --- |
| `/wheel/odom_raw` | `nav_msgs/msg/Odometry` | `small_car_base_node` |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | `small_car_base_node` |
| `/odom` | `nav_msgs/msg/Odometry` | `robot_localization` |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | `small_car_base_node` |
| `/joint_states` | `sensor_msgs/msg/JointState` | `small_car_base_node` |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 底盘与 EKF |
| `/tf` | `tf2_msgs/msg/TFMessage` | 动态 TF 发布者 |
| `/tf_static` | `tf2_msgs/msg/TFMessage` | `robot_state_publisher` |

`/wheel/odom_raw` 和 `/imu/data_raw` 只作为 EKF 输入；Nav2 使用融合后的 `/odom`。IMU 的 `orientation_covariance[0] = -1` 表示姿态不可用。

## 坐标系

- `robot_localization` 唯一发布 `odom -> base_link`。
- `robot_state_publisher` 发布传感器和底盘的 URDF 固定关系。
- SLAM 或 AMCL 负责未来的 `map -> odom`。

## 在线参数

`chassis.yaml` 中的 24 项底盘参数均允许在线调整，包括编码器比例、轮距、
IMU 标定、手柄区间、速度限制、轮速闭环、最小 PWM 和左右轮补偿。例如：

```bash
ros2 param get /small_car_base wheel_pwm_min
ros2 param set /small_car_base wheel_pwm_min 550
ros2 param set /small_car_base odom_mm_per_tick_num 2513
ros2 param set /small_car_base wheel_track_mm 115
```

每次只能修改一项。节点校验参数并写入 MCU，回读一致后才返回成功；涉及里程计、
IMU 换算和安全限幅的参数也会立即更新上位机计算。在线修改不会写回
`config/chassis.yaml`，确认标定结果后必须手动同步到 YAML。
