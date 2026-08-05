"""轻量级语音活动检测：只负责判定一句话的开始和结束。"""

from __future__ import annotations

import audioop
from dataclasses import dataclass

from .perception import AudioFrame


@dataclass(frozen=True)
class SpeechSegment:
    """一段已结束的连续语音，保留原始音频帧和时间范围。"""

    frames: tuple[AudioFrame, ...]

    @property
    def start_timestamp_ns(self) -> int:
        return self.frames[0].timestamp_ns

    @property
    def end_timestamp_ns(self) -> int:
        last = self.frames[-1]
        return last.timestamp_ns + last.duration_ns


class EnergyVad:
    """基于 PCM 能量的 VAD，避免第一版引入额外模型依赖。

    触发条件：连续语音达到 min_speech_ms，之后静音达到 silence_ms。
    阈值需根据实际 Jabra 环境通过 ROS 参数调整。
    """

    def __init__(
        self,
        energy_threshold: int = 500,
        min_speech_ms: int = 300,
        silence_ms: int = 600,
        max_segment_ms: int = 15_000,
    ) -> None:
        self._energy_threshold = energy_threshold
        self._min_speech_ns = min_speech_ms * 1_000_000
        self._silence_ns = silence_ms * 1_000_000
        self._max_segment_ns = max_segment_ms * 1_000_000
        self._frames: list[AudioFrame] = []
        self._speech_ns = 0
        self._silence_ns_seen = 0

    def push(self, frame: AudioFrame) -> SpeechSegment | None:
        """输入一帧音频；一句话结束时返回语音片段，否则返回 None。"""
        if frame.encoding.lower() != "pcm_s16le" or not frame.data:
            return None
        is_speech = audioop.rms(frame.data, 2) >= self._energy_threshold

        if not self._frames:
            if not is_speech:
                return None
            self._frames.append(frame)
            self._speech_ns = frame.duration_ns
            return None

        self._frames.append(frame)
        if is_speech:
            self._speech_ns += frame.duration_ns
            self._silence_ns_seen = 0
        else:
            self._silence_ns_seen += frame.duration_ns

        total_ns = sum(item.duration_ns for item in self._frames)
        if total_ns >= self._max_segment_ns:
            return self._finish_if_valid()
        if self._silence_ns_seen >= self._silence_ns:
            return self._finish_if_valid()
        return None

    def _finish_if_valid(self) -> SpeechSegment | None:
        frames = tuple(self._frames)
        self._frames = []
        self._speech_ns = 0
        self._silence_ns_seen = 0
        if not frames:
            return None
        speech_ns = sum(
            frame.duration_ns
            for frame in frames
            if audioop.rms(frame.data, 2) >= self._energy_threshold
        )
        if speech_ns < self._min_speech_ns:
            return None
        return SpeechSegment(frames=frames)
