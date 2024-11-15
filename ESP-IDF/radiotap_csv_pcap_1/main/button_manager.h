#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "esp_err.h"
#include "driver/gpio.h"

// Initializes the button with the specified GPIO pin and necessary callbacks
esp_err_t button_manager_init(gpio_num_t button_pin_s1, gpio_num_t button_pin_s2);

#endif // BUTTON_MANAGER_H
