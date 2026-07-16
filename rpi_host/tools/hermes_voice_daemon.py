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

SAMPLE_RATE = int(os.getenv("CAR_VOICE_SAMPLE_RATE", "48000"))
BLOCK_MS = int(os.getenv("CAR_VOICE_BLOCK_MS", "100"))
BLOCK_SIZE = SAMPLE_RATE * BLOCK_MS // 1000
INPUT_DEVICE = int(os.getenv("CAR_VOICE_INPUT_DEVICE", "0"))
RMS_THRESHOLD = int(os.getenv("CAR_VOICE_RMS_THRESHOLD", "250"))
SPEECH_CONFIRM_MS = int(os.getenv("CAR_VOICE_SPEECH_CONFIRM_MS", "300"))
END_SILENCE_MS = int(os.getenv("CAR_VOICE_END_SILENCE_MS", "1200"))
IDLE_TIMEOUT_SECONDS = float(os.getenv("CAR_VOICE_IDLE_TIMEOUT_SECONDS", "30"))
ACTIVE_TIMEOUT_SECONDS = float(os.getenv("CAR_VOICE_ACTIVE_TIMEOUT_SECONDS", "90"))
MAX_UTTERANCE_SECONDS = float(os.getenv("CAR_VOICE_MAX_UTTERANCE_SECONDS", "20"))
PLAYBACK_DEVICE = os.getenv("CAR_VOICE_PLAYBACK_DEVICE", "plughw:0,0")
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


def request_stop(_signum: int, _frame: object) -> None:
  global STOP_REQUESTED
  STOP_REQUESTED = True


def normalize_text(text: str) -> str:
  return re.sub(r"[^\w\u4e00-\u9fff]", "", text).lower()


def contains_phrase(text: str, phrases: tuple[str, ...]) -> bool:
  normalized = normalize_text(text)
  return any(normalize_text(phrase) in normalized for phrase in phrases)


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


def ask_hermes(text: str, session_id: str | None) -> tuple[str, str | None]:
  prompt = text
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
  if session_id:
    command.extend(["--resume", session_id])
  else:
    prompt = (
        "你是安装在智能小车上的中文语音助手。回答应简短、自然、适合直接朗读，"
        "不要使用 Markdown。当前阶段禁止控制小车或 STM32，只进行对话。\n"
        f"用户说：{text}"
    )
    command[3] = prompt

  LOG.info("Calling Hermes%s", f" session {session_id}" if session_id else "")
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
            str(SAMPLE_RATE),
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


def conversation_loop() -> None:
  session_id: str | None = None
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
    response, session_id = ask_hermes(text, session_id)
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
      "Voice daemon ready: input=%s, playback=%s, threshold=%s, wake=%s, "
      "wake_on_any_speech=%s",
      INPUT_DEVICE,
      PLAYBACK_DEVICE,
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
