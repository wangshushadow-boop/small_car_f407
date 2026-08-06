#!/usr/bin/env bash
# 启动 MiniCPM-o 原生语音输出服务（文本 + 24 kHz WAV）。
set -euo pipefail

OMNI_BIN="/opt/minicpm-omni/venv/bin/vllm-omni"
MODEL_DIR="/opt/models/MiniCPM-o-4_5"
MODEL_READY_FILE="${MODEL_DIR}/.omni_download_complete"
DEPLOY_CONFIG="/opt/minicpm-omni/venv/lib/python3.12/site-packages/vllm_omni/deploy/minicpmo_4_5.yaml"

if [[ ! -x "${OMNI_BIN}" || ! -f "${MODEL_READY_FILE}" || ! -f "${DEPLOY_CONFIG}" ]]; then
  echo "错误：Omni 环境或完整模型未安装。请先运行 ./scripts/install_minicpm_omni.sh"
  exit 1
fi
if pgrep -f '/opt/minicpm-service/venv/bin/vllm serve' >/dev/null; then
  echo "错误：普通 MiniCPM vLLM（端口 8000）仍在运行。请先停止它，3090 单卡不能并行运行 Omni。"
  exit 1
fi

exec "${OMNI_BIN}" serve "${MODEL_DIR}" \
  --omni \
  --deploy-config "${DEPLOY_CONFIG}" \
  --trust-remote-code \
  --host 0.0.0.0 \
  --port 8099
