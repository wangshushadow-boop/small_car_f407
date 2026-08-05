"""ROS 事件转换为 Agent 状态的纯数据契约。"""

from __future__ import annotations

import base64
import io
import wave


def pcm_s16le_to_wav(data: bytes, sample_rate: int, channels: int) -> bytes | None:
    """将 ROS 中的 PCM S16LE 数据转换为模型可使用的 WAV 字节。"""
    if not data or sample_rate <= 0 or channels <= 0:
        return None
    output = io.BytesIO()
    with wave.open(output, "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(sample_rate)
        wav.writeframes(data)
    return output.getvalue()


def speech_event_to_agent_input(event) -> dict:
    """将 SpeechEvent 转成 LangGraph 一轮调用的输入。

    图像与音频字节仅用于本轮模型调用；长期记忆应保存摘要或转写文本。
    """
    image_bytes = bytes(event.image.data)
    image_data_url = None
    if image_bytes:
        image_data_url = "data:image/jpeg;base64," + base64.b64encode(image_bytes).decode("ascii")
    audio = event.speech_audio
    return {
        "event": "speech_finished",
        "speech_wav": pcm_s16le_to_wav(bytes(audio.data), audio.sample_rate, audio.channels),
        "speech_start_timestamp_ns": event.header.stamp.sec * 1_000_000_000 + event.header.stamp.nanosec,
        "perception": {
            "image_data_url": image_data_url,
            "image_timestamp_ns": event.image.header.stamp.sec * 1_000_000_000
            + event.image.header.stamp.nanosec if image_bytes else None,
            "audio_sample_rate": audio.sample_rate,
            "audio_channels": audio.channels,
            "audio_encoding": audio.encoding,
        },
    }
