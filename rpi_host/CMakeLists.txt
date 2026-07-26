cmake_minimum_required(VERSION 3.16)

project(small_car_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMALL_CAR_ENABLE_OPENCV "Build OpenCV camera tools" OFF)

add_library(small_car_protocol
  src/small_car_base/protocol/protocol.cpp
)
target_include_directories(small_car_protocol
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(UNIX)
  find_package(yaml-cpp REQUIRED)

  add_library(small_car_host
    src/small_car_base/audio/audio_device.cpp
    src/small_car_base/chassis/chassis_config.cpp
    src/small_car_base/mcu/car_client.cpp
    src/small_car_base/transport/serial_port.cpp
  )
  target_link_libraries(small_car_host
    PUBLIC
      small_car_protocol
      yaml-cpp
  )
  find_package(ALSA REQUIRED)
  target_link_libraries(small_car_host
    PUBLIC ALSA::ALSA
  )
  target_include_directories(small_car_host
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(small_car_host
    PRIVATE _DEFAULT_SOURCE
  )

  add_executable(small_car_host_cli
    apps/cli/main.cpp
  )
  target_link_libraries(small_car_host_cli
    PRIVATE small_car_host
  )

  add_executable(v4l2_capture
    apps/v4l2_capture/v4l2_capture.cpp
  )

  add_executable(audio_loopback
    apps/audio_loopback/audio_loopback.cpp
  )
  target_link_libraries(audio_loopback
    PRIVATE small_car_host
  )

  add_executable(sensor_monitor
    apps/sensor_monitor/sensor_monitor.cpp
  )
  target_link_libraries(sensor_monitor
    PRIVATE small_car_host
  )
endif()

add_executable(protocol_test
  src/small_car_base/protocol/protocol_test.cpp
)
target_link_libraries(protocol_test
  PRIVATE small_car_protocol
)

add_executable(module_test
  src/small_car_base/control/module_test.cpp
  src/small_car_base/control/command_safety.cpp
  src/small_car_base/servo/gimbal_controller.cpp
)
target_include_directories(module_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

enable_testing()
add_test(NAME protocol_test COMMAND protocol_test)
add_test(NAME module_test COMMAND module_test)

if(SMALL_CAR_ENABLE_OPENCV)
  find_package(OpenCV REQUIRED COMPONENTS core videoio imgcodecs)

  add_executable(camera_capture
    apps/camera_capture/camera_capture.cpp
  )
  target_link_libraries(camera_capture
    PRIVATE ${OpenCV_LIBS}
  )
  target_include_directories(camera_capture
    PRIVATE ${OpenCV_INCLUDE_DIRS}
  )
endif()
