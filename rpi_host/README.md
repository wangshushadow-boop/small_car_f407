# 树莓派上位机 C++ 模块

本目录是运行在树莓派上的上位机侧代码，负责通过 USART3 二进制协议与 MCU 通信。

## 模块边界

| 模块 | 职责 |
| --- | --- |
| `protocol` | 只负责帧格式、CRC、payload 编解码，不访问串口 |
| `serial_port` | 只负责 Linux 串口打开、读写、关闭 |
| `CarClient` | 对算法层提供干净接口，隐藏串口和协议细节 |
| `small_car_host_cli` | 临时调试入口，后续算法接入时可以不用 |

## 本地协议测试

```bash
cmake -S rpi_host -B rpi_host/build
cmake --build rpi_host/build
ctest --test-dir rpi_host/build --output-on-failure
```

## 树莓派构建

```bash
sudo apt update
sudo apt install -y cmake g++
cmake -S rpi_host -B rpi_host/build
cmake --build rpi_host/build
```

## 树莓派串口调试

监听 MCU 上行数据：

```bash
./rpi_host/build/small_car_host_cli --port /dev/ttyACM0 monitor
```

周期发送心跳并监听：

```bash
./rpi_host/build/small_car_host_cli --port /dev/ttyACM0 monitor --heartbeat-ms 1000
```

发送底盘命令：

```bash
./rpi_host/build/small_car_host_cli --port /dev/ttyACM0 drive 200 0
```

停车：

```bash
./rpi_host/build/small_car_host_cli --port /dev/ttyACM0 stop
```

发送舵机命令：

```bash
./rpi_host/build/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
```

