# Agent 语音对话链路

本文记录已验证的本地闭环：树莓派音频与图像经 ROS 2 触发 Agent，Agent 调用 WSL 中的
MiniCPM-o 4.5，并在终端打印文本回复。

```text
Jabra 麦克风、摄像头（树莓派）
  → /car/audio/input、/car/camera/image/compressed
  → small_car_agent：VAD
  → /car/agent/speech_finished
  → llm_agent：LangGraph
  → MiniCPM-o API（127.0.0.1:8000）
  → Agent 终端输出 MiniCPM-o 回复
```

## 运行前条件

- 树莓派 ROS 容器正在发布相机和 Jabra 麦克风 topic。
- WSL 已构建 `small_car_interfaces` 与 `small_car_agent`。
- 本地模型路径为 `/opt/models/MiniCPM-o-4_5-AWQ`。
- 三个终端使用相同的 `MINICPM_API_KEY`；密钥不要写入仓库或文档。

## 终端 1：启动 MiniCPM-o

```zsh
cd /mnt/d/work/smart_car/llm_agent
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY ALL_PROXY all_proxy
export NO_PROXY='127.0.0.1,localhost'
export no_proxy='127.0.0.1,localhost'
export MINICPM_API_KEY='<与模型服务相同的密钥>'
./scripts/start_minicpm.sh
```

保持该终端打开。出现 `Application startup complete` 表示服务可用。

另开终端检查：

```zsh
cd /mnt/d/work/smart_car/llm_agent
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY ALL_PROXY all_proxy
export NO_PROXY='127.0.0.1,localhost'
export no_proxy='127.0.0.1,localhost'
export MINICPM_API_KEY='<与模型服务相同的密钥>'
./scripts/status_minicpm.sh
```

正常时会输出 `minicpm-o-4.5-awq` 的模型列表。

## 终端 2：发布 ROS 语音事件

```zsh
source /opt/ros/kilted/setup.zsh
source /mnt/d/work/smart_car/robot_host/install-ros/setup.zsh
ros2 run small_car_agent agent_event_node
```

该节点订阅音视频，使用 VAD 判断一句话结束，并发布 `/car/agent/speech_finished`。

## 终端 3：启动 Agent

```zsh
cd /mnt/d/work/smart_car/llm_agent
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY ALL_PROXY all_proxy
export NO_PROXY='127.0.0.1,localhost'
export no_proxy='127.0.0.1,localhost'
export MINICPM_API_KEY='<与模型服务相同的密钥>'
./scripts/start_agent.sh
```

出现以下日志表示 Agent 已开始等待语音：

```text
Agent 已启动，等待 /car/agent/speech_finished
```

对 Jabra 麦克风说话，例如“hi 小车”，说完后保持约 1 秒静音。VAD 默认在至少 300 ms
语音、随后 600 ms 静音时触发。成功后终端会输出：

```text
MiniCPM-o：……
```

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `status_minicpm.sh` 返回 502 | WSL 代理拦截了 `127.0.0.1`，执行上述 `unset ...proxy` 和 `NO_PROXY` 设置后重试。 |
| `The URL must be either a HTTP, data or file URL` | Agent 版本过旧；当前版本会使用 `file://` 格式的临时 WAV。重启 Agent。 |
| `Cannot load local files without --allowed-local-media-path` | 模型服务版本过旧；当前 `start_minicpm.sh` 已放行受限目录 `/tmp`。停止并重新启动模型服务。 |
| Agent 一直等待事件 | 在终端 2 用 `ros2 topic echo /car/agent/speech_finished` 检查；同时确认树莓派正在发布 `/car/audio/input`。 |
| VAD 没有触发或触发过多 | 调整 ROS 节点参数 `vad_energy_threshold`、`vad_min_speech_ms` 和 `vad_silence_ms`。 |

模型仅允许读取 `/tmp` 下由 Agent 创建的单次临时音频文件；文件在请求结束后删除。
