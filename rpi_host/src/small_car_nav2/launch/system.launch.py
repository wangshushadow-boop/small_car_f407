import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, LoadComposableNodes, Node
from launch_ros.descriptions import ComposableNode
from launch_ros.parameter_descriptions import ParameterFile, ParameterValue


def generate_launch_description():
    nav2_share = get_package_share_directory("nav2_bringup")
    car_nav2_share = get_package_share_directory("small_car_nav2")
    description_share = get_package_share_directory("small_car_description")
    base_share = get_package_share_directory("small_car_base")

    params_file = LaunchConfiguration("params_file")
    use_sim_time = LaunchConfiguration("use_sim_time")
    configured_params = ParameterFile(params_file, allow_substs=True)
    robot_description = ParameterValue(
        Command([
            "xacro ",
            os.path.join(
                description_share, "urdf", "small_car.urdf.xacro"
            ),
        ]),
        value_type=str,
    )

    navigation = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_share, "launch", "navigation_launch.py")
        ),
        launch_arguments={
            "params_file": params_file,
            "use_sim_time": use_sim_time,
            "autostart": "True",
            "use_composition": "True",
            "container_name": "nav2_container",
            "use_respawn": "False",
        }.items(),
    )

    nav2_container = ComposableNodeContainer(
        name="nav2_container",
        namespace="",
        package="rclcpp_components",
        executable="component_container_isolated",
        parameters=[
            configured_params,
            {"autostart": True, "use_sim_time": use_sim_time},
        ],
        output="screen",
    )

    description_component = LoadComposableNodes(
        target_container="nav2_container",
        composable_node_descriptions=[
            ComposableNode(
                package="robot_state_publisher",
                plugin="robot_state_publisher::RobotStatePublisher",
                name="robot_state_publisher",
                parameters=[
                    {
                        "robot_description": robot_description,
                        "use_sim_time": use_sim_time,
                    }
                ],
                extra_arguments=[{"use_intra_process_comms": True}],
            ),
        ],
    )

    base_node = Node(
        package="small_car_base",
        executable="small_car_base_node",
        name="small_car_base",
        output="screen",
        parameters=[
            os.path.join(base_share, "config", "base.yaml"),
            {
                "chassis_config": os.path.join(
                    base_share, "config", "chassis.yaml"
                )
            },
        ],
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            "params_file",
            default_value=os.path.join(
                car_nav2_share, "config", "nav2.yaml"
            ),
        ),
        DeclareLaunchArgument("use_sim_time", default_value="false"),
        base_node,
        nav2_container,
        navigation,
        description_component,
    ])
