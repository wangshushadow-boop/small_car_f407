cmake_minimum_required(VERSION 3.16)
project(small_car_bridge LANGUAGES CXX)

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
find_package(std_srvs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(trajectory_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

# 复用已经通过串口实车测试的传输层，避免维护第二份协议实现。
set(HOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_library(small_car_transport STATIC
  ${HOST_ROOT}/src/protocol.cpp
  ${HOST_ROOT}/src/serial_port.cpp
  ${HOST_ROOT}/src/car_client.cpp
  ${HOST_ROOT}/src/chassis_config.cpp
)
target_include_directories(small_car_transport PUBLIC ${HOST_ROOT}/include)
target_compile_definitions(small_car_transport PRIVATE _DEFAULT_SOURCE)
target_link_libraries(small_car_transport PUBLIC yaml-cpp)

add_executable(small_car_bridge_node src/bridge_node.cpp)
target_link_libraries(small_car_bridge_node PRIVATE
  small_car_transport
  ${diagnostic_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ${std_srvs_TARGETS}
  ${trajectory_msgs_TARGETS}
  ament_index_cpp::ament_index_cpp
  rclcpp::rclcpp
  sensor_msgs::sensor_msgs_library
  tf2_ros::tf2_ros
)

install(TARGETS small_car_bridge_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
install(FILES ${HOST_ROOT}/config/chassis_params.yaml
  DESTINATION share/${PROJECT_NAME}/config)

ament_package()
