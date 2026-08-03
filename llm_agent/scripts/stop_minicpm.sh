#!/usr/bin/env bash
# 用途：停止由 start_minicpm.sh 启动的 MiniCPM-o 服务。
# 使用：./scripts/stop_minicpm.sh

set -euo pipefail

SERVICE_DIR="/opt/minicpm-service"
PROCESS_PATTERN="${SERVICE_DIR}/venv/bin/vllm serve /opt/models/MiniCPM-o-4_5-AWQ"

if pgrep -f "${PROCESS_PATTERN}" >/dev/null; then
  pkill -f "${PROCESS_PATTERN}"
  echo "已停止残留的 MiniCPM-o 进程。"
else
  echo "MiniCPM-o 当前未运行。"
fi
