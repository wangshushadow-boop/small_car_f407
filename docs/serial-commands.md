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
| `odom on` / `odom off` | 打开 / 关闭里程计日志 |
| `odom reset` | 清零里程计，重新开始陀螺仪零偏校准 |

里程计调试时建议先执行：

```text
enc on
odom on
odom reset
```

`odom on` 会输出两类数据：

```text
[ODOM] t=123456 dist=120 speed=80 yaw=1500 rate=20 cal=1
[ODOM_WHEEL] L=78 R=82 turn=4 dL=2 dR=2
```

| 字段 | 含义 |
| --- | --- |
| `dist` | 编码器估算累计距离，单位 mm |
| `speed` | 编码器估算前后线速度，单位 mm/s |
| `yaw` | IMU Z 轴积分航向角，单位 mdeg，`1000=1deg` |
| `rate` | IMU Z 轴角速度，单位 mdeg/s |
| `cal` | 陀螺仪零偏校准是否完成 |
| `L` / `R` | 左右侧线速度，单位 mm/s |
| `turn` | 右侧速度减左侧速度，单位 mm/s |
| `dL` / `dR` | 当前周期左右侧位移增量，单位 mm |
