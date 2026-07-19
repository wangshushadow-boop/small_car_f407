#!/usr/bin/env python3
"""Transcribe a mono 16-bit WAV file with local SenseVoice-Small."""

from __future__ import annotations

import argparse
import json
import sys
import time
import wave
from pathlib import Path

import numpy as np
import sherpa_onnx


DEFAULT_MODEL_DIR = Path(
    "/home/ubuntu/.hermes/models/"
    "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17"
)


def load_wav(path: Path) -> tuple[int, np.ndarray]:
  with wave.open(str(path), "rb") as wav_file:
    channels = wav_file.getnchannels()
    sample_width = wav_file.getsampwidth()
    sample_rate = wav_file.getframerate()
    frames = wav_file.readframes(wav_file.getnframes())

  if channels != 1:
    raise ValueError(f"expected mono WAV, got {channels} channels")
  if sample_width != 2:
    raise ValueError(f"expected 16-bit WAV, got {sample_width * 8} bits")
  if sample_rate != 16000:
    raise ValueError(f"expected 16000 Hz WAV, got {sample_rate} Hz")

  samples = np.frombuffer(frames, dtype="<i2").astype(np.float32)
  samples /= 32768.0
  return sample_rate, samples


def transcribe(path: Path, model_dir: Path, threads: int) -> str:
  model = model_dir / "model.int8.onnx"
  tokens = model_dir / "tokens.txt"
  if not model.is_file() or not tokens.is_file():
    raise FileNotFoundError(f"SenseVoice model is incomplete: {model_dir}")

  sample_rate, samples = load_wav(path)
  recognizer = sherpa_onnx.OfflineRecognizer.from_sense_voice(
      model=str(model),
      tokens=str(tokens),
      num_threads=threads,
      sample_rate=sample_rate,
      language="zh",
      use_itn=True,
      provider="cpu",
  )
  stream = recognizer.create_stream()
  stream.accept_waveform(sample_rate, samples)
  recognizer.decode_stream(stream)
  return stream.result.text.strip()


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("wav", type=Path)
  parser.add_argument("--model-dir", type=Path, default=DEFAULT_MODEL_DIR)
  parser.add_argument("--threads", type=int, default=4)
  args = parser.parse_args()

  started = time.monotonic()
  try:
    text = transcribe(args.wav, args.model_dir, max(1, args.threads))
    print(
        json.dumps(
            {
                "success": True,
                "text": text,
                "elapsed_seconds": round(time.monotonic() - started, 3),
            },
            ensure_ascii=False,
        )
    )
    return 0
  except Exception as error:
    print(
        json.dumps(
            {"success": False, "error": str(error)}, ensure_ascii=False
        )
    )
    return 1


if __name__ == "__main__":
  sys.exit(main())
