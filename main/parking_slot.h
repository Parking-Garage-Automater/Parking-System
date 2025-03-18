/**
 * @file parking_slot.h
 * @brief Parking slot data structures and functions
 */
#ifndef PARKING_SLOT_H
#define PARKING_SLOT_H

#include <stdbool.h>

typedef struct {
    char* slot_name;    /* Parking slot name */
    int trig_pin;       /* GPIO pin connected to TRIG */
    int echo_pin;       /* GPIO pin connected to ECHO */
    int led_red_pin;    /* GPIO pin for RED */
    int led_green_pin;  /* GPIO pin for GREEN */
    float distance;     /* Last measured distance */
    bool is_occupied;   /* Current slot status */
    bool is_valid;      /* Whether last reading was valid */
} parking_slot_t;

extern parking_slot_t parking_slots[];

extern const parking_slot_t parking_slots_config[];

void init_parking_slot(int slot_index);
void measure_distance(int slot_index);
void print_slot_status(int slot_index);
void print_parking_summary(void);
void send_initial_status(void);
int get_total_parking_slots(void);

#endif /* PARKING_SLOT_H */