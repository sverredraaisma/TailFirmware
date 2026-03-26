cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w)

include(pico_sdk_import.cmake)

project(TailFirmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(TailFirmware
        src/main.c
        src/servo.c
        src/ble_service.c
)

target_include_directories(TailFirmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(TailFirmware
        pico_stdlib
        hardware_pwm
        hardware_clocks
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
)

pico_btstack_make_gatt_header(TailFirmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/tail_service.gatt
)

pico_add_extra_outputs(TailFirmware)

# USB serial for debug output
pico_enable_stdio_usb(TailFirmware 1)
pico_enable_stdio_uart(TailFirmware 0)
