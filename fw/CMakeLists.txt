cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH required.
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(shortcutpd C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# scan all source files in directory.
file(GLOB_RECURSE SRC_CPP "${PROJECT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE SRC_CC "${PROJECT_SOURCE_DIR}/src/*.c")

# merge two scan results.
set(ALL_SRCS ${SRC_CC} ${SRC_CPP})
add_executable(shortcutpd ${ALL_SRCS})

pico_enable_stdio_usb(shortcutpd 0)
target_link_libraries(shortcutpd
    pico_stdlib
    pico_unique_id
    pico_bootrom
    pico_multicore
    pico_sync
    tinyusb_device
    tinyusb_board
    hardware_gpio
    hardware_spi
)

add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=32)

# --> to make TinyUSB to pick up tusb_config.h file.
target_include_directories(shortcutpd
PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}/src
    #$ENV{PICO_SDK_PATH}/lib/tinyusb/hw
    #$ENV{PICO_SDK_PATH}/lib/tinyusb/src
)

pico_add_extra_outputs(shortcutpd)
    