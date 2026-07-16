# Hermes 语音助手完整恢复手册

本文用于树莓派系统、SD 卡或整机丢失后，从本项目重新部署 Hermes 常驻语音助手。仓库中不保存真实 API Key、Hermes 运行数据、Python 虚拟环境或 Whisper 模型；这些内容按照本文重新生成或下载。

## 1. 恢复目标

恢复后的链路：

```text
USB 麦克风
  → 本地音量检测和语音分段
  → Whisper tiny 待机识别
  → 宽松唤醒（调试阶段检测到有效语音即可唤醒）
  → Whisper base 对话识别
  → Hermes Agent / MiniMax M3
  → MiniMax TTS
  → USB 扬声器
```

项目中需要长期保留的文件：

| 文件 | 用途 |
| --- | --- |
| `rpi_host/tools/hermes_voice_daemon.py` | 常驻语音守护进程。 |
| `rpi_host/systemd/hermes-car-voice.service` | systemd 用户服务。 |
| `rpi_host/README.md` | 日常启动、停止和日志命令。 |
| `rpi_host/hermes-voice-recovery.md` | 本恢复手册。 |

当前固定配置：

| 项目 | 值 |
| --- | --- |
| Linux 用户 | `ubuntu` |
| Hermes Agent | `0.18.2` |
| Python | `3.11` |
| 推理服务商 | `minimax-cn` |
| 对话模型 | `MiniMax-M3` |
| MiniMax Anthropic 地址 | `https://api.minimaxi.com/anthropic` |
| TTS 地址 | `https://api.minimaxi.com/v1/t2a_v2` |
| TTS 模型 | `speech-2.8-hd` |
| TTS 音色 | `male-qn-qingse` |
| 待机 STT | `faster-whisper-tiny` |
| 对话 STT | `faster-whisper-base` |
| USB 声卡 | ALSA card `0`，`hw:0,0` / `plughw:0,0` |
| 采样格式 | 48 kHz、单声道录音、16 位 PCM |
| 当前唤醒方式 | 调试阶段检测到有效语音即可唤醒，推荐说“小车”。 |

## 2. 系统准备

建议使用 64 位 Raspberry Pi OS 或 Debian，主机名和用户名可以不同，但当前 service 文件中的 `/home/ubuntu` 需要相应修改。

确认架构和 USB 外设：

```bash
uname -m
arecord -l
aplay -l
lsusb
```

预期 `uname -m` 返回 `aarch64`，USB 音频设备同时出现在 `arecord -l` 和 `aplay -l` 中。

安装系统依赖：

```bash
sudo apt-get update
sudo apt-get install -y \
  curl \
  ffmpeg \
  git \
  libportaudio2 \
  portaudio19-dev \
  ripgrep
```

## 3. 安装 uv、Python 和 Hermes

安装 uv：

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
uv --version
```

创建独立 Python 3.11 环境并安装固定版本 Hermes：

```bash
mkdir -p ~/.hermes ~/.local/bin
chmod 700 ~/.hermes

uv python install 3.11
uv venv ~/.hermes/venv --python 3.11
uv pip install \
  --python ~/.hermes/venv/bin/python \
  --index-url https://pypi.tuna.tsinghua.edu.cn/simple \
  'hermes-agent[voice]==0.18.2' \
  'edge-tts==7.2.7'

ln -sfn ~/.hermes/venv/bin/hermes ~/.local/bin/hermes
~/.hermes/venv/bin/hermes --version
```

预期版本：

```text
Hermes Agent v0.18.2
Python 3.11.x
```

## 4. 安全写入 MiniMax 密钥

不要把真实 Key 写入本仓库、README、Git 历史或 shell 命令历史。使用隐藏输入生成 `~/.hermes/.env`：

```bash
read -r -s -p 'MiniMax API Key: ' MINIMAX_KEY
printf '\n'

install -m 600 /dev/null ~/.hermes/.env
{
  printf 'MINIMAX_CN_API_KEY=%s\n' "$MINIMAX_KEY"
  printf 'MINIMAX_API_KEY=%s\n' "$MINIMAX_KEY"
  printf 'MINIMAX_CN_BASE_URL=%s\n' 'https://api.minimaxi.com/anthropic'
  printf 'HF_ENDPOINT=%s\n' 'https://hf-mirror.com'
  printf 'HF_HUB_DISABLE_XET=%s\n' '1'
} > ~/.hermes/.env

chmod 600 ~/.hermes/.env
unset MINIMAX_KEY
```

检查权限，不要输出文件内容：

```bash
stat -c '%a %n' ~/.hermes ~/.hermes/.env
```

预期分别为 `700` 和 `600`。

## 5. 配置 Hermes、MiniMax M3 和语音服务

```bash
HERMES=~/.hermes/venv/bin/hermes

$HERMES config set model.provider minimax-cn
$HERMES config set model.default MiniMax-M3

$HERMES config set stt.provider local
$HERMES config set stt.local.model ~/.hermes/models/faster-whisper-base
$HERMES config set stt.local.language zh

$HERMES config set tts.provider minimax
$HERMES config set tts.minimax.base_url https://api.minimaxi.com/v1/t2a_v2
$HERMES config set tts.minimax.model speech-2.8-hd
$HERMES config set tts.minimax.voice_id male-qn-qingse
$HERMES config set voice.auto_tts true

