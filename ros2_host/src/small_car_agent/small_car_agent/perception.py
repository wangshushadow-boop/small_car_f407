"""不依赖 ROS 的音视频快照组装逻辑，便于 LangGraph 节点直接使用。"""

from __future__ import annotations

import base64
import io
import wave
from dataclasses import dataclass


@dataclass(frozen=True)
class ImageFrame:
    """已压缩的相机帧。timestamp_ns 使用 ROS Header 的采集时间。"""

    timestamp_ns: int
    jpeg: bytes
    frame_id: str = ""


@dataclass(frozen=True)
class AudioFrame:
    """PCM 音频帧；timestamp_ns 表示第一个采样点的时间。"""

    timestamp_ns: int
    sample_rate: int
    channels: int
    encoding: str
    frame_samples: int
    data: bytes

    @property
    def duration_ns(self) -> int:
        if self.sample_rate <= 0:
            return 0
        return self.frame_samples * 1_000_000_000 // self.sample_rate


@dataclass(frozen=True)
class AgentPerception:
    """供 LangGraph 节点消费的一次完整感知快照。"""

    image: ImageFrame | None
    audio_frames: tuple[AudioFrame, ...]
    audio_wav: bytes | None

    def to_model_input(self) -> dict:
        """返回与模型实现无关的可序列化输入。

        image_data_url 可直接用于支持 OpenAI 风格 image_url 的视觉模型；audio_wav
        是标准 WAV 字节，可写入临时文件后交给 MiniCPM-o 等多模态模型。
        """
        image_data_url = None
        if self.image is not None:
            image_data_url = "data:image/jpeg;base64," + base64.b64encode(
                self.image.jpeg
            ).decode("ascii")
        audio_timestamp_ns = self.audio_frames[0].timestamp_ns if self.audio_frames else None
        return {
            "image_data_url": image_data_url,
            "image_timestamp_ns": self.image.timestamp_ns if self.image else None,
            "audio_wav": self.audio_wav,
            "audio_timestamp_ns": audio_timestamp_ns,
            "audio_frame_count": len(self.audio_frames),
            "audio_duration_ms": sum(frame.duration_ns for frame in self.audio_frames) / 1_000_000,
        }


def build_wav(audio_frames: tuple[AudioFrame, ...]) -> bytes | None:
    """将格式一致的 pcm_s16le 帧拼成一个标准 WAV 文件。"""
    if not audio_frames:
        return None
    first = audio_frames[0]
    if first.encoding.lower() != "pcm_s16le":
        return None
    if first.sample_rate <= 0 or first.channels <= 0:
        return None
    if any(
        frame.encoding.lower() != first.encoding.lower()
        or frame.sample_rate != first.sample_rate
        or frame.channels != first.channels
        for frame in audio_frames
    ):
        return None

    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(first.channels)
        wav.setsampwidth(2)
        wav.setframerate(first.sample_rate)
        wav.writeframes(b"".join(frame.data for frame in audio_frames))
    return output.getvalue()
