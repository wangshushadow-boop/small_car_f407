# MiniCPM-o 原生语音输出

普通 `start_minicpm.sh` 使用 vLLM Chat Completions，只输出文本。原生语音使用独立的
vLLM-Omni 服务，响应同时包含文本和 24 kHz 单声道 WAV（`message.audio.data` Base64）。

## 安装

```zsh
cd /mnt/d/work/smart_car/llm_agent
chmod +x scripts/install_minicpm_omni.sh scripts/start_minicpm_omni.sh
./scripts/install_minicpm_omni.sh
```

安装会创建 `/opt/minicpm-omni/venv`，并下载完整模型到 `/opt/models/MiniCPM-o-4_5`。
不要使用当前 AWQ 目录替代完整 Omni 模型。
安装脚本以 `.omni_download_complete` 为下载完成标志；目录存在或已有 `config.json` 但文件不完整时会自动继续下载。

`vllm-omni` 还需要同版本的基础 `vllm` 运行时；安装脚本固定安装二者的 `0.26.0` 版本。

如果曾用旧脚本安装，需删除旧的 Omni 虚拟环境后重新执行安装，以消除它与 ROS/colcon
系统包之间的 `setuptools` 冲突：

```zsh
sudo rm -rf /opt/minicpm-omni/venv
./scripts/install_minicpm_omni.sh
```

默认使用清华 PyPI 与 `hf-mirror.com`。需要改用其他镜像时可在安装前设置：

```zsh
export PIP_INDEX_URL='https://你的-PyPI-镜像/simple'
export HF_ENDPOINT='https://你的-HuggingFace-镜像'
```

脚本默认设置 `HF_HUB_DISABLE_XET=1`，避免 Xet CAS 下载绕过国内镜像并出现 401；
中断后的下载会继续复用已完成分片。脚本默认下载超时为 600 秒，并在中断时最多自动重试 5 次。

## 启动

先停止普通 8000 端口 vLLM，然后运行：

```zsh
./scripts/start_minicpm_omni.sh
```

Omni 使用端口 `8099`。请求需要指定 `modalities: ["text", "audio"]` 和
`chat_template_kwargs.use_tts_template: true`；返回的 `message.audio.data` 即可解码为 WAV。

单张 RTX 3090 使用官方单卡配置，不能和普通 vLLM 并行运行。后续 Agent 将该 WAV 发布到
`/car/audio/output`，树莓派播放节点再输出到 Jabra。
