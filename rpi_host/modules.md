# 树莓派侧模块说明

本文档说明 `rpi_host` 目录中已经存在的 C++ 模块。当前目标是让树莓派作为上位机：负责运行算法、接收 MCU 上传的数据，并向 MCU 下发控制命令。

## 模块总览

| 模块 | 主要文件 | 职责 | 当前状态 |
| --- | --- | --- | --- |
| 协议模块 | `protocol.hpp` / `protocol.cpp` | 负责二进制帧编码、解码、CRC 校验、帧同步。 | 已实现并有单元测试。 |
| 串口模块 | `serial_port.hpp` / `serial_port.cpp` | 负责 Linux 串口打开、配置、读写、关闭。 | 已实现。 |
| 小车客户端 | `car_client.hpp` / `car_client.cpp` | 面向算法层提供干净接口，隐藏串口和协议细节。 | 已实现基础命令和状态接收。 |
| 命令行工具 | `main.cpp` | 用于临时调试 MCU 通信。 | 已实现。 |
| 音频模块 | `audio_device.hpp` / `audio_device.cpp` | 使用 ALSA API 录音、播放、保存 WAV。 | 已实现并在树莓派测试通过。 |
| 音频测试工具 | `tools/audio_loopback.cpp` | 录制一段音频并立即播放。 | 已实现并测试通过。 |
| 传感器监视工具 | `tools/sensor_monitor.cpp` | 远程查看 IMU、编码器、超声、底盘和设备状态日志。 | 已实现并测试通过。 |
| OpenCV 相机工具 | `tools/camera_capture.cpp` | 使用 OpenCV 从 USB 摄像头抓取 JPG。 | 已实现并测试过。 |
| V4L2 相机工具 | `tools/v4l2_capture.cpp` | 不依赖 OpenCV，直接使用 V4L2 抓取 MJPG 或 YUYV。 | 已实现并测试过。 |
| 协议测试 | `tests/protocol_test.cpp` | 验证协议编码、解码、CRC、帧解析。 | 已实现。 |

## 协议模块

| 项目 | 说明 |
| --- | --- |
| 对外头文件 | `include/small_car_host/protocol.hpp` |
| 命名空间 | `small_car` |
| 帧头 | `AA 55` |
| 最大负载 | `64` 字节 |
| 校验 | `CRC16-CCITT-FALSE` |
| 主要类 | `FrameParser`、`PacketCodec` |
| 主要函数 | `EncodeFrame()`、`DecodePayload()`、`MakeDriveFrame()`、`MakeServoFrame()` |

常用消息：

| 方向 | 消息 | 类型值 | 说明 |
| --- | --- | --- | --- |
| 树莓派到 MCU | `kControl` | `0x01` | 控制底盘速度。 |
| 树莓派到 MCU | `kServo` | `0x02` | 控制两路舵机。 |
| 树莓派到 MCU | `kHeartbeat` | `0x03` | 心跳。 |
| MCU 到树莓派 | `kChassisStatus` | `0x81` | 底盘状态。 |
| MCU 到树莓派 | `kEncoderDelta` | `0x82` | 编码器增量。 |
| MCU 到树莓派 | `kImuRaw` | `0x83` | IMU 原始数据。 |
| MCU 到树莓派 | `kDeviceStatus` | `0x84` | 设备在线状态。 |
| MCU 到树莓派 | `kAck` | `0x85` | 命令响应。 |
| MCU 到树莓派 | `kOdometry` | `0x86` | 编码器和 IMU 融合后的里程计数据。 |

## 串口模块

| 项目 | 说明 |
| --- | --- |
| 对外头文件 | `include/small_car_host/serial_port.hpp` |
| 主要类 | `SerialPort` |
| 默认使用 | `/dev/ttyACM0` 或实际识别到的串口设备 |
| 默认波特率 | `115200` |
| 数据格式 | `8N1` |

接口说明：

| 接口 | 说明 |
| --- | --- |
| `Open(device, baudrate)` | 打开并配置串口。 |
| `Close()` | 关闭串口。 |
| `IsOpen()` | 判断串口是否打开。 |
| `Read(max_size)` | 读取串口数据。 |
| `Write(data)` | 写入串口数据。 |

## 小车客户端模块

| 项目 | 说明 |
| --- | --- |
| 对外头文件 | `include/small_car_host/car_client.hpp` |
| 主要类 | `CarClient` |
| 作用 | 给算法层提供更简单的小车控制接口。 |

控制接口：

| 接口 | 说明 |
| --- | --- |
| `Open(device, baudrate)` | 打开和 MCU 的串口连接。 |
| `SendHeartbeat()` | 发送心跳。 |
| `SendStop()` | 下发停车命令。 |
| `SendDrive(forward, turn)` | 下发底盘速度命令。 |
| `SendServo(left_us, right_us)` | 下发两路舵机脉宽。 |
| `Poll()` | 读取并解析 MCU 上传的数据。 |

