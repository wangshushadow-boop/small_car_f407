# 树莓派上位机 C++ 模块

本目录是运行在树莓派上的上位机代码。正式运行链路采用 ROS2 Kilted，
`smallcar_ros_and_mcu_bridge` 负责连接 STM32F407；原有命令行、相机和音频程序只保留为硬件诊断工具。

## 目录说明

| 路径 | 说明 |
| --- | --- |
| `include/small_car_host/` | 对外头文件，算法模块后续优先引用这里的接口。 |
| `modules/` | 上位机公共模块实现，例如协议、串口、MCU 客户端、底盘参数和音频。 |
| `apps/` | C++ 独立诊断工具，例如 CLI、传感器监视、相机抓图、V4L2 抓图、音频录放。 |
| `tools/` | Python 或 Shell 工具，例如语音守护进程、STT 测试和 Jabra 录放脚本。 |
| `tests/` | 协议相关单元测试。 |
| `ros2_ws/` | ROS2 工作区，包含 `smallcar_ros_and_mcu_bridge` 和小车 URDF 描述包。 |
| `ros2/` | ROS2 Kilted ARM64 容器和运行说明。 |
| `modules.md` | 各模块职责和使用方式说明。 |
| `raspberry-pi-debug.md` | 树莓派常用调试命令，包括 WiFi、串口、相机、音频和 STM32 烧录。 |
| `hermes-voice-recovery.md` | 树莓派丢失或重装后，完整恢复 Hermes 语音助手的步骤。 |

## ROS2 运行

完整接口见 `docs/ros2-interface.md`，树莓派启动命令见 `ros2/README.md`。
Windows 修改代码后，在仓库根目录执行：

```powershell
.\scripts\sync_rpi_host.ps1
```

脚本会同步代码、运行协议测试、重建 ROS2 包并重启 bridge。正常运行时算法节点使用
`/cmd_vel`、`/odom`、`/imu/data`、`/ultrasonic/front` 等 ROS2 接口，不直接打开串口。

## 独立诊断工具

以下 CMake 工具用于 ROS2 bridge 未运行时排查硬件。使用前进入 `rpi_host/ros2`
执行 `docker compose down`，避免多个进程同时占用 `/dev/ttyACM0`。

### 构建

```bash
cd ~/small_car_f407/rpi_host
cmake -S . -B build
cmake --build build
```

### 协议测试

```bash
cd ~/small_car_f407/rpi_host
ctest --test-dir build --output-on-failure
```

### 串口通信测试

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

清零里程计：

```bash
./build/small_car_host_cli --port /dev/ttyACM0 odom-reset
```

远程查看传感器日志：

```bash
./build/sensor_monitor --port /dev/ttyACM0 --imu --enc --ultra --odom
```

只查看编码器和里程计：

```bash
./build/sensor_monitor --port /dev/ttyACM0 --enc --odom
```

### 可选工具

启用 OpenCV 相机工具：

```bash
cmake -S . -B build-opencv -DSMALL_CAR_ENABLE_OPENCV=ON
cmake --build build-opencv
```

音频录放测试：

```bash
./build/audio_loopback 5 ~/small_car_f407/audio_alsa_test.wav plughw:0,0
```

Jabra Speak 410 专用录音回放测试（默认录制 5 秒）：

```bash
chmod +x ~/small_car_f407/rpi_host/tools/jabra_record_playback.sh
~/small_car_f407/rpi_host/tools/jabra_record_playback.sh 5
```

脚本会暂时停止 Hermes 语音服务，使用 16 kHz 单声道录音，转换成 48 kHz 双声道后回放，并在结束时自动恢复原先运行中的服务。录音保存在 `~/jabra-mic-test.wav`，实际播放文件保存在 `~/jabra-mic-test-48k.wav`。

## Hermes 常驻语音助手

语音助手在树莓派上以 systemd 用户服务运行，启动链路如下：

