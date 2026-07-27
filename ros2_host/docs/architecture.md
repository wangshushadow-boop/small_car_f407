# 树莓派端架构

## 运行时进程

最终导航运行时包含三个核心进程：

1. `small_car_base_node`：独占 MCU 串口，执行协议转换、硬限幅、失效停车、
   云台控制和底盘遥测发布。
2. `ekf_filter_node`：融合轮式速度和 IMU Z 轴角速度，发布 `/odom` 和
   `odom -> base_link`。
3. `nav2_container`：按 Nav2 官方 Composition 方式承载 Planner、Controller、
   Behavior、Velocity Smoother、Collision Monitor、生命周期管理器以及
   `robot_state_publisher`。

语音、标定和串口监视工具是运维工具，不得直接打开正在被
`small_car_base_node` 占用的串口。

## 控制链

```text
Nav2 Controller / Behavior
  -> cmd_vel_nav
  -> Velocity Smoother
  -> cmd_vel_smoothed
  -> Collision Monitor
  -> /cmd_vel (TwistStamped)
  -> small_car_base_node
  -> CommandSafety
  -> MCU
```

`/cmd_vel` 是底盘唯一的 ROS 速度入口。`/cmd_vel_mcu` 已删除，进程内部使用
普通 C++ 类型和函数调用。

## 状态估计链

```text
MCU EncoderCounts -> /wheel/odom_raw --+
                                      +-> robot_localization -> /odom
MCU ImuRaw -------> /imu/data_raw ----+                       -> odom -> base_link
```

MCU 不再计算或上传位姿。底盘节点只完成累计编码器运动学换算和 IMU SI
单位转换；Nav2 只消费 EKF 的 `/odom`。以后接入 SLAM 或 AMCL 时，由定位模块
补充 `map -> odom`，本地 EKF 链路保持不变。

## 设计约束

- 只有 `ros/` 模块可以依赖 `rclcpp` 和 ROS 消息。
- `protocol` 不访问串口，`transport` 不理解协议。
- `mcu` 组合协议与传输，但不创建 ROS 接口。
- `control` 和 `servo` 使用 SI 单位，不依赖 ROS。
- Nav2 负责规划、正常速度平滑与环境碰撞监控。
- 底盘节点始终保留速度硬限幅、命令时间戳检查、超时停车和串口故障停车。
