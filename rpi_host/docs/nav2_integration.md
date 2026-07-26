# Nav2 集成

> 当前状态：Nav2 已安装在树莓派的 ROS 2 Kilted Docker 镜像中，默认 Compose
> 同时启动 `small_car_base_node` 与 `nav2_container`。

## 开发方式

Nav2 保持官方 Server、Action、Lifecycle 和 pluginlib 架构，不 fork Nav2，
也不把各 Server 改成项目私有的普通 C++ 调用。

算法扩展使用对应官方插件接口：

- `nav2_core::GlobalPlanner`
- `nav2_core::Controller`
- Nav2 Behavior plugin
- Costmap Layer plugin
- BehaviorTree.CPP 节点插件

运行时通过 `navigation_launch.py` 的 `use_composition=True` 把 Nav2 节点装入
`nav2_container`。自研底盘保持独立进程，以隔离串口和硬件故障。

完整参数文件也传给 `component_container_isolated`。这是组合模式下让
local/global costmap 子节点正确读取各自参数的必要条件。

## 当前配置

- Controller：Regulated Pure Pursuit
- Planner：NavFn
- 速度平滑：Nav2 Velocity Smoother
- 最终安全过滤：Nav2 Collision Monitor
- 前向障碍输入：`/ultrasonic/front`
- 最终速度输出：`/cmd_vel`，类型为 `TwistStamped`
- 底盘坐标：`base_link`
- 里程计坐标：`odom`
- 当前全局坐标：`odom`
- local/global costmap：odom-only 滚动窗口，使用前向超声波 Range Layer

当前 odom-only 配置用于在尚未接入定位和地图时完成 Nav2 全链路部署验证。
它只能进行局部、短距离导航，不能替代正式定位。

正式导航阶段必须提供稳定的 `map -> odom` 变换。建图阶段可接入 SLAM
Toolbox；已有地图阶段应使用 Map Server 与 AMCL。届时把 `bt_navigator` 和
global costmap 的全局坐标切回 `map`，global costmap 改用 Static Layer。
定位模块继续装入或配合 `nav2_container`，不要放进底盘进程。
