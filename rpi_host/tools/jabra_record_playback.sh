#!/usr/bin/env bash

set -euo pipefail

readonly VOICE_SERVICE="hermes-car-voice.service"
readonly CAPTURE_DEVICE="hw:CARD=USB,DEV=0"
readonly PLAYBACK_DEVICE="plughw:CARD=USB,DEV=0"
readonly CAPTURE_RATE=16000
readonly PLAYBACK_RATE=48000

seconds="${1:-5}"
output="${2:-${HOME}/jabra-mic-test.wav}"
playback_wav="${output%.wav}-48k.wav"
service_was_active=false

if [[ ! "${seconds}" =~ ^[0-9]+$ ]] || ((seconds < 1 || seconds > 60)); then
  echo "用法: $0 [录音秒数 1-60] [输出 WAV 路径]" >&2
  exit 2
fi

restore_service() {
  if [[ "${service_was_active}" == true ]]; then
    echo "[恢复] 启动 ${VOICE_SERVICE}"
    systemctl --user start "${VOICE_SERVICE}"
  fi
}
trap restore_service EXIT INT TERM

if systemctl --user is-active --quiet "${VOICE_SERVICE}"; then
  service_was_active=true
  echo "[准备] 暂停 ${VOICE_SERVICE}，释放 Jabra 麦克风"
  systemctl --user stop "${VOICE_SERVICE}"
fi

mkdir -p "$(dirname "${output}")"

echo "[录音] 1 秒后开始，请对着 Jabra 说话；录制 ${seconds} 秒"
sleep 1
arecord \
  -q \
  -D "${CAPTURE_DEVICE}" \
  -t wav \
  -f S16_LE \
  -r "${CAPTURE_RATE}" \
  -c 1 \
  -d "${seconds}" \
  "${output}"

echo "[转换] ${CAPTURE_RATE} Hz 单声道 -> ${PLAYBACK_RATE} Hz 双声道"
ffmpeg \
  -y \
  -hide_banner \
  -loglevel error \
  -i "${output}" \
  -ar "${PLAYBACK_RATE}" \
  -ac 2 \
  -c:a pcm_s16le \
  "${playback_wav}"

echo "[播放] 现在从 Jabra 扬声器回放录音"
aplay -q -D "${PLAYBACK_DEVICE}" "${playback_wav}"

echo "[完成] 原始录音: ${output}"
echo "[完成] 播放文件: ${playback_wav}"
