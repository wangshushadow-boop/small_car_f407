"""Agent 运行层的 ROS 事件入口；模型调用始终在后台线程执行。"""

from __future__ import annotations

from queue import Queue
from threading import Event, Thread
from typing import Callable

import rclpy
from rclpy.node import Node
from small_car_interfaces.msg import SpeechEvent

from .input_contract import speech_event_to_agent_input


class RosAgentEventSource(Node):
    """订阅感知层事件并异步交给 Agent 图，防止推理阻塞 ROS 回调。"""

    def __init__(self, handler: Callable[[dict], None]) -> None:
        super().__init__("llm_agent_event_source")
        self._handler = handler
        self._events: Queue[dict | None] = Queue(maxsize=4)
        self._stopping = Event()
        self.create_subscription(SpeechEvent, "/car/agent/speech_finished", self._on_event, 10)
        self._worker = Thread(target=self._run, daemon=True)
        self._worker.start()

    def _on_event(self, event: SpeechEvent) -> None:
        if self._events.full():
            self.get_logger().warning("Agent 忙碌，丢弃一条语音事件")
            return
        self._events.put_nowait(speech_event_to_agent_input(event))

    def _run(self) -> None:
        while not self._stopping.is_set():
            event = self._events.get()
            if event is None:
                return
            try:
                self._handler(event)
            except Exception as error:
                self.get_logger().error(f"Agent 图执行失败：{error}")

    def destroy_node(self) -> bool:
        self._stopping.set()
        if not self._events.full():
            self._events.put_nowait(None)
        self._worker.join(timeout=2)
        return super().destroy_node()
