# 常见问题与解决方案

本文件记录已经确认解决的问题。每个问题只保留现象、确定原因和最终解决方案。

| 日期 | 问题现象 | 确定原因 | 解决方案 |
| --- | --- | --- | --- |
| 2026-07-26 | 临时调整 `wheel_pwm_min` 等底盘参数时，必须重启节点整组下发，异步 Topic 也容易错过 MCU 回读结果。 | ROS 节点没有同步的运行时参数入口，现有参数回读缓存也无法区分新旧响应。 | 将八项易变控制参数接入 `small_car_base` Parameter Server；`ros2 param set` 校验类型和范围后执行 SET，清除旧缓存再 GET，只有 MCU 回读一致才返回成功。 |
| 2026-07-26 | ROS 节点与 MCU 对相同的速度上限和停车距离分别配置，修改时容易出现数值不一致。 | `base.yaml` 和 `chassis.yaml` 分别保存同一物理约束，节点与 MCU 各自读取。 | 以 `chassis.yaml` 为唯一来源，底盘节点启动时一次加载并换算为 ROS SI 单位，同时复用同一组参数下发 MCU；补全 `base.yaml` 字段注释并新增配置测试。 |
| 2026-07-23 | WSL 中 `ros2` 不存在，或 `ros2 topic list` 长时间卡住。 | ROS2 环境只在子进程中生效，或旧 ROS2 daemon 保留了失效的 DDS 状态。 | 执行 `bash /mnt/d/stm32/demo/small_car_f407/scripts/setup_wsl_ros_env.sh`，由脚本配置环境并重建 daemon。若 WSL 只剩 `lo` 接口，先在 PowerShell 执行 `wsl --shutdown`。 |
| 2026-07-23 | Docker 能收到 `/cmd_vel`，但小车不运动。 | CH9102 长时间通信后触发树莓派 xHCI 端点错误，bridge 显示 `serial_write=failed`；USB 重新枚举还会使容器设备映射失效。 | 空闲心跳降为 1 Hz；收到控制命令但串口写失败时，由 `small-car-mcu-recovery.path` 自动复位 USB 并重建容器。确认日志出现 `applied and verified 15 chassis parameters`、`/diagnostics` 显示 `serial_write=ok`，且运动时 `/joint_states` 轮速非零。 |
| 2026-07-23 | 语音模块有日志，但无法判断运动控制哪一段失败。 | 麦克风、STT、意图规则、ROS 发布和 MCU 控制共用一条长链路，单看识别日志无法定位。 | 使用 `hermes_voice_daemon.py --test-motion 小车前进` 跳过麦克风和 STT，验证“本地意图规则 → `/cmd_vel` → MCU → 编码器”。自动 USB 恢复同时校验 `Voice daemon ready` 和 MCU 的 15 个参数。 |
| 2026-07-25 | ROS2 底盘由 bridge 和 motion controller 两个进程串联，存在内部 Topic、重复超时职责和源码跨目录复用。 | ROS 接口、运动控制、串口协议和遥测转换未按职责划分，ROS 包还通过相对路径引用工作区外模块。 | 工作区统一迁到 `rpi_host/src`，合并为唯一 `small_car_base_node`，以纯 C++ `control`、`servo`、`mcu`、`protocol`、`transport` 模块内部调用；删除 `/cmd_vel_mcu` 和 `/control/source`，并通过 Kilted colcon 构建及 2 项测试。 |
| 2026-07-26 | 新目录部署后 colcon 只构建顶层宿主机工程，Nav2 组合进程也未读取 costmap 参数。 | `rpi_host` 顶层 CMake 阻止递归发现 `src` 包；外部自建 component container 未接收 Nav2 完整参数文件。 | colcon 显式使用 `--base-paths src`，宿主机与 ROS 构建目录分离；按官方 bringup 将参数文件传给 `component_container_isolated`，三个 ROS 包构建成功且 Nav2 生命周期节点全部 active。 |
