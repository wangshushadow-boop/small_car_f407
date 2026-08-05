"""ROS 感知事件发布模块，不包含模型、LangGraph 或小车控制逻辑。"""

from __future__ import annotations

from .perception import AudioFrame
from .perception_tool import PerceptionTool
from .voice_activity import EnergyVad, SpeechSegment
from sensor_msgs.msg import CompressedImage
from small_car_interfaces.msg import AudioFrame as RosAudioFrame
from small_car_interfaces.msg import SpeechEvent

class SpeechEventPublisher(PerceptionTool):
    """接收 ROS 音视频、检测语句结束并发布 /car/agent/speech_finished。"""

    def __init__(
        self,
    ) -> None:
        super().__init__()
        self.declare_parameter("vad_energy_threshold", 500)
        self.declare_parameter("vad_min_speech_ms", 300)
        self.declare_parameter("vad_silence_ms", 600)
        self.declare_parameter("vad_max_segment_ms", 15_000)
        self._vad = EnergyVad(
            energy_threshold=int(self.get_parameter("vad_energy_threshold").value),
            min_speech_ms=int(self.get_parameter("vad_min_speech_ms").value),
            silence_ms=int(self.get_parameter("vad_silence_ms").value),
            max_segment_ms=int(self.get_parameter("vad_max_segment_ms").value),
        )
        self._publisher = self.create_publisher(SpeechEvent, "/car/agent/speech_finished", 10)

    def _on_audio(self, msg) -> None:
        super()._on_audio(msg)
        frame = AudioFrame(
            timestamp_ns=msg.header.stamp.sec * 1_000_000_000 + msg.header.stamp.nanosec,
            sample_rate=msg.sample_rate,
            channels=msg.channels,
            encoding=msg.encoding,
            frame_samples=msg.frame_samples,
            data=bytes(msg.data),
        )
        segment = self._vad.push(frame)
        if segment is None:
            return
        self._publish_event(segment)
        self.get_logger().info(
            "检测到语音结束：%.2f 秒，已发布 Agent 事件"
            % ((segment.end_timestamp_ns - segment.start_timestamp_ns) / 1_000_000_000)
        )

    def _publish_event(self, segment: SpeechSegment) -> None:
        event = SpeechEvent()
        event.header.stamp.sec = segment.start_timestamp_ns // 1_000_000_000
        event.header.stamp.nanosec = segment.start_timestamp_ns % 1_000_000_000
        snapshot = self.get_latest_perception()
        if snapshot.image:
            event.image.header.stamp.sec = snapshot.image.timestamp_ns // 1_000_000_000
            event.image.header.stamp.nanosec = snapshot.image.timestamp_ns % 1_000_000_000
            event.image.header.frame_id = snapshot.image.frame_id
            event.image.format = "jpeg"
            event.image.data = list(snapshot.image.jpeg)
        first = segment.frames[0]
        event.speech_audio.header.stamp = event.header.stamp
        event.speech_audio.sample_rate = first.sample_rate
        event.speech_audio.channels = first.channels
        event.speech_audio.encoding = first.encoding
        event.speech_audio.frame_samples = sum(frame.frame_samples for frame in segment.frames)
        event.speech_audio.data = list(b"".join(frame.data for frame in segment.frames))
        self._publisher.publish(event)
