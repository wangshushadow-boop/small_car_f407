cmake_minimum_required(VERSION 3.16)
project(smallcar_ros_and_mcu_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(trajectory_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

# 复用已经通过串口实车测试的主机端模块，避免维护第二份协议实现。
set(HOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_library(smallcar_mcu_host_modules STATIC
  ${HOST_ROOT}/modules/protocol/protocol.cpp
  ${HOST_ROOT}/modules/transport/serial_port.cpp
  ${HOST_ROOT}/modules/mcu_client/car_client.cpp
  ${HOST_ROOT}/modules/chassis/chassis_config.cpp
)
target_include_directories(smallcar_mcu_host_modules PUBLIC ${HOST_ROOT}/include)
target_compile_definitions(smallcar_mcu_host_modules PRIVATE _DEFAULT_SOURCE)
target_link_libraries(smallcar_mcu_host_modules PUBLIC yaml-cpp)

add_executable(smallcar_ros_and_mcu_bridge_node src/bridge_node.cpp)
target_link_libraries(smallcar_ros_and_mcu_bridge_node PRIVATE
  smallcar_mcu_host_modules
  ${diagnostic_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ${std_msgs_TARGETS}
  ${std_srvs_TARGETS}
  ${trajectory_msgs_TARGETS}
  ament_index_cpp::ament_index_cpp
  rclcpp::rclcpp
  sensor_msgs::sensor_msgs_library
  tf2_ros::tf2_ros
)

install(TARGETS smallcar_ros_and_mcu_bridge_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
install(FILES ${HOST_ROOT}/config/chassis_params.yaml
  DESTINATION share/${PROJECT_NAME}/config)

ament_package()
