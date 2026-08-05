"""LangGraph 运行时适配；图定义、模型和工具由上层组合。"""

from __future__ import annotations

from typing import Protocol


class InvokableGraph(Protocol):
    def invoke(self, input: dict) -> object:
        """执行一轮 LangGraph。"""


def create_graph_handler(graph: InvokableGraph):
    """把 ROS 事件输入交给编译后的 LangGraph。"""

    def handle(event_input: dict) -> None:
        graph.invoke(event_input)

    return handle
