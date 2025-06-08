#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "esp_err.h"
#include "driver/gpio.h"

esp_err_t button_manager_init(gpio_num_t button_pin_s1, gpio_num_t button_pin_s2);
esp_err_t disconnect_timer_init(void);

#endif
