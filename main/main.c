#include <stdio.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define PARKING_THRESHOLD 10.0   /* Distance threshold in cm for parking slot status */
#define MAX_PARKING_SLOTS 1    /* Maximum number of parking slots supported */

typedef struct {
    char* slot_name;    /* Parking slot name */
    int trig_pin;       /* GPIO pin connected to TRIG */
    int echo_pin;       /* GPIO pin connected to ECHO */
    float distance;     /* Last measured distance*/
    bool is_occupied;   /* Current slot status */
    bool is_valid;      /* Whether last reading was valid */
} parking_slot_t;

const parking_slot_t parking_slots_config[] = {
    {"Slot1", 5, 18, 0, false, false},       /* Slot 1: TRIG on GPIO 5, ECHO on GPIO 18 */
    // {"Slot2", 19, 21, 0, false, false},   /* Slot 2: TRIG on GPIO 19, ECHO on GPIO 21 */
};

#define TOTAL_PARKING_SLOTS (sizeof(parking_slots_config) / sizeof(parking_slot_t))

/* Global array to track current slot status */
parking_slot_t parking_slots[MAX_PARKING_SLOTS];

/*  Function to initialise a single parking slot */
void init_parking_slot(int slot_index) {
    if (slot_index >= TOTAL_PARKING_SLOTS) return;
    
    parking_slots[slot_index] = parking_slots_config[slot_index];
    
    gpio_reset_pin(parking_slots[slot_index].trig_pin);
    gpio_set_direction(parking_slots[slot_index].trig_pin, GPIO_MODE_OUTPUT);
    
    gpio_reset_pin(parking_slots[slot_index].echo_pin);
    gpio_set_direction(parking_slots[slot_index].echo_pin, GPIO_MODE_INPUT);
}

/*  Function to measure distance for a single parking slot */
void measure_distance(int slot_index) {
    if (slot_index >= TOTAL_PARKING_SLOTS) return;
    
    int trig_pin = parking_slots[slot_index].trig_pin;
    int echo_pin = parking_slots[slot_index].echo_pin;
    
    gpio_set_level(trig_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(trig_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(trig_pin, 0);
    
    uint32_t startTime = esp_timer_get_time();
    uint32_t timeout = startTime + 30000; // 30ms timeout
    
    while (gpio_get_level(echo_pin) == 0 && esp_timer_get_time() < timeout) { 
        startTime = esp_timer_get_time(); 
    }
    
    uint32_t endTime = startTime;
    while (gpio_get_level(echo_pin) == 1 && esp_timer_get_time() < timeout + 30000) { 
        endTime = esp_timer_get_time(); 
    }
    
    uint32_t duration = endTime - startTime;
    parking_slots[slot_index].distance = (duration * 0.0343) / 2;
    
    if (parking_slots[slot_index].distance > 400 || parking_slots[slot_index].distance < 2) {
        parking_slots[slot_index].is_valid = false;
    } else {
        parking_slots[slot_index].is_valid = true;
        parking_slots[slot_index].is_occupied = 
            (parking_slots[slot_index].distance < PARKING_THRESHOLD);
    }
}

/* Function to print the status of a single parking slot */
void print_slot_status(int slot_index) {
    if (slot_index >= TOTAL_PARKING_SLOTS) return;
    
    printf("Parking status for %s: ", parking_slots[slot_index].slot_name);
    
    if (!parking_slots[slot_index].is_valid) {
        printf("ERROR! Invalid reading\n");
    } else {        
        if (parking_slots[slot_index].is_occupied) {
            printf("OCCUPIED\n");
        } else {
            printf("AVAILABLE\n");
        }
        printf("Distance: %.2f cm", parking_slots[slot_index].distance);
    }
}

/* Function to count and print overall parking lot status */
void print_parking_summary() {
    int occupied = 0;
    int available = 0;
    int invalid = 0;
    
    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        if (!parking_slots[i].is_valid) {
            invalid++;
        } else if (parking_slots[i].is_occupied) {
            occupied++;
        } else {
            available++;
        }
    }
    
    printf("\n\n===== PARKING SUMMARY =====\n");
    printf("Total slots: %d\n", (int)TOTAL_PARKING_SLOTS);
    printf("Occupied: %d\n", occupied);
    printf("Available: %d\n", available);
    printf("Sensors with errors: %d\n", invalid);
    printf("===========================\n\n");
}

void app_main() {
    // Initialize all parking slots
    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        init_parking_slot(i);
    }
    
    printf("Parking system initialized with %d slots\n", (int)TOTAL_PARKING_SLOTS);
    
    while (1) {
        for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
            measure_distance(i);
            print_slot_status(i);
            vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between sensors to avoid interference
        }
        
        // Print summary
        print_parking_summary();
        
        // Wait before next reading cycle
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}