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
| `hermes-voice-recovery.md` | 树莓派丢失或重装后，完整恢复 Hermes 语音助手的步骤。 |

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

## Hermes 常驻语音助手

语音助手在树莓派上以 systemd 用户服务运行，启动链路如下：

```text
USB 麦克风 → 本地 VAD → faster-whisper-base → 本地唤醒词匹配
→ Hermes / MiniMax M3 → MiniMax TTS → USB 扬声器
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
