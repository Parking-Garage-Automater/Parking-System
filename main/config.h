/**
 * @file config.h
 * @brief Configuration settings for the smart parking system
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/* ===== System Configuration ===== */
#define PARKING_THRESHOLD 10.0      /* Distance threshold in cm for parking slot status */
#define MAX_PARKING_SLOTS 2         /* Maximum number of parking slots supported */
#define UPDATE_INTERVAL_SEC 3000    /* Send parking updates every 3 seconds */

/* ===== Server Configuration ===== */
#define SERVER_URL "https://138.199.217.16"
#define PARKING_ENDPOINT "/pt/parking"

/* ===== WiFi Configuration ===== */
#define WIFI_SSID "PRKiPhone"
#define WIFI_PASS "prka1705"
#define WIFI_MAXIMUM_RETRY 5

/* ===== WiFi Event Group Bits ===== */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* ===== Logging Tag ===== */
#define TAG "PARKING_SYSTEM"

/* External declarations for shared resources */
extern EventGroupHandle_t wifi_event_group;

#endif /* CONFIG_H */