# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS
        "../main/main.c"
        "../main/parking_slot.c" 
        "../main/ultrasonic_sensor.c" 
        "../main/led_control.c" 
        "../main/wifi_manager.c" 
        "../main/http_client.c" 
    INCLUDE_DIRS "."
    REQUIRES
        json
        esp_wifi
        esp_http_client
        nvs_flash
    PRIV_REQUIRES
        driver
        esp_timer
)