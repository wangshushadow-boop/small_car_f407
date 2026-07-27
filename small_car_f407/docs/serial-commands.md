# 串口1调试命令

串口1用于人工调试，参数为 `115200 8N1`。命令只控制日志是否打印到串口1，不关闭模块本身。

| 命令 | 作用 |
| --- | --- |
| `ping` | 测试串口收发，返回 `[CMD] pong` |
| `status` | 查看当前日志开关 |
| `help` | 显示命令帮助 |
| `imu on` / `imu off` | 打开 / 关闭 IMU 日志 |
| `pad on` / `pad off` | 打开 / 关闭手柄日志 |
| `servo on` / `servo off` | 打开 / 关闭舵机日志 |
| `motor on` / `motor off` | 打开 / 关闭电机日志 |
| `enc on` / `enc off` | 打开 / 关闭编码器日志 |
| `ultra on` / `ultra off` | 打开 / 关闭超声日志 |

MCU 不再计算融合里程计。使用 `enc on` 检查四路累计编码器计数，IMU 与轮式
里程计融合由上位机 `robot_localization` 完成。