$HERMES config check
```

如果 `config check` 提示配置版本需要迁移，执行：

```bash
$HERMES config migrate
```

迁移程序询问未使用的可选服务密钥时选择 `n`。

执行真实模型连通性测试：

```bash
$HERMES -z '只回复 RESTORE_OK'
```

预期返回：

```text
RESTORE_OK
```

## 6. 下载 Whisper 模型

Hugging Face 在部分网络环境下访问较慢，当前使用镜像直接下载完整模型目录。

```bash
mkdir -p \
  ~/.hermes/models/faster-whisper-tiny \
  ~/.hermes/models/faster-whisper-base

for model in tiny base; do
  destination="$HOME/.hermes/models/faster-whisper-$model"
  for file in config.json model.bin tokenizer.json vocabulary.txt; do
    curl -fL \
      --retry 8 \
      --retry-delay 2 \
      --connect-timeout 15 \
      -o "$destination/$file" \
      "https://hf-mirror.com/Systran/faster-whisper-$model/resolve/main/$file"
  done
done
```

检查目录大小：

```bash
du -sh \
  ~/.hermes/models/faster-whisper-tiny \
  ~/.hermes/models/faster-whisper-base
```

参考大小：tiny 约 `75 MB`，base 约 `142 MB`。

## 7. 从项目部署守护进程

假设项目已经恢复到：

```text
/home/ubuntu/small_car_f407
```

部署代码与服务：

```bash
mkdir -p ~/.hermes/car_voice ~/.config/systemd/user

install -m 700 \
  ~/small_car_f407/rpi_host/tools/hermes_voice_daemon.py \
  ~/.hermes/car_voice/hermes_voice_daemon.py

install -m 600 \
  ~/small_car_f407/rpi_host/systemd/hermes-car-voice.service \
  ~/.config/systemd/user/hermes-car-voice.service

~/.hermes/venv/bin/python -m py_compile \
  ~/.hermes/car_voice/hermes_voice_daemon.py
```

如果恢复时用户名不是 `ubuntu`，先修改 `hermes-car-voice.service` 和 `hermes_voice_daemon.py` 中的 `/home/ubuntu`。

## 8. 配置 USB 麦克风和扬声器

查看声卡控制项：

```bash
amixer -c 0 scontrols
amixer -c 0 contents
```

将麦克风调到硬件最大，开启自动增益，并把扬声器设置为当前验证值：

```bash
amixer -c 0 sset Mic 147
amixer -c 0 sset 'Auto Gain Control' on
amixer -c 0 sset PCM 110
sudo alsactl store
```

录音测试：

```bash
arecord -D hw:0,0 -f S16_LE -r 48000 -c 1 -d 5 /tmp/mic-test.wav
```

播放测试：

```bash
aplay -D plughw:0,0 /tmp/mic-test.wav
```

## 9. 安装并启动 systemd 服务

先安装 Hermes 网关服务：

```bash
~/.hermes/venv/bin/hermes gateway install \
  --force \
  --start-now \
  --start-on-login
```

允许用户服务在退出 SSH 后继续运行：

```bash
sudo loginctl enable-linger ubuntu
```

安装并启动小车语音服务：

```bash
systemctl --user daemon-reload
systemctl --user enable --now hermes-car-voice.service
```

如果旧系统安装过 OpenClaw，避免两个网关同时运行：

```bash
systemctl --user disable --now openclaw-gateway.service || true
```

确认状态：

```bash
systemctl --user is-active hermes-gateway.service
systemctl --user is-enabled hermes-gateway.service
systemctl --user is-active hermes-car-voice.service
systemctl --user is-enabled hermes-car-voice.service
```

四项应依次返回 `active`、`enabled`、`active`、`enabled`。

## 10. 现场验收

打开实时日志：

```bash
journalctl --user -u hermes-car-voice.service -f
```

当前为宽松调试模式，对麦克风说一段话即可触发，推荐先说：

```text
小车
```

预期流程：

1. 日志出现 `Speech detected`。
2. 待机 Whisper tiny 完成本地转写。
3. 日志出现 `Wake accepted`。
4. 扬声器播放“我在，请说”。
5. 继续提问，例如“请介绍一下你自己”。
6. Whisper base 转写问题。
7. 日志出现 `Calling Hermes` 和 `Hermes response`。
8. MiniMax TTS 生成语音并从 USB 扬声器播放。

当前调试模式配置位于 `hermes-car-voice.service`：

```ini
Environment=CAR_VOICE_WAKE_ON_ANY_SPEECH=true
```

该模式可能被环境人声误触发并产生 MiniMax 调用费用。链路稳定后将其改为 `false`，再执行：

```bash
systemctl --user daemon-reload
systemctl --user restart hermes-car-voice.service
```

## 11. 常用维护命令

```bash
# 状态
systemctl --user status hermes-car-voice.service

# 重启
systemctl --user restart hermes-car-voice.service

# 停止
systemctl --user stop hermes-car-voice.service

# 最近日志
journalctl --user -u hermes-car-voice.service -n 100 --no-pager

# 实时日志
journalctl --user -u hermes-car-voice.service -f

# Hermes 文本连通性
~/.hermes/venv/bin/hermes -z '只回复 OK'
```

## 12. 不应提交到 Git 的内容

以下内容包含密钥、运行状态或大型下载文件，不应加入仓库：

```text
~/.hermes/.env
~/.hermes/config.yaml
~/.hermes/state.db
~/.hermes/sessions/
~/.hermes/models/
~/.hermes/venv/
```

应当提交并备份本项目中的 Python 守护进程、systemd service、README 和本恢复手册。只有当这些文件已经提交并推送到远端 Git 仓库或其他备份介质后，才真正具备树莓派丢失后的恢复能力。
