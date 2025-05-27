#ifndef I2C_OLED_H
#define I2C_OLED_H

#include "esp_err.h"
#include <stdbool.h>

// Initialize OLED display
void oled_init(const char* display_type, bool flip_oled);

// Clear the screen
void oled_clear_screen(void);
void oled_flip(bool flip_display);

// Display text on the screen
esp_err_t oled_display_text(const char *text);

void oled_power_save(void);

#endif /* I2C_OLED_H */