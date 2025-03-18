/**
 * @file parking_slot.c
 * @brief Implementation of parking slot functions
 */
#include "parking_slot.h"
#include "config.h"
#include "ultrasonic_sensor.h"
#include "led_control.h"
#include "http_client.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Slot configuration array */
const parking_slot_t parking_slots_config[] = {
    {"Slot1", 5, 18, 8, 9, 0, false, false},   /* Slot 1 */
    {"Slot2", 19, 21, 4, 6, 0, false, false},  /* Slot 2 */
};

/* Runtime slot state array */
parking_slot_t parking_slots[MAX_PARKING_SLOTS];

int get_total_parking_slots(void) {
    return sizeof(parking_slots_config) / sizeof(parking_slot_t);
}

void init_parking_slot(int slot_index) {
    if (slot_index >= get_total_parking_slots()) return;

    /* Copy the configuration into the runtime state */
    parking_slots[slot_index] = parking_slots_config[slot_index];

    /* Initialize the ultrasonic sensor pins */
    gpio_reset_pin(parking_slots[slot_index].trig_pin);
    gpio_set_direction(parking_slots[slot_index].trig_pin, GPIO_MODE_OUTPUT);

    gpio_reset_pin(parking_slots[slot_index].echo_pin);
    gpio_set_direction(parking_slots[slot_index].echo_pin, GPIO_MODE_INPUT);

    /* Initialize the LED pins */
    gpio_reset_pin(parking_slots[slot_index].led_red_pin);
    gpio_set_direction(parking_slots[slot_index].led_red_pin, GPIO_MODE_OUTPUT);
    
    gpio_reset_pin(parking_slots[slot_index].led_green_pin);
    gpio_set_direction(parking_slots[slot_index].led_green_pin, GPIO_MODE_OUTPUT);
    
    /* Turn off all LEDs initially */
    set_led_color(slot_index, false, false);
}

void measure_distance(int slot_index) {
    if (slot_index >= get_total_parking_slots()) return;

    int trig_pin = parking_slots[slot_index].trig_pin;
    int echo_pin = parking_slots[slot_index].echo_pin;
    
    /* Get the distance measurement from the ultrasonic sensor */
    float distance = measure_ultrasonic_distance(trig_pin, echo_pin);
    parking_slots[slot_index].distance = distance;
    
    /* Store the previous state for change detection */
    bool previous_state = parking_slots[slot_index].is_occupied;

    /* Validate the reading */
    if (distance > 400 || distance < 2) {
        parking_slots[slot_index].is_valid = false;
    } else {
        parking_slots[slot_index].is_valid = true;
        parking_slots[slot_index].is_occupied = (distance < PARKING_THRESHOLD);
    }

    /* Update the LED state based on parking slot occupancy */
    if (parking_slots[slot_index].is_valid) {
        if (parking_slots[slot_index].is_occupied) {
            /* Occupied - glow RED */
            set_led_color(slot_index, true, false);
        } else {
            /* Available - glow GREEN */
            set_led_color(slot_index, false, true);
        }

        /* If state changed, send update to server */
        if (previous_state != parking_slots[slot_index].is_occupied) {
            ESP_LOGI(TAG, "%s status changed: %s -> %s", 
                parking_slots[slot_index].slot_name,
                previous_state ? "OCCUPIED" : "AVAILABLE",
                parking_slots[slot_index].is_occupied ? "OCCUPIED" : "AVAILABLE");

            bool update_success = send_parking_update(
                parking_slots[slot_index].slot_name, 
                parking_slots[slot_index].is_occupied
            );
            
            if (update_success) {
                ESP_LOGI(TAG, "Server update successful for %s", parking_slots[slot_index].slot_name);
            } else {
                ESP_LOGE(TAG, "Failed to update server for %s", parking_slots[slot_index].slot_name);
            }
        }
    }
}

void print_slot_status(int slot_index) {
    if (slot_index >= get_total_parking_slots()) return;

    ESP_LOGI(TAG, "Parking status for %s: ", parking_slots[slot_index].slot_name);

    if (!parking_slots[slot_index].is_valid) {
        ESP_LOGE(TAG, "ERROR! Invalid reading");
    } else {        
        if (parking_slots[slot_index].is_occupied) {
            ESP_LOGI(TAG, "OCCUPIED");
        } else {
            ESP_LOGI(TAG, "AVAILABLE");
        }
        ESP_LOGI(TAG, "Distance: %.2f cm", parking_slots[slot_index].distance);
    }
    ESP_LOGI(TAG, "\n\n");
}

void print_parking_summary(void) {
    int occupied = 0;
    int available = 0;
    int invalid = 0;
    int total = get_total_parking_slots();

    for (int i = 0; i < total; i++) {
        if (!parking_slots[i].is_valid) {
            invalid++;
        } else if (parking_slots[i].is_occupied) {
            occupied++;
        } else {
            available++;
        }
    }

    ESP_LOGI(TAG, "\n\n===== PARKING SUMMARY =====");
    ESP_LOGI(TAG, "Total slots: %d", total);
    ESP_LOGI(TAG, "Occupied: %d", occupied);
    ESP_LOGI(TAG, "Available: %d", available);
    ESP_LOGI(TAG, "Sensors with errors: %d", invalid);
    ESP_LOGI(TAG, "===========================\n\n");
}

void send_initial_status(void) {
    ESP_LOGI(TAG, "Sending initial status for all parking slots");

    for (int i = 0; i < get_total_parking_slots(); i++) {
        if (parking_slots[i].is_valid) {
            bool update_success = send_parking_update(
                parking_slots[i].slot_name, 
                parking_slots[i].is_occupied
            );

            if (update_success) {
                ESP_LOGI(TAG, "Initial update successful for %s", parking_slots[i].slot_name);
            } else {
                ESP_LOGE(TAG, "Failed to send initial update for %s", parking_slots[i].slot_name);
            }
        }
    }
}