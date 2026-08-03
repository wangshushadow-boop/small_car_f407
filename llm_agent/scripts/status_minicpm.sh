#!/usr/bin/env bash
# 用途：检查 MiniCPM-o 的进程、HTTP 健康状态和可用模型。
# 使用：export MINICPM_API_KEY='启动时的密钥'; ./scripts/status_minicpm.sh

set -euo pipefail

if [[ -z "${MINICPM_API_KEY:-}" ]]; then
  echo "错误：请设置与启动服务时相同的 MINICPM_API_KEY。"
  exit 1
fi

echo "模型进程："
pgrep -af '/opt/minicpm-service/venv/bin/vllm serve' || true

echo
echo "API 模型列表："
curl --fail --silent --show-error \
  -H "Authorization: Bearer ${MINICPM_API_KEY}" \
  http://127.0.0.1:8000/v1/models
echo

