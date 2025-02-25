#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <curl/curl.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define PARKING_THRESHOLD 10.0  /* Distance threshold in cm for parking slot status */
#define MAX_PARKING_SLOTS 1     /* Maximum number of parking slots supported */
#define UPDATE_INTERVAL_SEC 3000   /* Send parking updates every 3 seconds */

static const char *TAG = "PARKING_SPOT_TRACKER_APP";
#define SERVER_URL "http://152.53.124.121:3000/"
#define PARKING_URL "parking"

typedef struct {
    char* slot_name;    /* Parking slot name */
    int trig_pin;       /* GPIO pin connected to TRIG */
    int echo_pin;       /* GPIO pin connected to ECHO */
    int led_pin;        /* GPIO pin connected to LED indicator */
    float distance;     /* Last measured distance */
    bool is_occupied;   /* Current slot status */
    bool is_valid;      /* Whether last reading was valid */
} parking_slot_t;

const parking_slot_t parking_slots_config[] = {
    {"Slot1", 5, 18, 8, 0, false, false},   /* Slot 1: TRIG on GPIO 5, ECHO on GPIO 18, LED on GPIO 8 */
    // {"Slot2", 19, 21, 4, 0, false, false},  /* Slot 2: TRIG on GPIO 19, ECHO on GPIO 21, LED on GPIO 4 */
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

    gpio_reset_pin(parking_slots[slot_index].led_pin);
    gpio_set_direction(parking_slots[slot_index].led_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(parking_slots[slot_index].led_pin, 0); /* Start with LED off */
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
    uint32_t timeout = startTime + 30000;
    
    while (gpio_get_level(echo_pin) == 0 && esp_timer_get_time() < timeout) { 
        startTime = esp_timer_get_time(); 
    }
    
    uint32_t endTime = startTime;
    while (gpio_get_level(echo_pin) == 1 && esp_timer_get_time() < timeout + 30000) { 
        endTime = esp_timer_get_time(); 
    }
    
    uint32_t duration = endTime - startTime;
    parking_slots[slot_index].distance = (duration * 0.0343) / 2;

    bool previous_state = parking_slots[slot_index].is_occupied;
    
    if (parking_slots[slot_index].distance > 400 || parking_slots[slot_index].distance < 2) {
        parking_slots[slot_index].is_valid = false;
    } else {
        parking_slots[slot_index].is_valid = true;
        parking_slots[slot_index].is_occupied = 
            (parking_slots[slot_index].distance < PARKING_THRESHOLD);
    }

    /* Update the LED state based on parking slot occupancy */
    if (parking_slots[slot_index].is_valid) {
        gpio_set_level(parking_slots[slot_index].led_pin, 
            parking_slots[slot_index].is_occupied ? 1 : 0);
        
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

/* Function to print the status of a single parking slot */
void print_slot_status(int slot_index) {
    if (slot_index >= TOTAL_PARKING_SLOTS) return;
    
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
    
    ESP_LOGI(TAG, "===== PARKING SUMMARY =====");
    ESP_LOGI(TAG, "Total slots: %d", (int)TOTAL_PARKING_SLOTS);
    ESP_LOGI(TAG, "Occupied: %d", occupied);
    ESP_LOGI(TAG, "Available: %d", available);
    ESP_LOGI(TAG, "Sensors with errors: %d", invalid);
    ESP_LOGI(TAG, "===========================");
}

/* Function to send initial status for all slots */
void send_initial_status() {
    ESP_LOGI(TAG, "Sending initial status for all parking slots");
    
    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
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

/* Function to send parking status update to the server */
bool send_parking_update(const char* spot_name, bool is_taken) {
    CURL *curl;
    CURLcode res;
    bool success = false;
    struct curl_slist *headers = NULL;
    char json_data[256];
    char url[256];
    
    snprintf(url, sizeof(url), "%s%s", SERVER_URL, PARKING_URL);
    
    snprintf(json_data, sizeof(json_data), "{\"spot\": \"%s\", \"taken\": %s}", 
        spot_name, is_taken ? "true" : "false");
    
    ESP_LOGI(TAG, "Sending update to %s: %s", url, json_data);
    
    curl = curl_easy_init();
    if (!curl) {
        ESP_LOGE(TAG, "Curl initialization failed");
        return false;
    }
    
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);       /* Set 10 seconds timeout */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L); /* Set 5 seconds connection timeout */
    
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        ESP_LOGE(TAG, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code >= 200 && http_code < 300) {
            ESP_LOGI(TAG, "Update sent successfully, HTTP response code: %ld", http_code);
            success = true;
        } else {
            ESP_LOGE(TAG, "Server returned error code: %ld", http_code);
        }
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return success;
}

void app_main() {
    ESP_LOGI(TAG, "Initializing parking system...");
    curl_global_init(CURL_GLOBAL_ALL);
    ESP_LOGI(TAG, "CURL initialized");


    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        init_parking_slot(i);
    }
    
    ESP_LOGI(TAG, "Parking system initialized with %d slots", (int)TOTAL_PARKING_SLOTS);
    
    /* Send one-time initial status to server */
    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        measure_distance(i);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    send_initial_status();

    while (1) {
        for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
            measure_distance(i);
            print_slot_status(i);
            vTaskDelay(pdMS_TO_TICKS(100)); // Introduce small delay between sensors to avoid interference
        }
        
        print_parking_summary();
        
        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_SEC));
    }

    curl_global_cleanup();
}