当前语音硬件为 `Jabra SPEAK 410 USB`：PortAudio 输入设备 `0`，录音采样率 `16000 Hz`；ALSA 播放设备 `plughw:CARD=USB,DEV=0`，播放采样率 `48000 Hz`、双声道。录音与播放采样率必须分开设置，否则回复语音会变速。麦克风增益设置为 `7/7`，扬声器设置为 `8/11`。

```text
USB 麦克风 → 本地 VAD → SenseVoice-Small INT8 → 本地唤醒词匹配
→ Hermes / MiniMax M3 → MiniMax TTS → USB 扬声器
```

当前 STT 在树莓派本地运行，使用 `sherpa-onnx 1.13.4` 加载 SenseVoice-Small INT8 模型。模型和独立 Python 环境位于 `~/.hermes/`，音频不会上传到云端；Whisper tiny/base 仅保留用于手动回退。

相机已接入语音链路。每次唤醒会从 `/dev/video0` 抓取一张 `1280x720` JPEG；说出“看看前面”“前面有什么”“你看到了啥”“能看见什么”“拍张照片”“重新看一下”“这个离我们多远”等视觉请求时，会重新抓图，并通过 Hermes `--image` 将图片和问题一起发送给 MiniMax。除固定触发词外，守护进程还会匹配“看/看见/拍/观察”等动作与“什么/啥/前面/画面/距离”等目标的口语组合。支持一次说完“小车，看看前面有什么”，也支持先说“小车”，听到回应后再说视觉指令。相机不是持续开启，最新图片保存在 `~/.hermes/run/car-voice/camera-latest.jpg`，权限为 `600`。

视觉问答会把该次抓拍图片上传给 MiniMax；普通语音问题不会附带图片。

语音文本会先经过结构化意图路由。相机、退出、音乐、音量和导航等明确表达优先由本地规则识别；包含动作线索但规则不确定的表达，会调用 MiniMax 返回受约束 JSON，普通聊天不会额外调用分类模型。当前允许执行 `camera.inspect` 和 `conversation.exit`；音乐、音量、导航只完成识别并给出尚未接入的语音提示，不会操作电机或系统命令。

当前意图类型：

```text
camera.inspect
conversation.chat
conversation.exit
music.play
music.stop
volume.adjust
navigation.request
unknown
```

手动测试一条文字的意图分类：

```bash
~/.hermes/venv/bin/python \
  ~/.hermes/car_voice/hermes_voice_daemon.py \
  --classify-intent '帮我确认眼前是不是一把椅子'
```

日志中的 `Intent` 行会记录意图、置信度、来源和提取参数；`source=local_rule` 表示本地规则，`source=minimax` 表示 MiniMax 语义分类。

手动测试一段 16 kHz、单声道、16 位 PCM WAV：

```bash
~/.hermes/sensevoice-venv/bin/python \
  ~/.hermes/car_voice/sensevoice_transcribe.py \
  ~/jabra-mic-test.wav
```

当前唤醒口令：

```text
小车
```

为降低短词识别失败率，本地还接受 Whisper 常见的同音结果“晓车”“校车”“像车”“想车”“小撤”。当前处于链路调试阶段，服务设置了 `CAR_VOICE_WAKE_ON_ANY_SPEECH=true`，因此待机时检测到任何有效语音也会唤醒；链路稳定后可改成 `false`，恢复唤醒词匹配。听到“我在，请说”后即可提问。对话期间会续接同一个 Hermes 会话；30 秒没有检测到语音会自动返回待机。也可以说“再见”“退出对话”或“结束对话”主动结束。

### 开机自启和日常管理

服务已经设置为用户登录环境下开机自启，并启用了 systemd linger，因此退出 SSH 后仍会运行。

查看运行状态：

```bash
systemctl --user status hermes-car-voice.service
```

手动启动、停止和重启：

