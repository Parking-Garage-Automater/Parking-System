/**
 * @file main.c
 * @brief Main application entry point for parking system
 */
#include "config.h"
#include "parking_slot.h"
#include "ultrasonic_sensor.h"
#include "led_control.h"
#include "wifi_manager.h"
#include "http_client.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main() {
    ESP_LOGI(TAG, "Initializing parking system...");
    esp_log_level_set("esp-tls", ESP_LOG_DEBUG);
    esp_log_level_set("TRANS_SSL", ESP_LOG_DEBUG);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_DEBUG);

    /* Initialize NVS flash */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize WiFi connection */
    ESP_LOGI(TAG, "Connecting to WiFi...");
    wifi_init_sta();

    /* Initialize all parking slots */
    int total_slots = get_total_parking_slots();
    for (int i = 0; i < total_slots; i++) {
        init_parking_slot(i);
    }

    ESP_LOGI(TAG, "Parking system initialized with %d slots", total_slots);

    /* Take initial measurements for all slots */
    for (int i = 0; i < total_slots; i++) {
        measure_distance(i);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    /* Send one-time initial status to server */
    send_initial_status();

    /* Main monitoring loop */
    while (1) {
        for (int i = 0; i < total_slots; i++) {
            measure_distance(i);
            print_slot_status(i);
            /* Introduce small delay between sensors to avoid interference */
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        print_parking_summary();

        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_SEC));
    }
}