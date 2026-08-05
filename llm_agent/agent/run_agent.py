"""运行入口：ROS 事件 → LangGraph → MiniCPM-o 回复。"""
import rclpy
from rclpy.executors import ExternalShutdownException
from .graph import build_graph
from .ros_event_source import RosAgentEventSource


def main() -> None:
    rclpy.init()
    graph = build_graph()
    def handle(event: dict) -> None:
        answer = graph.invoke(event).get("answer", "（无回复）")
        print(f"\nMiniCPM-o：{answer}", flush=True)
    node = RosAgentEventSource(handle)
    node.get_logger().info("Agent 已启动，等待 /car/agent/speech_finished")
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
