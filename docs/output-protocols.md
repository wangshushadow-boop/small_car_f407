# 三路输出协议说明

本文档记录当前固件已经实现的三个输出通道：串口1、串口3、OLED。未实现或没有可靠数据来源的字段不写入协议。

## 总体分工

| 输出通道 | 面向对象 | 主要用途 | 代码入口 |
| --- | --- | --- | --- |
| 串口1 USART1 | 开发者 | 调试命令、调试日志 | `host_link.c` / `debug_uart.c` |
| 串口3 USART3 | 树莓派上位机 | 结构化状态数据、上位机通信 | `raspi_link.c` / `system_status.c` |
| OLED | 本地观察 | 低频状态摘要 | `oled.c` / `system_status.c` |

## 串口1：调试控制台

### 串口参数

| 参数 | 值 |
| --- | --- |
| 串口 | USART1 |
| 引脚 | PA9 TX，PA10 RX |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | 无 |
| 数据格式 | 文本 |

### 命令

串口1命令只控制“是否打印到串口1”，不关闭模块本身。

| 命令 | 作用 |
| --- | --- |
| `ping` | 测试串口1收发，返回 `[CMD] pong` |
| `help` | 显示命令帮助 |
| `?` | 同 `help` |
| `status` | 查看当前调试打印开关 |
| `imu on` / `imu off` | 打开 / 关闭 IMU 调试打印 |
| `pad on` / `pad off` | 打开 / 关闭手柄数据调试打印 |
| `servo on` / `servo off` | 打开 / 关闭舵机输出调试打印 |
| `motor on` / `motor off` | 打开 / 关闭电机输出调试打印 |
| `enc on` / `enc off` | 打开 / 关闭编码器调试打印 |
| `ultra on` / `ultra off` | 打开 / 关闭超声波调试打印 |

### 输出示例

```text
[CMD] pong
[CMD] imu=on pad=off servo=off motor=off enc=on ultra=off
[IMU] ax=151 ay=-334 az=1931 gx=27 gy=-24 gz=-32 temp=5200
[ENC] A=0/0 B=0/0 C=0/0 D=0/0
```

## 串口3：树莓派状态协议

### 串口参数

| 参数 | 值 |
| --- | --- |
| 串口 | USART3 |
| 引脚 | PD8 TX，PD9 RX |
| 波特率 | 115200 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验位 | 无 |
| 数据格式 | 文本，一行一条消息 |

### 启动输出

MCU启动后，串口3会输出：

```text
[RPI] USART3 ready, 115200 8N1
[RPI] commands: ping, help, status, echo <text>
```

### 命令

| 命令 | 回复 |
| --- | --- |
| `ping` | `[RPI] pong` |
| `help` 或 `?` | `[RPI] commands: ping, help, status, echo <text>` |
| `status` | `[RPI] ok` |
| `echo <text>` | `[RPI] echo <text>` |

### 周期状态输出

`system_status.c` 每 500ms 向串口3输出状态。当前有两类消息：`STAT` 和 `IMU`。

#### STAT

格式：

```text
STAT src=<source> en=<0|1> f=<forward> t=<turn> pad=<0|1> ultra=<mm|-1> enc=<a>,<b>,<c>,<d>
```

字段说明：

| 字段 | 含义 |
| --- | --- |
| `src` | 当前控制源：`NONE`、`HOST`、`PAD`、`SAFE` |
| `en` | 当前控制命令是否有效，`1` 有效，`0` 无效 |
| `f` | 前进 / 后退命令值，来自 `ControlCommand.forward` |
| `t` | 转向命令值，来自 `ControlCommand.turn` |
| `pad` | USB手柄连接状态，`1` 已连接，`0` 未连接 |
| `ultra` | 超声距离，单位 mm；无有效数据时为 `-1` |
| `enc` | 四路编码器累计计数，顺序为 A、B、C、D |

示例：

```text
STAT src=PAD en=1 f=200 t=-50 pad=1 ultra=320 enc=102,98,101,100
```

#### IMU

格式：

```text
IMU ax=<accel_x> ay=<accel_y> az=<accel_z> gx=<gyro_x> gy=<gyro_y> gz=<gyro_z> temp=<temperature>
```

字段说明：

| 字段 | 含义 |
| --- | --- |
| `ax`、`ay`、`az` | ICM20948 加速度原始值 |
| `gx`、`gy`、`gz` | ICM20948 陀螺仪原始值 |
| `temp` | ICM20948 温度原始值 |

示例：

```text
IMU ax=151 ay=-334 az=1931 gx=27 gy=-24 gz=-32 temp=5200
```

## OLED：本地状态摘要

OLED不作为完整通信协议，只显示低频摘要。当前每 1000ms 刷新一次。

### 显示内容

| 行 | 内容 | 示例 |
| --- | --- | --- |
| 1 | 当前控制源 | `SRC:PAD` |
| 2 | 前进和转向命令 | `F:200 T:-50` |
| 3 | 手柄连接状态 | `PAD:OK` 或 `PAD:NO` |
| 4 | 超声距离 | `UL:320mm` 或 `UL:--` |

### 控制源取值

| 显示值 | 含义 |
| --- | --- |
| `NONE` | 当前没有有效控制源 |
| `HOST` | 上位机控制 |
| `PAD` | USB手柄控制 |
| `SAFE` | 安全保护接管 |

## 当前不确定或未实现的内容

以下内容当前没有可靠数据来源或尚未实现，因此没有写入协议字段：

- 电池电压
- 电流
- 里程
- 真实车速
- 舵机角度的物理单位
- 树莓派下发运动控制协议


