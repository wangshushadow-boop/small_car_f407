cmake_minimum_required(VERSION 3.16)

project(small_car_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SMALL_CAR_ENABLE_OPENCV "Build OpenCV camera tools" OFF)

add_library(small_car_protocol
  src/protocol.cpp
)
target_include_directories(small_car_protocol
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX)
  find_package(yaml-cpp REQUIRED)

  add_library(small_car_host
    src/audio_device.cpp
    src/car_client.cpp
    src/chassis_config.cpp
    src/serial_port.cpp
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
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_compile_definitions(small_car_host
    PRIVATE _DEFAULT_SOURCE
  )

  add_executable(small_car_host_cli
    src/main.cpp
  )
  target_link_libraries(small_car_host_cli
    PRIVATE small_car_host
  )

  add_executable(v4l2_capture
    tools/v4l2_capture.cpp
  )

  add_executable(audio_loopback
    tools/audio_loopback.cpp
  )
  target_link_libraries(audio_loopback
    PRIVATE small_car_host
  )

  add_executable(sensor_monitor
    tools/sensor_monitor.cpp
  )
  target_link_libraries(sensor_monitor
    PRIVATE small_car_host
  )
endif()

add_executable(protocol_test
  tests/protocol_test.cpp
)
target_link_libraries(protocol_test
  PRIVATE small_car_protocol
)

enable_testing()
add_test(NAME protocol_test COMMAND protocol_test)

if(SMALL_CAR_ENABLE_OPENCV)
  find_package(OpenCV REQUIRED COMPONENTS core videoio imgcodecs)

  add_executable(camera_capture
    tools/camera_capture.cpp
  )
  target_link_libraries(camera_capture
    PRIVATE ${OpenCV_LIBS}
  )
  target_include_directories(camera_capture
    PRIVATE ${OpenCV_INCLUDE_DIRS}
  )
endif()