```bash
systemctl --user start hermes-car-voice.service
systemctl --user stop hermes-car-voice.service
systemctl --user restart hermes-car-voice.service
```

启用或取消开机自启：

```bash
systemctl --user enable --now hermes-car-voice.service
systemctl --user disable --now hermes-car-voice.service
```

实时查看语音识别、唤醒和模型调用日志：

```bash
journalctl --user -u hermes-car-voice.service -f
```

### 前台调试启动

前台调试前应先停止后台服务，避免两个进程同时占用麦克风：

```bash
systemctl --user stop hermes-car-voice.service
cd ~
~/.hermes/venv/bin/python ~/.hermes/car_voice/hermes_voice_daemon.py
```

按 `Ctrl+C` 退出前台进程，然后恢复后台服务：

```bash
systemctl --user start hermes-car-voice.service
```

### 部署位置

| 内容 | 树莓派路径 |
| --- | --- |
| 语音守护进程 | `~/.hermes/car_voice/hermes_voice_daemon.py` |
| SenseVoice 转写工具 | `~/.hermes/car_voice/sensevoice_transcribe.py` |
| 最新相机抓图 | `~/.hermes/run/car-voice/camera-latest.jpg` |
| systemd 服务 | `~/.config/systemd/user/hermes-car-voice.service` |
| Hermes 配置 | `~/.hermes/config.yaml` |
| API 密钥环境文件 | `~/.hermes/.env` |
| Whisper base 模型 | `~/.hermes/models/faster-whisper-base/` |

修改本仓库中的守护进程或服务文件后，可重新部署：

```bash
mkdir -p ~/.hermes/car_voice ~/.config/systemd/user
cp ~/small_car_f407/rpi_host/tools/hermes_voice_daemon.py \
  ~/.hermes/car_voice/hermes_voice_daemon.py
cp ~/small_car_f407/rpi_host/systemd/hermes-car-voice.service \
  ~/.config/systemd/user/hermes-car-voice.service
systemctl --user daemon-reload
systemctl --user enable --now hermes-car-voice.service
```

## 运行时参数调试

已确认的参数记录在 `config/chassis_params.yaml`，该文件应随代码提交到 Git。
上位机每次启动都会读取整份文件、批量下发全部参数并逐项回读校验：

```bash
./build/small_car_host_cli --port /dev/ttyACM0
```

`sensor_monitor` 启动时也会执行相同的加载和校验流程。

调整参数时只修改 `config/chassis_params.yaml` 并提交 Git。再次运行一键同步脚本后，
新值会自动同步到树莓派并应用到 MCU，不再提供单参数读写命令。

| ID | 含义 |
| ---: | --- |
| 1 | 里程计比例分子，分母固定为15600 |
| 2 | 手柄前进起步输出 |
| 3 | 手柄后退起步输出 |
| 4 | 手柄前后最大输出 |
| 5 | 手柄转向起步输出 |
| 6 | 手柄转向最大输出 |
| 7 | 超声避障距离阈值，单位mm |
| 8 | 陀螺仪灵敏度乘以10 |
| 9 | 有效轮距，单位mm；0表示关闭轮速航向融合 |
| 10 | 航向融合的陀螺仪权重，0-1000 |
| 11 | roll/pitch互补滤波的陀螺仪权重，0-1000 |
| 12 | IMU安装横滚偏置，单位mdeg |
| 13 | IMU安装俯仰偏置，单位mdeg |

# 同步并编译

源码在 Windows 电脑上修改后，在仓库根目录运行：

```powershell
.\scripts\sync_rpi_host.ps1
```

脚本会同步 `rpi_host` 源码，在树莓派上执行 CMake/CTest，并重建、重启 ROS2 bridge。默认目标为
`ubuntu@192.168.3.85:/home/ubuntu/small_car_f407/rpi_host`。
远端 `build` 是生成目录，每次同步后都会清理并重新构建，避免混用 Windows
和树莓派的 CMake 缓存。
