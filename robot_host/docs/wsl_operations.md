# WSL 操作

本机 WSL 用于查看树莓派的 ROS 2 数据、运行 rqt/RViz 和录制 bag。树莓派端服务的启停、底盘测试与参数调整见[树莓派运维](../operations.md)。

## 进入 ROS 终端

在 WSL 中执行：

```zsh
cd /mnt/d/work/smart_car/robot_host
bash scripts/setup_wsl_ros_env.sh 192.168.3.85
```

脚本会启动一个已加载 ROS 2 Kilted 的 zsh，并设置：

```text
ROS_DOMAIN_ID=0
RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
ROS_STATIC_PEERS=192.168.3.85
```

不要在这个 zsh 中再切换为 Fast DDS。`/mnt/c`、`/mnt/d` 等当前目录不会影响 ROS 节点发现。

## 检查树莓派节点和话题

```zsh
ros2 daemon stop
ros2 node list --no-daemon
ros2 topic list --no-daemon --spin-time 5
ros2 topic info /car/camera/image/compressed --verbose
```

`ros2 node list` 没有标题；没有任何输出表示当前 WSL 未发现节点。先重新执行“进入 ROS 终端”的命令；仍为空时，确认树莓派容器运行：

```zsh
ssh ubuntu@192.168.3.85 'cd ~/small_car_f407/robot_host/ros2 && docker compose ps'
```

## 查看相机

```zsh
rqt_image_view /car/camera/image/compressed
```

若命令不存在，先安装对应插件：

```zsh
sudo apt update
sudo apt install ros-kilted-rqt-image-view
```

## Agent 感知工具

先按“进入 ROS 终端”加载环境，再启动接收检查程序：

```zsh
ros2 run small_car_agent perception_snapshot
```

它订阅 `/car/camera/image/compressed` 与 `/car/audio/input`，打印是否收到图像、音频帧数和音频时长。
LangGraph 与该工具在同一 Python 进程时，直接使用：

```python
from small_car_agent.perception_tool import PerceptionTool

perception_tool = PerceptionTool()
# 由 rclpy executor 持续 spin perception_tool。
snapshot = perception_tool.get_latest_perception(audio_window_seconds=3.0)
model_input = snapshot.to_model_input()
```

`model_input["image_data_url"]` 可交给支持 JPEG data URL 的视觉模型；
`model_input["audio_wav"]` 是 WAV 字节，适合写入临时文件后传给 MiniCPM-o 等音频模型。

### 语音主动触发 Agent

以下命令只做联调：检测到一句话结束后打印 Agent 事件，不调用模型或控制小车：

```zsh
ros2 run small_car_agent agent_event_node
```

`small_car_agent` 只发布 `/car/agent/speech_finished`。正式 Agent 进程位于
`llm_agent/agent`，订阅该事件并在后台工作线程调用 LangGraph，因此不会阻塞 ROS 接收回调：

```python
from llm_agent.agent.langgraph_runtime import create_graph_handler
from llm_agent.agent.ros_event_source import RosAgentEventSource

event_source = RosAgentEventSource(create_graph_handler(graph))
# 将 event_source 添加到 rclpy executor 后持续 spin。
```

触发输入的 `event` 固定为 `speech_finished`，包含 `speech_wav`（本次语音）和 `perception`
（最新图像）。VAD 默认条件为：能量阈值 500、至少 300 ms 语音、随后 600 ms 静音。
可通过 ROS 参数 `vad_energy_threshold`、`vad_min_speech_ms`、`vad_silence_ms` 调整。
模型不能直接发布底盘控制；控制请求仍应经过独立的 ROS 工具和安全校验层。

## 录制与回放音视频

录制当前实际使用的相机、相机参数和麦克风话题：

```zsh
mkdir -p runtime/bags
ros2 bag record -o runtime/bags/av_$(date +%Y%m%d_%H%M%S) \
  /car/camera/image/compressed /car/camera/camera_info /car/audio/input
```

回放：

```zsh
ros2 bag play runtime/bags/<bag目录>
```

回放前在另一个已加载相同 ROS 环境的 WSL 终端中启动：

```zsh
rqt_image_view /car/camera/image/compressed
```
