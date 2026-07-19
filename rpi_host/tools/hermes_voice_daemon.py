#!/usr/bin/env python3
"""Always-on, wake-phrase voice loop for Hermes Agent on Raspberry Pi."""

from __future__ import annotations

import json
import logging
import os
import re
import signal
import subprocess
import tempfile
import time
import wave
from collections import deque
from pathlib import Path

import numpy as np
import sounddevice as sd

from tools.transcription_tools import transcribe_audio
from tools.tts_tool import text_to_speech_tool


LOG = logging.getLogger("hermes-car-voice")
STOP_REQUESTED = False

SAMPLE_RATE = int(os.getenv("CAR_VOICE_SAMPLE_RATE", "16000"))
PLAYBACK_SAMPLE_RATE = int(
    os.getenv("CAR_VOICE_PLAYBACK_SAMPLE_RATE", "48000")
)
BLOCK_MS = int(os.getenv("CAR_VOICE_BLOCK_MS", "100"))
BLOCK_SIZE = SAMPLE_RATE * BLOCK_MS // 1000
INPUT_DEVICE = int(os.getenv("CAR_VOICE_INPUT_DEVICE", "0"))
RMS_THRESHOLD = int(os.getenv("CAR_VOICE_RMS_THRESHOLD", "250"))
SPEECH_CONFIRM_MS = int(os.getenv("CAR_VOICE_SPEECH_CONFIRM_MS", "300"))
END_SILENCE_MS = int(os.getenv("CAR_VOICE_END_SILENCE_MS", "1200"))
IDLE_TIMEOUT_SECONDS = float(os.getenv("CAR_VOICE_IDLE_TIMEOUT_SECONDS", "30"))
ACTIVE_TIMEOUT_SECONDS = float(os.getenv("CAR_VOICE_ACTIVE_TIMEOUT_SECONDS", "90"))
MAX_UTTERANCE_SECONDS = float(os.getenv("CAR_VOICE_MAX_UTTERANCE_SECONDS", "20"))
PLAYBACK_DEVICE = os.getenv(
    "CAR_VOICE_PLAYBACK_DEVICE", "plughw:CARD=USB,DEV=0"
)
STT_PROVIDER = os.getenv("CAR_VOICE_STT_PROVIDER", "sensevoice")
SENSEVOICE_PYTHON = os.getenv(
    "CAR_VOICE_SENSEVOICE_PYTHON",
    "/home/ubuntu/.hermes/sensevoice-venv/bin/python",
)
SENSEVOICE_SCRIPT = os.getenv(
    "CAR_VOICE_SENSEVOICE_SCRIPT",
    "/home/ubuntu/.hermes/car_voice/sensevoice_transcribe.py",
)
CAMERA_CAPTURE_BIN = os.getenv(
    "CAR_VOICE_CAMERA_CAPTURE_BIN",
    "/home/ubuntu/small_car_f407/rpi_host/build-v4l2/v4l2_capture",
)
CAMERA_DEVICE = os.getenv("CAR_VOICE_CAMERA_DEVICE", "/dev/video0")
CAMERA_WIDTH = int(os.getenv("CAR_VOICE_CAMERA_WIDTH", "1280"))
CAMERA_HEIGHT = int(os.getenv("CAR_VOICE_CAMERA_HEIGHT", "720"))
CAMERA_IMAGE = Path(
    os.getenv(
        "CAR_VOICE_CAMERA_IMAGE",
        "/home/ubuntu/.hermes/run/car-voice/camera-latest.jpg",
    )
)
HERMES_BIN = os.getenv("HERMES_BIN", "/home/ubuntu/.hermes/venv/bin/hermes")
RUN_DIR = Path(os.getenv("CAR_VOICE_RUN_DIR", "/home/ubuntu/.hermes/run/car-voice"))
WAKE_STT_MODEL = os.getenv(
    "CAR_VOICE_WAKE_STT_MODEL",
    "/home/ubuntu/.hermes/models/faster-whisper-tiny",
)
WAKE_ON_ANY_SPEECH = os.getenv(
    "CAR_VOICE_WAKE_ON_ANY_SPEECH", "true"
).lower() in {"1", "true", "yes", "on"}

