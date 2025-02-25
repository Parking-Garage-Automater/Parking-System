#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "cJSON.h"

#define PARKING_THRESHOLD 10.0      /* Distance threshold in cm for parking slot status */
#define MAX_PARKING_SLOTS 1         /* Maximum number of parking slots supported */
#define UPDATE_INTERVAL_SEC 3000    /* Send parking updates every 3 seconds */

#define SERVER_URL "http://152.53.124.121:3000"
#define PARKING_ENDPOINT "/parking"

static const char *TAG = "PARKING_SPOT_TRACKER_APP";

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

/* HTTP Event Handler */
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

/* Function to send parking status update to the server */
bool send_parking_update(const char* spot_id, bool is_taken) {
    char url[256];
    bool success = false;
    
    snprintf(url, sizeof(url), "%s%s", SERVER_URL, PARKING_ENDPOINT);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "spot", spot_id);
    cJSON_AddBoolToObject(root, "taken", is_taken);
    
    char *post_data = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Sending update to %s: %s", url, post_data);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
        
        if (status_code >= 200 && status_code < 300) {
            success = true;
        } else {
            ESP_LOGE(TAG, "Server returned error code: %d", status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    cJSON_free(post_data);
    cJSON_Delete(root);
    
    return success;
}

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

void app_main() {
    ESP_LOGI(TAG, "Initializing parking system...");

    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        init_parking_slot(i);
    }

    ESP_LOGI(TAG, "Parking system initialized with %d slots", (int)TOTAL_PARKING_SLOTS);

    for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
        measure_distance(i);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* Send one-time initial status to server */
    send_initial_status();

    while (1) {
        for (int i = 0; i < TOTAL_PARKING_SLOTS; i++) {
            measure_distance(i);
            print_slot_status(i);
            /* Introduce small delay between sensors to avoid interference */
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        print_parking_summary();

        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_SEC));
    }
}