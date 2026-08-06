"""订阅小车音视频 topic，并向 LangGraph 提供最新感知快照。"""

from __future__ import annotations

from collections import deque
from threading import Condition
from typing import Deque

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CompressedImage
from small_car_interfaces.msg import AudioFrame as RosAudioFrame

from .perception import AgentPerception, AudioFrame, ImageFrame, build_wav


def stamp_to_ns(stamp) -> int:
    """将 ROS 时间戳转换为纳秒。"""
    return stamp.sec * 1_000_000_000 + stamp.nanosec


class PerceptionTool(Node):
    """在本进程缓存图像与音频，供 Agent 的工具节点按需读取。"""

    def __init__(self, audio_buffer_seconds: float = 8.0) -> None:
        super().__init__("small_car_agent_perception")
        self._audio_buffer_ns = int(audio_buffer_seconds * 1_000_000_000)
        self._latest_image: ImageFrame | None = None
        self._audio_frames: Deque[AudioFrame] = deque()
        self._condition = Condition()
        self.create_subscription(
            CompressedImage,
            "/car/camera/image/compressed",
            self._on_image,
            qos_profile_sensor_data,
        )
        self.create_subscription(
            RosAudioFrame,
            "/car/audio/input",
            self._on_audio,
            qos_profile_sensor_data,
        )

    def _on_image(self, msg: CompressedImage) -> None:
        image = ImageFrame(
            timestamp_ns=stamp_to_ns(msg.header.stamp),
            jpeg=bytes(msg.data),
            frame_id=msg.header.frame_id,
        )
        with self._condition:
            self._latest_image = image
            self._condition.notify_all()

    def _on_audio(self, msg: RosAudioFrame) -> None:
        frame = AudioFrame(
            timestamp_ns=stamp_to_ns(msg.header.stamp),
            sample_rate=msg.sample_rate,
            channels=msg.channels,
            encoding=msg.encoding,
            frame_samples=msg.frame_samples,
            data=bytes(msg.data),
        )
        with self._condition:
            self._audio_frames.append(frame)
            newest_ns = frame.timestamp_ns + frame.duration_ns
            while self._audio_frames and (
                newest_ns - self._audio_frames[0].timestamp_ns > self._audio_buffer_ns
            ):
                self._audio_frames.popleft()
            self._condition.notify_all()

    def get_latest_perception(self, audio_window_seconds: float = 3.0) -> AgentPerception:
        """返回最新图像及其时间点前的最近一段音频。"""
        with self._condition:
            image = self._latest_image
            end_ns = image.timestamp_ns if image else (
                self._audio_frames[-1].timestamp_ns + self._audio_frames[-1].duration_ns
                if self._audio_frames
                else 0
            )
            start_ns = end_ns - int(audio_window_seconds * 1_000_000_000)
            audio_frames = tuple(
                frame
                for frame in self._audio_frames
                if frame.timestamp_ns + frame.duration_ns > start_ns
                and frame.timestamp_ns <= end_ns
            )
        return AgentPerception(
            image=image,
            audio_frames=audio_frames,
            audio_wav=build_wav(audio_frames),
        )

    def wait_for_perception(self, timeout_seconds: float = 5.0) -> bool:
        """等待至少收到一帧图像或一帧音频，适合 Agent 启动时调用。"""
        with self._condition:
            return self._condition.wait_for(
                lambda: self._latest_image is not None or bool(self._audio_frames),
                timeout=timeout_seconds,
            )


def main(args=None) -> None:
    """用于联调：持续接收，并周期性打印最新快照摘要。"""
    rclpy.init(args=args)
    node = PerceptionTool()
    try:
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.2)
            perception = node.get_latest_perception()
            if perception.image or perception.audio_frames:
                model_input = perception.to_model_input()
                node.get_logger().info(
                    "感知快照：image=%s, audio_frames=%d, audio_duration_ms=%.1f"
                    % (
                        "yes" if perception.image else "no",
                        model_input["audio_frame_count"],
                        model_input["audio_duration_ms"],
                    )
                )
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
