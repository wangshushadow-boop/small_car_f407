# ROS2 接口

ROS2 运行在树莓派，STM32 负责传感器采集、电机控制、安全逻辑和实时里程计。`small_car_bridge` 将 USART3 二进制协议转换为标准 ROS2 消息，算法节点不直接依赖 MCU 协议。

## 话题

| 名称 | 类型 | 方向 | 内容 |
| --- | --- | --- | --- |
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 订阅 | `linear.x` 前进速度，`angular.z` 左转角速度 |
| `/odom` | `nav_msgs/msg/Odometry` | 发布 | 三维位置、姿态、前向速度和航向角速度 |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | 发布 | ICM20948 原始加速度和角速度，不含姿态 |
| `/imu/data` | `sensor_msgs/msg/Imu` | 发布 | MCU 融合姿态以及原始加速度、角速度 |
| `/ultrasonic/front` | `sensor_msgs/msg/Range` | 发布 | 前方超声距离 |
| `/joint_states` | `sensor_msgs/msg/JointState` | 发布 | 四轮角速度和两路舵机指令位置 |
| `/diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | 发布 | 手柄、IMU、超声和 MCU 错误状态 |
| `/servo_controller/joint_trajectory` | `trajectory_msgs/msg/JointTrajectory` | 订阅 | `left_servo_joint`、`right_servo_joint` 目标位置 |

## 服务与坐标系

| 名称 | 类型/关系 | 说明 |
| --- | --- | --- |
| `/reset_odometry` | `std_srvs/srv/Empty` | 清零 MCU 里程计 |
| 动态 TF | `odom -> base_link` | 包含坡道高度及横滚、俯仰、航向的三维位姿 |
| 静态 TF | `base_link -> imu_link` | IMU 安装位姿，待实测 |
| 静态 TF | `base_link -> ultrasonic_link` | 超声安装位姿，待实测 |

## 参数

| 文件 | 作用 |
| --- | --- |
| `rpi_host/config/chassis_params.yaml` | MCU 标定参数，节点启动时整组下发并回读校验 |
| `rpi_host/ros2_ws/src/small_car_bridge/config/bridge.yaml` | 串口、ROS 坐标系、速度上限、舵机映射和传感器属性 |

`/cmd_vel` 当前按配置的最大线速度和角速度线性换算为 MCU 的 `[-1000, 1000]` 控制量。电机闭环速度控制尚未完成前，消息单位符合 ROS2 约定，但实际速度精度仍取决于底盘标定和负载。
