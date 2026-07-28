# Nav2 集成

项目直接使用 ROS 2 Kilted 的 Nav2 包，不 fork 官方源码。自定义算法应实现 Nav2 Planner、Controller、Behavior、Costmap Layer 或 BehaviorTree 插件接口。

## 启动方式

`small_car_nav2/launch/system.launch.py` 包含 `small_car_base/base.launch.py`，再通过官方 `navigation_launch.py` 将 Nav2 组件加载到 `nav2_container`。底盘、EKF 和 `robot_state_publisher` 保持独立进程。

## 当前配置

| 功能 | 配置 |
| --- | --- |
| Planner | NavFn |
| Controller | Regulated Pure Pursuit |
| 速度平滑 | Nav2 Velocity Smoother |
| 最终碰撞过滤 | Nav2 Collision Monitor |
| 障碍输入 | `/ultrasonic/front` |
| 最终速度输出 | `/cmd_vel`，`TwistStamped` |
| 里程计 | `/odom`，`odom -> base_link` |
| 全局坐标 | 当前为 `odom` |

当前 odom-only 滚动代价地图只用于短距离链路验证，不替代正式定位。

## 接入地图定位

- 建图阶段接入 SLAM Toolbox。
- 已有地图时接入 Map Server 与 AMCL。
- 定位模块发布 `map -> odom`。
- 将全局代价地图和导航全局坐标切换为 `map`，并启用 Static Layer。

定位模块属于 ROS 层，不应放入 MCU 或 `small_car_base_node`。
