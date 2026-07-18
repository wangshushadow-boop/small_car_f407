from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    share = get_package_share_directory("small_car_bridge")
    default_bridge_config = os.path.join(share, "config", "bridge.yaml")
    default_chassis_config = os.path.join(share, "config", "chassis_params.yaml")

    bridge_config = LaunchConfiguration("bridge_config")
    chassis_config = LaunchConfiguration("chassis_config")

    return LaunchDescription([
        DeclareLaunchArgument("bridge_config", default_value=default_bridge_config),
        DeclareLaunchArgument("chassis_config", default_value=default_chassis_config),
        Node(
            package="small_car_bridge",
            executable="small_car_bridge_node",
            name="small_car_bridge",
            output="screen",
            parameters=[bridge_config, {"chassis_config": chassis_config}],
        ),
        # 传感器安装位姿尚未实测，默认均与 base_link 重合；后续只改 launch 参数。
        Node(
            package="tf2_ros",
            executable="static_transform_publisher",
            name="imu_tf",
            arguments=["--x", "0", "--y", "0", "--z", "0",
                       "--roll", "0", "--pitch", "0", "--yaw", "0",
                       "--frame-id", "base_link", "--child-frame-id", "imu_link"],
        ),
        Node(
            package="tf2_ros",
            executable="static_transform_publisher",
            name="ultrasonic_tf",
            arguments=["--x", "0", "--y", "0", "--z", "0",
                       "--roll", "0", "--pitch", "0", "--yaw", "0",
                       "--frame-id", "base_link", "--child-frame-id", "ultrasonic_link"],
        ),
    ])
