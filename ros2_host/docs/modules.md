# 模块划分

所有底盘模块位于 `rpi_host/src/small_car_base`，头文件、实现和测试按领域放在
同一目录。

| 模块 | 单一职责 | 允许依赖 |
| --- | --- | --- |
| `protocol` | MCU 帧编码、解析、CRC 和协议类型 | `mcu/types.hpp` |
| `transport` | Linux 串口生命周期与字节收发 | 系统 API |
| `mcu` | 组合协议与串口，提供 MCU 命令和最新遥测 | `protocol`、`transport` |
| `chassis` | 底盘参数文件读取、校验和下发 | `mcu`、yaml-cpp |
| `control` | 速度限幅、命令超时和近障停车 | C++ 标准库 |
| `servo` | 上下两路云台角度与 PWM 映射 | C++ 标准库 |
| `ros` | ROS 消息转换、Topic、Service、TF 和定时调度 | 以上全部 |

依赖方向只能从上层指向下层。禁止：

- `protocol` 或 `mcu` 包含 ROS 头文件。
- `control` 直接调用 `CarClient`。
- ROS 消息类型进入纯 C++ 模块接口。
- 正式逻辑订阅 `/debug/*`。

云台内部命令使用：

```cpp
struct GimbalCommand {
  double upper_rad;
  double lower_rad;
};
```

对应关节名为 `upper_servo_joint` 和 `lower_servo_joint`。
