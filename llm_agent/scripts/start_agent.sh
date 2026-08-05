#!/usr/bin/env bash
# 启动事件驱动的小车 Agent：ROS SpeechEvent → LangGraph → MiniCPM-o。
set -eo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
source /opt/ros/kilted/setup.bash
source "${root}/ros2_host/install-ros/setup.bash"
set -u
export PYTHONPATH="${root}:${PYTHONPATH:-}"
exec /opt/minicpm-service/venv/bin/python -m llm_agent.agent.run_agent
