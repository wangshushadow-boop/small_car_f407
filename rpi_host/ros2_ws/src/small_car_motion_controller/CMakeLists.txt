cmake_minimum_required(VERSION 3.8)
project(small_car_motion_controller)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

add_executable(small_car_motion_controller_node src/motion_controller_node.cpp)
target_compile_features(small_car_motion_controller_node PUBLIC cxx_std_17)
target_link_libraries(small_car_motion_controller_node PRIVATE
  ${geometry_msgs_TARGETS}
  ${nav2_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  rclcpp::rclcpp
  rclcpp_action::rclcpp_action
  ${sensor_msgs_TARGETS}
  ${std_msgs_TARGETS}
)

install(TARGETS small_car_motion_controller_node
  DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME})

ament_package()