WAKE_PHRASES = tuple(
    phrase.strip()
    for phrase in os.getenv(
        "CAR_VOICE_WAKE_PHRASES", "小车,晓车,校车,像车,想车,小撤"
    ).split(",")
    if phrase.strip()
)
EXIT_PHRASES = tuple(
    phrase.strip()
    for phrase in os.getenv(
        "CAR_VOICE_EXIT_PHRASES", "再见,退出对话,结束对话,休息吧,不用了"
    ).split(",")
    if phrase.strip()
)
VISION_PHRASES = tuple(
    phrase.strip()
    for phrase in os.getenv(
        "CAR_VOICE_VISION_PHRASES",
        "看看前面,看一下前面,看下前面,前面有什么,你看到了什么,"
        "你看到了啥,看见什么,看见啥,能看到什么,能看到啥,眼前有什么,"
        "看看,看一下,看一看,拍张照片,拍照,重新看一下,重新看,"
        "打开相机,调用相机,摄像头,相机",
    ).split(",")
    if phrase.strip()
)


def request_stop(_signum: int, _frame: object) -> None:
  global STOP_REQUESTED
  STOP_REQUESTED = True


def normalize_text(text: str) -> str:
  return re.sub(r"[^\w\u4e00-\u9fff]", "", text).lower()


def contains_phrase(text: str, phrases: tuple[str, ...]) -> bool:
  normalized = normalize_text(text)
  return any(normalize_text(phrase) in normalized for phrase in phrases)


def is_vision_request(text: str) -> bool:
  """Detect colloquial Chinese requests that need a fresh camera frame."""
  if contains_phrase(text, VISION_PHRASES):
    return True

  normalized = normalize_text(text)
  visual_actions = ("看", "看到", "看见", "瞧", "拍", "观察", "识别")
  visual_targets = (
      "什么",
      "啥",
      "前面",
      "眼前",
      "周围",
      "画面",
      "图像",
      "图片",
      "照片",
      "这个",
      "那个",
      "哪里",
      "哪儿",
      "多远",
      "距离",
  )
  has_action = any(term in normalized for term in visual_actions)
  has_target = any(term in normalized for term in visual_targets)
  distance_request = any(term in normalized for term in ("多远", "距离")) and any(
      term in normalized for term in ("这", "那", "前面", "眼前")
  )
  return (has_action and has_target) or distance_request


def capture_camera() -> Path | None:
  """Capture one MJPEG frame from the USB camera."""
  CAMERA_IMAGE.parent.mkdir(parents=True, exist_ok=True)
  command = [
      CAMERA_CAPTURE_BIN,
      CAMERA_DEVICE,
      "mjpg",
      str(CAMERA_IMAGE),
      str(CAMERA_WIDTH),
      str(CAMERA_HEIGHT),
  ]
  LOG.info(
      "Capturing camera image: device=%s, size=%sx%s",
      CAMERA_DEVICE,
      CAMERA_WIDTH,
      CAMERA_HEIGHT,
  )
  try:
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        timeout=20,
        check=False,
    )
  except (OSError, subprocess.SubprocessError) as error:
    LOG.error("Camera capture failed: %s", error)
    return None

  if completed.returncode != 0 or not CAMERA_IMAGE.is_file():
    detail = completed.stderr.strip() or completed.stdout.strip()
    LOG.error("Camera capture failed: %s", detail or "unknown error")
    return None

  CAMERA_IMAGE.chmod(0o600)
  LOG.info("Camera image ready: %s", CAMERA_IMAGE)
  return CAMERA_IMAGE


def save_wav(path: Path, samples: np.ndarray) -> None:
  with wave.open(str(path), "wb") as wav_file:
    wav_file.setnchannels(1)
    wav_file.setsampwidth(2)
    wav_file.setframerate(SAMPLE_RATE)
    wav_file.writeframes(samples.astype(np.int16).tobytes())


