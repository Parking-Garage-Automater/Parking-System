/**
 * @file http_client.c
 * @brief Implementation of HTTP client functions
 */
#include "http_client.h"
#include "config.h"
#include "wifi_manager.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

/* The HTTP Event Handler */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            if (evt->data) {
                ESP_LOGI(TAG, "Last error: 0x%x", *(int*)evt->data);
            }
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
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

bool send_parking_update(const char* spot_id, bool is_taken) {
    print_memory_stats("Before HTTP request");

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

    print_memory_stats("After HTTP request");
    return success;
}