#!/usr/bin/env bash
# 安装 MiniCPM-o 原生语音输出所需的独立 vLLM-Omni 环境。
set -euo pipefail

OMNI_ROOT="/opt/minicpm-omni"
OMNI_VENV="${OMNI_ROOT}/venv"
MODEL_DIR="/opt/models/MiniCPM-o-4_5"
MODEL_READY_FILE="${MODEL_DIR}/.omni_download_complete"
PIP_INDEX_URL="${PIP_INDEX_URL:-https://pypi.tuna.tsinghua.edu.cn/simple}"
HF_ENDPOINT="${HF_ENDPOINT:-https://hf-mirror.com}"
# hf-mirror 不代理 Hugging Face 的 Xet CAS 域名；禁用 Xet 以确保所有模型文件
# 均通过上述镜像以普通 HTTP 下载，且已有分片可以续传。
HF_HUB_DISABLE_XET="${HF_HUB_DISABLE_XET:-1}"
HF_HUB_DOWNLOAD_TIMEOUT="${HF_HUB_DOWNLOAD_TIMEOUT:-600}"

echo "此安装会下载完整 MiniCPM-o 4.5 权重与 Omni 依赖，请确认磁盘和网络充足。"
sudo apt-get update
sudo apt-get install -y ffmpeg libsndfile1 python3-venv
sudo mkdir -p "${OMNI_ROOT}" /opt/models
sudo chown -R "${USER}:${USER}" "${OMNI_ROOT}" /opt/models

# Omni 与 ROS/colcon 使用完全隔离的虚拟环境，避免 setuptools 版本互相影响。
python3 -m venv "${OMNI_VENV}"
"${OMNI_VENV}/bin/pip" install --index-url "${PIP_INDEX_URL}" --upgrade pip
# vllm-omni 本身依赖基础 vLLM，但不会自动安装该运行时。
# 必须保持二者同一 0.26 系列，避免配置接口不兼容。
"${OMNI_VENV}/bin/pip" install --index-url "${PIP_INDEX_URL}" \
  "vllm==0.26.0" "vllm-omni==0.26.0" stepaudio2-minicpmo huggingface_hub

if [[ ! -f "${MODEL_READY_FILE}" ]]; then
  echo "正在下载或补全完整 MiniCPM-o 4.5 模型：${MODEL_DIR}"
  for attempt in 1 2 3 4 5; do
    if HF_ENDPOINT="${HF_ENDPOINT}" HF_HUB_DISABLE_XET="${HF_HUB_DISABLE_XET}" \
      HF_HUB_DOWNLOAD_TIMEOUT="${HF_HUB_DOWNLOAD_TIMEOUT}" \
      "${OMNI_VENV}/bin/hf" download openbmb/MiniCPM-o-4_5 --local-dir "${MODEL_DIR}"; then
      touch "${MODEL_READY_FILE}"
      break
    fi
    if [[ "${attempt}" == "5" ]]; then
      echo "模型下载连续失败 5 次；已下载内容保留，可稍后重新运行本脚本续传。"
      exit 1
    fi
    echo "下载中断，30 秒后第 ${attempt} 次重试（将续传已有分片）。"
    sleep 30
  done
fi

echo "安装完成。先停止 8000 端口的普通 vLLM，再运行 scripts/start_minicpm_omni.sh。"
