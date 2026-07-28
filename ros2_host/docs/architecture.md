# 树莓派端架构

## 运行进程

`system.launch.py` 通过 `base.launch.py` 复用底盘启动逻辑，完整系统包含：

1. `small_car_base_node`：独占 MCU 串口，发布原始传感器并执行最终安全限制。
2. `ekf_filter_node`：融合轮速与 IMU Z 轴角速度，发布 `/odom` 和动态 TF。
3. `robot_state_publisher`：读取 URDF，发布固定 TF。
4. `nav2_container`：承载 Nav2 Planner、Controller、Behavior 和安全组件。

底盘、EKF 和机器人模型均为独立节点，不在 `nav2_container` 内重复创建。

## 控制链

```text
Nav2 Controller
  -> Velocity Smoother
  -> Collision Monitor
  -> /cmd_vel
  -> small_car_base_node
  -> MCU
```

`/cmd_vel` 是唯一速度入口。Nav2 负责规划、平滑和环境碰撞监控；底盘节点仍执行硬限幅、时间戳检查、命令超时和串口故障停车。

## 状态估计链

```text
MCU 累计编码器 -> /wheel/odom_raw --+
                                      +-> robot_localization -> /odom
MCU 原始 IMU ----> /imu/data_raw -----+                       -> odom -> base_link
```

MCU 不计算位姿。底盘节点完成编码器运动学换算和 IMU SI 单位转换，`robot_localization` 负责融合。接入 SLAM 或 AMCL 后，由定位模块补充 `map -> odom`。

## 设计约束

- `protocol` 不访问串口，`transport` 不理解协议。
- `mcu` 组合协议与传输，但不创建 ROS 接口。
- `control`、`servo` 使用 SI 单位且不依赖 ROS。
- 只有 `ros` 层转换 ROS 消息并调度其他模块。
- 同一时间只能有一个进程占用 MCU 串口。
