"""通过 WSLg PulseAudio 或 Linux ALSA 采集麦克风并发布 ROS 音频帧。"""

from array import array
import subprocess

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from small_car_interfaces.msg import AudioFrame


class JabraAudioPublisher(Node):
    """将 RDPSource 的 16 kHz 单声道 PCM 转成 AudioFrame。"""

    SAMPLE_RATE = 16000
    FRAME_SAMPLES = 320
    FRAME_BYTES = FRAME_SAMPLES * 2
    FRAME_DURATION_NS = 20_000_000

    def __init__(self) -> None:
        super().__init__("small_car_jabra_audio_publisher")
        self.declare_parameter("backend", "pulse")
        self.declare_parameter("pulse_source", "RDPSource")
        self.declare_parameter("alsa_device", "plughw:CARD=USB,DEV=0")
        backend = str(self.get_parameter("backend").value)
        pulse_source = str(self.get_parameter("pulse_source").value)
        alsa_device = str(self.get_parameter("alsa_device").value)
        self._publisher = self.create_publisher(
            AudioFrame,
            "/car/audio/input",
            qos_profile_sensor_data,
        )
        if backend == "pulse":
            command = [
                "parec",
                "--raw",
                f"--device={pulse_source}",
                "--format=s16le",
                f"--rate={self.SAMPLE_RATE}",
                "--channels=1",
                "--latency-msec=20",
            ]
            source_description = f"PulseAudio {pulse_source}"
        elif backend == "alsa":
            command = [
                "arecord",
                "--quiet",
                "--device",
                alsa_device,
                "--format",
                "S16_LE",
                "--rate",
                str(self.SAMPLE_RATE),
                "--channels",
                "1",
                "--file-type",
                "raw",
            ]
            source_description = f"ALSA {alsa_device}"
        else:
            raise ValueError(f"不支持的音频后端：{backend}，应为 pulse 或 alsa")

        self._capture = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.create_timer(0.001, self._read_frame)
        self.get_logger().info(
            f"正在从 {source_description} 发布 16 kHz 单声道音频"
        )

    def _read_frame(self) -> None:
        if self._capture.poll() is not None:
            error = self._capture.stderr.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"音频采集进程已退出：{error}")
        data = self._capture.stdout.read(self.FRAME_BYTES)
        if len(data) != self.FRAME_BYTES:
            return

        # 读取完成时刻减去帧长，近似表示第一个采样点的采集时间。
        timestamp_ns = self.get_clock().now().nanoseconds - self.FRAME_DURATION_NS
        msg = AudioFrame()
        msg.header.stamp.sec = timestamp_ns // 1_000_000_000
        msg.header.stamp.nanosec = timestamp_ns % 1_000_000_000
        msg.header.frame_id = "jabra_microphone"
        msg.sample_rate = self.SAMPLE_RATE
        msg.channels = 1
        msg.encoding = "pcm_s16le"
        msg.frame_samples = self.FRAME_SAMPLES
        msg.data = array("B", data)
        self._publisher.publish(msg)

    def destroy_node(self) -> bool:
        if self._capture.poll() is None:
            self._capture.terminate()
            try:
                self._capture.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._capture.kill()
        return super().destroy_node()


def main(args=None) -> None:
    rclpy.init(args=args)
    node = JabraAudioPublisher()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
