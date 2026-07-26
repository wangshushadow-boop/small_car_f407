# 树莓派端架构

## 运行时进程

最终导航运行时固定为两个核心进程：

1. `small_car_base_node`：独占 MCU 串口，执行协议转换、硬限幅、失效停车、
   云台控制和底盘遥测发布。
2. `nav2_container`：按 Nav2 官方 Composition 方式承载 Planner、Controller、
   Behavior、Velocity Smoother、Collision Monitor、生命周期管理器以及
   `robot_state_publisher`。

当前阶段启动 `small_car_base_node`，并临时使用标准
`robot_state_publisher` 进程发布机器人描述。`nav2_container` 的源码配置和
启动骨架已经准备好，但 Nav2 软件包暂不安装；安装后
`robot_state_publisher` 会移入该容器。

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

## 设计约束

- 只有 `ros/` 模块可以依赖 `rclcpp` 和 ROS 消息。
- `protocol` 不访问串口，`transport` 不理解协议。
- `mcu` 组合协议与传输，但不创建 ROS 接口。
- `control` 和 `servo` 使用 SI 单位，不依赖 ROS。
- Nav2 负责规划、正常速度平滑与环境碰撞监控。
- 底盘节点始终保留速度硬限幅、命令时间戳检查、超时停车和串口故障停车。
