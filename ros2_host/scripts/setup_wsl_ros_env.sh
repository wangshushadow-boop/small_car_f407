#!/usr/bin/env bash

# bash /mnt/d/stm32/demo/smart_car/ros2_host/scripts/setup_wsl_ros_env.sh

# 根据脚本位置定位项目，避免写死仓库所在盘符。
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"
ros_setup="/opt/ros/kilted/setup.bash"
fastdds_profile="${project_dir}/config/fastdds_wsl.xml"

# 加载 ROS2，并使用与树莓派相同的通信域。
if [[ ! -f "${ros_setup}" ]]; then
  echo "错误：未找到 ROS2 环境 ${ros_setup}"
  exit 1
fi
source "${ros_setup}"
export ROS_DOMAIN_ID=0

# 允许发现同一局域网中的树莓派，关闭仅本机通信限制。
export ROS_AUTOMATIC_DISCOVERY_RANGE=SUBNET
unset ROS_LOCALHOST_ONLY
export ROS_STATIC_PEERS=192.168.3.85

# 固定 Fast DDS 使用电脑的局域网接口。
if [[ ! -f "${fastdds_profile}" ]]; then
  echo "错误：未找到 Fast DDS 配置 ${fastdds_profile}"
  exit 1
fi
export FASTDDS_DEFAULT_PROFILES_FILE="${fastdds_profile}"

# 清理旧终端遗留的 ROS2 daemon，并使用当前 DDS 配置重新启动。
# timeout 防止异常 daemon 再次让脚本长时间卡住。
timeout 5 ros2 daemon stop >/dev/null 2>&1 || true
if ! timeout 10 ros2 daemon start >/dev/null 2>&1; then
  echo "错误：ROS2 daemon 启动失败，请在 PowerShell 执行 wsl --shutdown 后重试。"
  exit 1
fi

# 给 DDS 留出短暂的局域网设备发现时间。
sleep 2

# 进入继承上述环境的交互终端。
# 输入 exit 可退出该终端，返回执行脚本前的终端。
echo "ROS2 环境和 daemon 配置完成。"
echo "已进入 ROS2 终端，可执行 ros2 topic list 检查树莓派话题。"
exec bash -i
