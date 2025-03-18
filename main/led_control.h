/**
 * @file led_control.h
 * @brief Functions for controlling LED indicators
 */
#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include <stdbool.h>

void set_led_color(int slot_index, bool red, bool green);

#endif /* LED_CONTROL_H */