"""直接订阅树莓派音视频 topic，并在语音结束时触发 Agent。"""
from __future__ import annotations

import audioop
import base64
import io
import wave
from collections import deque
from queue import Queue
from threading import Event, Thread

from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import CompressedImage
from small_car_interfaces.msg import AudioFrame


def _stamp_ns(stamp) -> int:
    return stamp.sec * 1_000_000_000 + stamp.nanosec


class RosPerceptionInput(Node):
    """WSL Agent 输入节点：缓存画面、检测语句结束、后台执行 Agent。"""

    def __init__(self, handler) -> None:
        super().__init__("llm_agent_perception_input")
        self._handler = handler
        self._image: tuple[int, bytes] | None = None
        self._frames = deque(maxlen=750)  # 最多约 15 秒，20 ms/帧。
        self._speech_frames = []
        self._speech_ns = 0
        self._silence_ns = 0
        self.declare_parameter("vad_energy_threshold", 500)
        self.declare_parameter("vad_min_speech_ms", 300)
        self.declare_parameter("vad_silence_ms", 600)
        self._events: Queue[dict | None] = Queue(maxsize=4)
        self._stopping = Event()
        self._worker = Thread(target=self._run, daemon=True)
        self._worker.start()
        self.create_subscription(CompressedImage, "/car/camera/image/compressed", self._on_image, qos_profile_sensor_data)
        self.create_subscription(AudioFrame, "/car/audio/input", self._on_audio, qos_profile_sensor_data)

    def _on_image(self, msg: CompressedImage) -> None:
        self._image = (_stamp_ns(msg.header.stamp), bytes(msg.data))

    def _on_audio(self, msg: AudioFrame) -> None:
        data = bytes(msg.data)
        if msg.encoding.lower() != "pcm_s16le" or not data or msg.sample_rate <= 0:
            return
        frame = (msg, data, _stamp_ns(msg.header.stamp), msg.frame_samples * 1_000_000_000 // msg.sample_rate)
        self._frames.append(frame)
        speech = audioop.rms(data, 2) >= int(self.get_parameter("vad_energy_threshold").value)
        if not self._speech_frames and not speech:
            return
        self._speech_frames.append(frame)
        if speech:
            self._speech_ns += frame[3]
            self._silence_ns = 0
        else:
            self._silence_ns += frame[3]
        if self._silence_ns >= int(self.get_parameter("vad_silence_ms").value) * 1_000_000:
            self._finish_speech()

    def _finish_speech(self) -> None:
        frames, self._speech_frames = self._speech_frames, []
        speech_ns, self._speech_ns = self._speech_ns, 0
        self._silence_ns = 0
        if not frames or speech_ns < int(self.get_parameter("vad_min_speech_ms").value) * 1_000_000:
            return
        first_msg = frames[0][0]
        wav = io.BytesIO()
        with wave.open(wav, "wb") as output:
            output.setnchannels(first_msg.channels)
            output.setsampwidth(2)
            output.setframerate(first_msg.sample_rate)
            output.writeframes(b"".join(frame[1] for frame in frames))
        image_url = None
        if self._image:
            image_url = "data:image/jpeg;base64," + base64.b64encode(self._image[1]).decode("ascii")
        event = {"event": "speech_finished", "speech_wav": wav.getvalue(), "perception": {"image_data_url": image_url}}
        if self._events.full():
            self.get_logger().warning("Agent 忙碌，丢弃语音事件")
        else:
            self._events.put_nowait(event)
            self.get_logger().info("语音结束，直接触发本地 Agent")

    def _run(self) -> None:
        while not self._stopping.is_set():
            event = self._events.get()
            if event is None:
                return
            try:
                self._handler(event)
            except Exception as error:
                self.get_logger().error(f"Agent 执行失败：{error}")

    def destroy_node(self) -> bool:
        self._stopping.set()
        if not self._events.full():
            self._events.put_nowait(None)
        self._worker.join(timeout=2)
        return super().destroy_node()