def record_utterance(wait_timeout: float) -> Path | None:
  """Wait for speech and record until silence using a small pre-roll buffer."""
  pre_roll_blocks = max(1, 500 // BLOCK_MS)
  confirm_blocks = max(1, SPEECH_CONFIRM_MS // BLOCK_MS)
  end_silence_blocks = max(1, END_SILENCE_MS // BLOCK_MS)
  max_blocks = max(1, int(MAX_UTTERANCE_SECONDS * 1000 // BLOCK_MS))
  pre_roll: deque[np.ndarray] = deque(maxlen=pre_roll_blocks)
  frames: list[np.ndarray] = []
  voiced_run = 0
  silence_run = 0
  started = False
  deadline = time.monotonic() + wait_timeout

  with sd.InputStream(
      device=INPUT_DEVICE,
      samplerate=SAMPLE_RATE,
      channels=1,
      dtype="int16",
      blocksize=BLOCK_SIZE,
  ) as stream:
    while not STOP_REQUESTED and time.monotonic() < deadline:
      data, overflowed = stream.read(BLOCK_SIZE)
      if overflowed:
        LOG.warning("Audio input overflow")
      block = data[:, 0].copy()
      rms = float(np.sqrt(np.mean(block.astype(np.float32) ** 2)))
      is_voiced = rms >= RMS_THRESHOLD

      if not started:
        pre_roll.append(block)
        voiced_run = voiced_run + 1 if is_voiced else 0
        if voiced_run >= confirm_blocks:
          started = True
          frames.extend(pre_roll)
          LOG.info("Speech detected (RMS %.0f)", rms)
        continue

      frames.append(block)
      silence_run = 0 if is_voiced else silence_run + 1
      if silence_run >= end_silence_blocks or len(frames) >= max_blocks:
        break

  if not started or not frames:
    return None

  samples = np.concatenate(frames)
  RUN_DIR.mkdir(parents=True, exist_ok=True)
  path = RUN_DIR / f"utterance-{int(time.time() * 1000)}.wav"
  save_wav(path, samples)
  return path


def transcribe(path: Path, model: str | None = None) -> str:
  if STT_PROVIDER == "sensevoice":
    try:
      completed = subprocess.run(
          [SENSEVOICE_PYTHON, SENSEVOICE_SCRIPT, str(path)],
          capture_output=True,
          text=True,
          timeout=60,
          check=False,
      )
      result = json.loads(completed.stdout)
      if completed.returncode != 0 or not result.get("success"):
        LOG.error(
            "SenseVoice failed: %s",
            result.get("error", completed.stderr.strip() or "unknown error"),
        )
        return ""
      text = str(result.get("text", "")).strip()
      if not normalize_text(text):
        text = ""
      LOG.info(
          "SenseVoice transcript (%.3fs): %s",
          float(result.get("elapsed_seconds", 0)),
          text or "<empty>",
      )
      return text
    except (json.JSONDecodeError, OSError, subprocess.SubprocessError) as error:
      LOG.error("SenseVoice failed: %s", error)
      return ""

  result = transcribe_audio(str(path), model=model)
  if not result.get("success"):
    LOG.error("STT failed: %s", result.get("error", "unknown error"))
    return ""
  text = str(result.get("transcript", "")).strip()
  LOG.info("Transcript: %s", text or "<empty>")
  return text


def parse_hermes_output(stdout: str, stderr: str = "") -> tuple[str, str | None]:
  ansi_escape = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
  clean = ansi_escape.sub("", stdout).strip()
  session_id = None
  response_lines: list[str] = []
  for line in (clean + "\n" + ansi_escape.sub("", stderr)).splitlines():
    stripped = line.strip()
    if stripped.startswith("session_id:"):
      session_id = stripped.split(":", 1)[1].strip()
    elif "tirith security scanner" in stripped:
      continue
    elif stripped.startswith("↻ Resumed session"):
      continue
    elif not stripped.startswith("session_title:"):
      response_lines.append(line)
  return "\n".join(response_lines).strip(), session_id


def ask_hermes(
    text: str, session_id: str | None, image_path: Path | None = None
) -> tuple[str, str | None]:
  user_prompt = text
  if image_path is not None:
    user_prompt = (
        "本轮附带的是小车刚刚拍摄的前方相机图片。请直接观察图片，"
        "结合用户的问题简短回答，不要声称无法访问相机。\n"
        f"用户说：{text}"
    )
  prompt = user_prompt
  command = [
      HERMES_BIN,
      "chat",
      "-q",
      prompt,
      "-Q",
      "--source",
      "tool",
      "--max-turns",
      "12",
  ]
  if image_path is not None:
    command.extend(["--image", str(image_path)])
  if session_id:
    command.extend(["--resume", session_id])
  else:
    prompt = (
        "你是安装在智能小车上的中文语音助手。回答应简短、自然、适合直接朗读，"
        "不要使用 Markdown。当前阶段禁止控制电机或 STM32，但可以观察本轮附带的"
        "小车相机图片。\n"
        f"{user_prompt}"
    )
    command[3] = prompt

  LOG.info(
      "Calling Hermes%s%s",
      f" session {session_id}" if session_id else "",
      " with camera image" if image_path is not None else "",
  )
  try:
    completed = subprocess.run(
        command,
        cwd="/home/ubuntu",
        capture_output=True,
        text=True,
        timeout=180,
        check=False,
    )
  except subprocess.TimeoutExpired:
    LOG.error("Hermes request timed out")
    return "请求超时了，请再说一次。", session_id

  if completed.returncode != 0:
    LOG.error("Hermes failed: %s", completed.stderr.strip())
    return "连接模型失败了，请稍后再试。", session_id

  response, new_session_id = parse_hermes_output(
      completed.stdout, completed.stderr
  )
  if not response:
    response = "我没有听清楚，请再说一次。"
  LOG.info("Hermes response: %s", response)
  return response, new_session_id or session_id


def speak(text: str) -> None:
  RUN_DIR.mkdir(parents=True, exist_ok=True)
  mp3_path = RUN_DIR / "reply.mp3"
  wav_path = RUN_DIR / "reply.wav"
  try:
    result = json.loads(text_to_speech_tool(text, str(mp3_path)))
    if not result.get("success"):
      raise RuntimeError(result.get("error", "TTS failed"))
    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(mp3_path),
            "-ar",
            str(PLAYBACK_SAMPLE_RATE),
            "-ac",
            "2",
            str(wav_path),
        ],
        check=True,
        timeout=60,
    )
    subprocess.run(
        ["aplay", "-q", "-D", PLAYBACK_DEVICE, str(wav_path)],
        check=True,
        timeout=60,
    )
  except (json.JSONDecodeError, OSError, RuntimeError, subprocess.SubprocessError) as error:
    LOG.error("TTS/playback failed: %s", error)


def conversation_loop(
    initial_text: str | None = None, initial_image: Path | None = None
) -> None:
  session_id: str | None = None
  if initial_text:
    response, session_id = ask_hermes(initial_text, session_id, initial_image)
    speak(response)
  else:
    speak("我在，请说。")
  active_deadline = time.monotonic() + ACTIVE_TIMEOUT_SECONDS

  while not STOP_REQUESTED and time.monotonic() < active_deadline:
    path = record_utterance(IDLE_TIMEOUT_SECONDS)
    if path is None:
      LOG.info("Conversation idle timeout; returning to wake mode")
      speak("我先休息了，需要时再叫我。")
      return

    text = transcribe(path)
    path.unlink(missing_ok=True)
    if not text:
      continue
    if contains_phrase(text, EXIT_PHRASES):
      speak("好的，再见。")
      return

    active_deadline = time.monotonic() + ACTIVE_TIMEOUT_SECONDS
    image_path = None
    if is_vision_request(text):
      image_path = capture_camera()
      if image_path is None:
        speak("相机抓图失败了，请检查相机连接。")
        continue
    response, session_id = ask_hermes(text, session_id, image_path)
    speak(response)

  if not STOP_REQUESTED:
    LOG.info("Conversation maximum active time reached")
    speak("这次对话先到这里，需要时再叫我。")


def main() -> int:
  logging.basicConfig(
      level=os.getenv("CAR_VOICE_LOG_LEVEL", "INFO"),
      format="%(asctime)s %(levelname)s %(message)s",
  )
  signal.signal(signal.SIGTERM, request_stop)
  signal.signal(signal.SIGINT, request_stop)
  RUN_DIR.mkdir(parents=True, exist_ok=True)

  LOG.info(
      "Voice daemon ready: input=%s at %s Hz, playback=%s at %s Hz, "
      "stt=%s, camera=%s, threshold=%s, wake=%s, wake_on_any_speech=%s",
      INPUT_DEVICE,
      SAMPLE_RATE,
      PLAYBACK_DEVICE,
      PLAYBACK_SAMPLE_RATE,
      STT_PROVIDER,
      CAMERA_DEVICE,
      RMS_THRESHOLD,
      ",".join(WAKE_PHRASES),
      WAKE_ON_ANY_SPEECH,
  )
  while not STOP_REQUESTED:
    try:
      path = record_utterance(3600)
      if path is None:
        continue
      text = transcribe(path, WAKE_STT_MODEL)
      path.unlink(missing_ok=True)
      phrase_matched = contains_phrase(text, WAKE_PHRASES)
      if phrase_matched or (WAKE_ON_ANY_SPEECH and bool(text)):
        LOG.info(
            "Wake accepted (%s)",
            "phrase" if phrase_matched else "debug any-speech mode",
        )
        wake_image = capture_camera()
        if is_vision_request(text) and wake_image is not None:
          conversation_loop(text, wake_image)
        else:
          conversation_loop()
      elif text:
        LOG.info("Wake phrase not found")
    except Exception:
      LOG.exception("Voice loop error; retrying")
      time.sleep(2)

  LOG.info("Voice daemon stopped")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
