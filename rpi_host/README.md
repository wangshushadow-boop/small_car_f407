# 树莓派上位机 C++ 模块

本目录是运行在树莓派上的上位机代码，用于和 STM32F407 主控板通信，并提供相机、音频等外设测试工具。

## 目录说明

| 路径 | 说明 |
| --- | --- |
| `include/small_car_host/` | 对外头文件，算法模块后续优先引用这里的接口。 |
| `src/` | 上位机核心模块实现。 |
| `tools/` | 独立测试工具，例如相机抓图、V4L2 抓图、音频录放。 |
| `tests/` | 协议相关单元测试。 |
| `modules.md` | 各模块职责和使用方式说明。 |
| `raspberry-pi-debug.md` | 树莓派常用调试命令，包括 WiFi、串口、相机、音频和 STM32 烧录。 |

## 构建

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build
cmake --build build
```

## 协议测试

```bash
cd ~/small_car_f407/rpi_host
ctest --test-dir build --output-on-failure
```

## 串口通信测试

监听 MCU 上行数据：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 monitor
```

发送停止命令：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 stop
```

发送底盘速度命令：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 drive 200 0
```

发送舵机命令：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
```

## 可选工具

启用 OpenCV 相机工具：

```bash
cmake -S . -B build-opencv -DSMALL_CAR_ENABLE_OPENCV=ON
cmake --build build-opencv
```

音频录放测试：

```bash
./build/audio_loopback 5 ~/small_car_f407/audio_alsa_test.wav plughw:0,0
```
