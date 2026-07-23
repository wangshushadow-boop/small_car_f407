# 常见问题与解决方案

本文件记录已经确认解决的问题。每个问题只保留现象、确定原因和最终解决方案。

| 日期 | 问题现象 | 确定原因 | 解决方案 |
| --- | --- | --- | --- |
| 2026-07-23 | WSL 中 `ros2` 不存在，或 `ros2 topic list` 长时间卡住。 | ROS2 环境只在子进程中生效，或旧 ROS2 daemon 保留了失效的 DDS 状态。 | 执行 `bash /mnt/d/stm32/demo/small_car_f407/scripts/setup_wsl_ros_env.sh`，由脚本配置环境并重建 daemon。若 WSL 只剩 `lo` 接口，先在 PowerShell 执行 `wsl --shutdown`。 |
| 2026-07-23 | Docker 能收到 `/cmd_vel`，但小车不运动。 | CH9102 长时间通信后触发树莓派 xHCI 端点错误，bridge 显示 `serial_write=failed`；USB 重新枚举还会使容器设备映射失效。 | 空闲心跳降为 1 Hz；收到控制命令但串口写失败时，由 `small-car-mcu-recovery.path` 自动复位 USB 并重建容器。确认日志出现 `applied and verified 15 chassis parameters`、`/diagnostics` 显示 `serial_write=ok`，且运动时 `/joint_states` 轮速非零。 |
