from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    share = get_package_share_directory("smallcar_ros_and_mcu_bridge")
    default_bridge_config = os.path.join(share, "config", "bridge.yaml")
    default_chassis_config = os.path.join(share, "config", "chassis_params.yaml")
    motion_share = get_package_share_directory("small_car_motion_controller")
    default_motion_config = os.path.join(
        motion_share, "config", "motion_controller.yaml"
    )
    description_share = get_package_share_directory("small_car_description")
    description_launch = os.path.join(description_share, "launch", "description.launch.py")

    bridge_config = LaunchConfiguration("bridge_config")
    chassis_config = LaunchConfiguration("chassis_config")

    return LaunchDescription([
        DeclareLaunchArgument("bridge_config", default_value=default_bridge_config),
        DeclareLaunchArgument("chassis_config", default_value=default_chassis_config),
        Node(
            package="small_car_motion_controller",
            executable="small_car_motion_controller_node",
            name="small_car_motion_controller",
            output="screen",
            parameters=[default_motion_config],
        ),
        Node(
            package="smallcar_ros_and_mcu_bridge",
            executable="smallcar_ros_and_mcu_bridge_node",
            name="smallcar_ros_and_mcu_bridge",
            output="screen",
            parameters=[bridge_config, {"chassis_config": chassis_config}],
        ),
        ExecuteProcess(
            cmd=[
                "python3",
                "/workspace/rpi_host/tools/hermes_voice_daemon.py",
            ],
            output="screen",
            respawn=True,
            respawn_delay=3.0,
        ),
        IncludeLaunchDescription(PythonLaunchDescriptionSource(description_launch)),
    ])
