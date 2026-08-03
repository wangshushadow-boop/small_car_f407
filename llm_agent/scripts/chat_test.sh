#!/usr/bin/env bash
# 用途：向本机 MiniCPM-o 发送一条最小文本对话，验证真实推理链路。
# 使用：export MINICPM_API_KEY='启动时的密钥'; ./scripts/chat_test.sh

set -euo pipefail

if [[ -z "${MINICPM_API_KEY:-}" ]]; then
  echo "错误：请设置与启动服务时相同的 MINICPM_API_KEY。"
  exit 1
fi

curl --fail --silent --show-error \
  -H "Authorization: Bearer ${MINICPM_API_KEY}" \
  -H 'Content-Type: application/json' \
  --data-binary @- \
  http://127.0.0.1:8000/v1/chat/completions <<'JSON'
{
  "model": "minicpm-o-4.5-awq",
  "messages": [
    {"role": "user", "content": "请用一句中文确认本地模型推理正常。"}
  ],
  "max_tokens": 80,
  "temperature": 0.1
}
JSON
echo

