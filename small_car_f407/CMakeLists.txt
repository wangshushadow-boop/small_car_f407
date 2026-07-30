cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME small_car_f407)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# system_stm32f4xx.c is compiled inside this object library, so the application
# vector-table relocation must be applied to the library target itself.
target_compile_definitions(STM32_Drivers PRIVATE
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=0x00020000U
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Actuator/Src/gamepad_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Actuator/Src/servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Src/chassis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Src/chassis_auto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Src/chassis_manual.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Src/chassis_params.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Src/control_mux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Comm/Src/debug_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Comm/Src/host_link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Comm/Src/raspi_link.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Comm/Src/system_status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Display/Src/oled.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Input/Src/gamepad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Input/Src/gamepad_usb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Motor/Src/encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Motor/Src/motor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Motor/Src/wheel_speed_controller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Sensors/Src/battery_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Sensors/Src/icm20948.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Sensors/Src/ultrasonic.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Actuator/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Chassis/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Comm/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Display/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Input/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Motor/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Modules/Sensors/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# CubeMX 的默认链接脚本是唯一源文件。每次配置时自动复制最新段定义，
# 只替换 OTA 所需的 Flash 分区和复位后保留的 16 字节 RAM。
set(CUBEMX_LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/STM32F407XX_FLASH.ld
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CUBEMX_LINKER_SCRIPT}
)

function(generate_ota_linker output_path flash_origin flash_length)
    file(READ ${CUBEMX_LINKER_SCRIPT} linker_content)
    string(REPLACE "\r\n" "\n" linker_content "${linker_content}")

    string(REGEX MATCHALL
        "\nRAM[ \t]*\\(xrw\\)[ \t]*:[^\n]*"
        ram_matches "${linker_content}"
    )
    list(LENGTH ram_matches ram_match_count)
    string(REGEX MATCHALL
        "\nFLASH[ \t]*\\(rx\\)[ \t]*:[^\n]*"
        flash_matches "${linker_content}"
    )
    list(LENGTH flash_matches flash_match_count)
    if(NOT ram_match_count EQUAL 1 OR NOT flash_match_count EQUAL 1)
        message(FATAL_ERROR
            "CubeMX linker MEMORY format changed; cannot generate OTA linker safely"
        )
    endif()

    string(REGEX REPLACE
        "\nRAM[ \t]*\\(xrw\\)[ \t]*:[^\n]*"
        "\nRAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K - 16"
        linker_content "${linker_content}"
    )
    string(REGEX REPLACE
        "\nFLASH[ \t]*\\(rx\\)[ \t]*:[^\n]*"
        "\nFLASH (rx)     : ORIGIN = ${flash_origin}, LENGTH = ${flash_length}"
        linker_content "${linker_content}"
    )
    file(WRITE ${output_path} "${linker_content}")
endfunction()

set(GENERATED_LINKER_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/linker)
file(MAKE_DIRECTORY ${GENERATED_LINKER_DIR})
set(APP_LINKER_SCRIPT ${GENERATED_LINKER_DIR}/STM32F407XX_APP.ld)
set(BOOTLOADER_LINKER_SCRIPT
    ${GENERATED_LINKER_DIR}/STM32F407XX_BOOTLOADER.ld
)
generate_ota_linker(${APP_LINKER_SCRIPT} 0x08020000 384K)
generate_ota_linker(${BOOTLOADER_LINKER_SCRIPT} 0x08000000 64K)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -T${APP_LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_PROJECT_NAME}.map
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
)

add_executable(small_car_bootloader
    ${CMAKE_CURRENT_SOURCE_DIR}/startup_stm32f407xx.s
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/system_stm32f4xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Bootloader/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Bootloader/Src/sha256.c
)
target_include_directories(small_car_bootloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Bootloader/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(small_car_bootloader PRIVATE STM32F407xx)
target_link_options(small_car_bootloader PRIVATE
    -T${BOOTLOADER_LINKER_SCRIPT}
    -Wl,-Map=small_car_bootloader.map
)
target_link_libraries(small_car_bootloader m)
set_target_properties(small_car_bootloader PROPERTIES
    ADDITIONAL_CLEAN_FILES "small_car_bootloader.map"
)
add_custom_command(TARGET small_car_bootloader POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:small_car_bootloader>
            ${CMAKE_CURRENT_BINARY_DIR}/small_car_bootloader.bin
)
