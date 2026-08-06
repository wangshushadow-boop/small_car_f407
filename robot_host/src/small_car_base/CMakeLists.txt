cmake_minimum_required(VERSION 3.16)
project(small_car_base LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(trajectory_msgs REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(small_car_core
  chassis/chassis_config.cpp
  control/command_safety.cpp
  mcu/car_client.cpp
  protocol/protocol.cpp
  servo/gimbal_controller.cpp
  transport/serial_port.cpp
)
target_include_directories(small_car_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
  $<INSTALL_INTERFACE:include>
)
target_compile_definitions(small_car_core PRIVATE _DEFAULT_SOURCE)
target_link_libraries(small_car_core PUBLIC yaml-cpp)

add_executable(small_car_base_node ros/base_node.cpp)
target_link_libraries(small_car_base_node PRIVATE
  small_car_core
  ${diagnostic_msgs_TARGETS}
  ${geometry_msgs_TARGETS}
  ${nav_msgs_TARGETS}
  ${rcl_interfaces_TARGETS}
  ${sensor_msgs_TARGETS}
  ${trajectory_msgs_TARGETS}
  ament_index_cpp::ament_index_cpp
  rclcpp::rclcpp
  sensor_msgs::sensor_msgs_library
)

if(BUILD_TESTING)
  add_executable(protocol_test protocol/protocol_test.cpp)
  target_link_libraries(protocol_test PRIVATE small_car_core)
  add_test(NAME protocol_test COMMAND protocol_test)

  add_executable(module_test control/module_test.cpp)
  target_link_libraries(module_test PRIVATE small_car_core)
  add_test(NAME module_test COMMAND module_test)

  add_executable(chassis_config_test chassis/chassis_config_test.cpp)
  target_link_libraries(chassis_config_test PRIVATE small_car_core)
  add_test(
    NAME chassis_config_test
    COMMAND chassis_config_test
            ${CMAKE_CURRENT_SOURCE_DIR}/config/chassis.yaml
  )
endif()

install(TARGETS small_car_core small_car_base_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY chassis control mcu protocol servo transport
  DESTINATION include/small_car_base
  FILES_MATCHING PATTERN "*.hpp"
)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_export_include_directories(include)
ament_export_libraries(small_car_core)
ament_package()
