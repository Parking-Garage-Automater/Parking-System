/**
 * @file ultrasonic_sensor.c
 * @brief Implementation of ultrasonic sensor functions
 */
#include "ultrasonic_sensor.h"
#include "config.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

float measure_ultrasonic_distance(int trig_pin, int echo_pin) {
    /* Send trigger pulse */
    gpio_set_level(trig_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(trig_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(trig_pin, 0);

    /* Wait for echo to start with timeout */
    uint32_t startTime = esp_timer_get_time();
    uint32_t timeout = startTime + 30000; /* 30ms timeout */

    while (gpio_get_level(echo_pin) == 0 && esp_timer_get_time() < timeout) { 
        startTime = esp_timer_get_time(); 
    }

    /* Measure echo pulse width with timeout */
    uint32_t endTime = startTime;
    while (gpio_get_level(echo_pin) == 1 && esp_timer_get_time() < timeout + 30000) { 
        endTime = esp_timer_get_time(); 
    }

    /* Calculate distance */
    uint32_t duration = endTime - startTime;
    float distance = (duration * 0.0343) / 2; /* Speed of sound = 343 m/s */
    
    return distance;
}