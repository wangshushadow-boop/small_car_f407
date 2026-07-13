cmake_minimum_required(VERSION 3.16)

project(small_car_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(small_car_protocol
  src/protocol.cpp
)
target_include_directories(small_car_protocol
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(UNIX)
  add_library(small_car_host
    src/car_client.cpp
    src/serial_port.cpp
  )
  target_link_libraries(small_car_host
    PUBLIC small_car_protocol
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
endif()

add_executable(protocol_test
  tests/protocol_test.cpp
)
target_link_libraries(protocol_test
  PRIVATE small_car_protocol
)

enable_testing()
add_test(NAME protocol_test COMMAND protocol_test)
