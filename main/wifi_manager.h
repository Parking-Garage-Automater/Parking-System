/**
 * @file wifi_manager.h
 * @brief WiFi connectivity management functions
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

void wifi_init_sta(void);
void print_memory_stats(char *event);

/* The WiFi event group that will be set when connected */
extern EventGroupHandle_t wifi_event_group;

#endif /* WIFI_MANAGER_H */