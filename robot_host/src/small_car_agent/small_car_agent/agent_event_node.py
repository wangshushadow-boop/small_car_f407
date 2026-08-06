"""事件桥接的联调入口：仅打印事件摘要，不调用真实模型。"""

from __future__ import annotations

import rclpy
from rclpy.executors import ExternalShutdownException

from .agent_events import SpeechEventPublisher


def main(args=None) -> None:
    rclpy.init(args=args)

    node = SpeechEventPublisher()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