状态接口：

| 接口 | 说明 |
| --- | --- |
| `GetChassisStatus()` | 获取最近一次底盘状态。 |
| `GetEncoderDelta()` | 获取最近一次编码器增量。 |
| `GetImuRaw()` | 获取最近一次 IMU 数据。 |
| `GetOdometry()` | 获取最近一次里程计融合数据。 |
| `GetDeviceStatus()` | 获取最近一次设备状态。 |
| `GetLastAck()` | 获取最近一次 MCU 命令响应。 |

## 命令行调试工具

| 项目 | 说明 |
| --- | --- |
| 可执行文件 | `small_car_host_cli` |
| 源文件 | `src/main.cpp` |
| 作用 | 手动测试串口协议和 MCU 控制链路。 |

常用命令：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 monitor
./build/small_car_host_cli --port /dev/ttyACM0 heartbeat
./build/small_car_host_cli --port /dev/ttyACM0 stop
./build/small_car_host_cli --port /dev/ttyACM0 drive 200 0
./build/small_car_host_cli --port /dev/ttyACM0 servo 1500 1500
```

## 传感器监视工具

| 项目 | 说明 |
| --- | --- |
| 可执行文件 | `sensor_monitor` |
| 源文件 | `tools/sensor_monitor.cpp` |
| 作用 | 通过串口3协议远程查看 MCU 上传的传感器和状态数据。 |

常用命令：

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-sensor-monitor
cmake --build build-sensor-monitor --target sensor_monitor
./build-sensor-monitor/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra
```

参数说明：

| 参数 | 说明 |
| --- | --- |
| `--imu` | 显示 IMU 原始数据。 |
| `--enc` | 显示四路编码器增量。 |
| `--ultra` | 显示超声距离。 |
| `--chassis` | 显示底盘状态。 |
| `--device` | 显示设备在线状态。 |
| `--all` | 显示全部数据。 |
| `--interval-ms 300` | 设置每类数据的最小打印间隔。 |

## 音频模块

| 项目 | 说明 |
| --- | --- |
| 对外头文件 | `include/small_car_host/audio_device.hpp` |
| 主要类 | `AudioDevice` |
| 后端 | ALSA |
| 默认设备 | `plughw:0,0` |
| 默认格式 | `S16_LE` |
| 默认采样率 | `16000 Hz` |
| 默认声道 | 单声道 |

接口说明：

| 接口 | 说明 |
| --- | --- |
| `RecordSeconds(seconds)` | 录制指定秒数，返回 PCM 采样数据。 |
| `Play(samples)` | 播放 PCM 采样数据。 |
| `SaveWav(path, samples)` | 保存为 WAV 文件。 |

测试命令：

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-audio
cmake --build build-audio --target audio_loopback
./build-audio/audio_loopback 5 ~/small_car_f407/audio_alsa_test.wav plughw:0,0
```

## 相机模块

当前相机部分先提供两个测试工具，方便后续算法接入前确认摄像头、分辨率和图像格式。

| 工具 | 依赖 | 说明 |
| --- | --- | --- |
| `camera_capture` | OpenCV | 直接抓取并保存 JPG。 |
| `v4l2_capture` | Linux V4L2 | 抓取 MJPG 或 YUYV，不依赖 OpenCV。 |

OpenCV 抓图：

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-opencv -DSMALL_CAR_ENABLE_OPENCV=ON
cmake --build build-opencv --target camera_capture
./build-opencv/camera_capture 0 ~/small_car_f407/camera_capture.jpg 1920 1080
```

V4L2 抓取 MJPG：

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build-v4l2
cmake --build build-v4l2 --target v4l2_capture
./build-v4l2/v4l2_capture /dev/video0 mjpg ~/small_car_f407/v4l2_mjpg.jpg 1920 1080
```

V4L2 抓取 YUYV：

```bash
./build-v4l2/v4l2_capture /dev/video0 yuyv ~/small_car_f407/v4l2_yuyv.yuyv 1920 1080
```

YUYV 模式会额外生成一个 `.ppm` 预览图，用来快速确认画面是否正常。

## 后续接入建议

| 方向 | 建议 |
| --- | --- |
| 算法接入 | 优先调用 `CarClient`，不要直接操作串口和协议字节。 |
| 视觉算法 | 可以先使用 OpenCV 工具验证摄像头，再把采集逻辑封装成独立类。 |
| 音频算法 | 优先复用 `AudioDevice::RecordSeconds()` 获取 PCM 数据。 |
| 实时控制 | 控制命令建议周期下发，MCU 侧保留超时保护。 |
