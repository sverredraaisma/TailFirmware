idf_component_register(
    SRCS
        "main.c"
        "ble_service.c"
        "servo.c"
        "drivers/i2c_mux.c"
        "drivers/encoder_driver.c"
        "drivers/imu_driver.c"
        "drivers/led_strip_driver.c"
        "motion/pid_controller.cpp"
        "motion/axis_controller.cpp"
        "motion/motion_system.cpp"
        "motion/patterns/static_pattern.cpp"
        "motion/patterns/wagging_pattern.cpp"
        "motion/patterns/loose_pattern.cpp"
        "led/led_matrix.cpp"
        "led/layer_compositor.cpp"
        "led/effects/rainbow_effect.cpp"
        "led/effects/static_color_effect.cpp"
        "led/effects/image_effect.cpp"
        "led/effects/audio_power_effect.cpp"
        "led/effects/audio_bar_effect.cpp"
        "led/effects/audio_freq_bars_effect.cpp"
        "config/fft_buffer.cpp"
    INCLUDE_DIRS "." "drivers" "motion" "led" "ble" "config"
    PRIV_REQUIRES nvs_flash bt driver esp_driver_ledc esp_driver_gpio
)

# Stub out files that don't exist yet so the build doesn't fail.
# As phases are implemented, real files replace the stubs.
set(STUB_FILES
    drivers/encoder_driver.c
    drivers/imu_driver.c
    drivers/led_strip_driver.c
    motion/pid_controller.cpp
    motion/axis_controller.cpp
    motion/motion_system.cpp
    motion/patterns/static_pattern.cpp
    motion/patterns/wagging_pattern.cpp
    motion/patterns/loose_pattern.cpp
    led/led_matrix.cpp
    led/layer_compositor.cpp
    led/effects/rainbow_effect.cpp
    led/effects/static_color_effect.cpp
    led/effects/image_effect.cpp
    led/effects/audio_power_effect.cpp
    led/effects/audio_bar_effect.cpp
    led/effects/audio_freq_bars_effect.cpp
    config/fft_buffer.cpp
)

foreach(STUB ${STUB_FILES})
    set(STUB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${STUB}")
    if(NOT EXISTS "${STUB_PATH}")
        if(STUB MATCHES "\\.cpp$")
            file(WRITE "${STUB_PATH}" "// Stub - to be implemented\n")
        else()
            file(WRITE "${STUB_PATH}" "// Stub - to be implemented\n")
        endif()
    endif()
endforeach()
