/**
 * @file led_control.c
 * @brief Implementation of LED control functions
 */
#include "led_control.h"
#include "parking_slot.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"

void set_led_color(int slot_index, bool red, bool green) {
    if (slot_index >= get_total_parking_slots()) return;
    
    /* For common cathode, a HIGH turns on the LED */
    gpio_set_level(parking_slots[slot_index].led_red_pin, red);
    gpio_set_level(parking_slots[slot_index].led_green_pin, green);
    
    if (red && green) {
        ESP_LOGD(TAG, "Slot %d: LED set to INVALID", slot_index);
    } else if (red) {
        ESP_LOGD(TAG, "Slot %d: LED set to RED", slot_index);
    } else if (green) {
        ESP_LOGD(TAG, "Slot %d: LED set to GREEN", slot_index);
    } else {
        ESP_LOGD(TAG, "Slot %d: LED turned OFF", slot_index);
    }
}