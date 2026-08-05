# WSL 操作

本机 WSL 用于查看树莓派的 ROS 2 数据、运行 rqt/RViz 和录制 bag。树莓派端服务的启停、底盘测试与参数调整见[树莓派运维](../operations.md)。

## 进入 ROS 终端

在 WSL 中执行：

```zsh
cd /mnt/d/work/smart_car/ros2_host
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
ssh ubuntu@192.168.3.85 'cd ~/small_car_f407/ros2_host/ros2 && docker compose ps'
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
