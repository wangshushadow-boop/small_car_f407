"""运行入口：ROS 事件 → LangGraph → MiniCPM-o 回复。"""
import rclpy
from rclpy.executors import ExternalShutdownException
from .graph import build_graph
from llm_agent.input.ros_perception import RosPerceptionInput


def main() -> None:
    rclpy.init()
    graph = build_graph()
    def handle(event: dict) -> None:
        answer = graph.invoke(event).get("answer", "（无回复）")
        print(f"\nMiniCPM-o：{answer}", flush=True)
    node = RosPerceptionInput(handle)
    node.get_logger().info("Agent 已启动，直接订阅树莓派音视频 topic")
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
