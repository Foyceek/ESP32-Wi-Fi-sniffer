#include "i2c_oled.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <string.h>
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
#include "esp_mac.h"
#include "variables.h"
#include "config.h"
#include "display_queue.h"

static const char* TAG = "oled_display";
static u8g2_t u8g2;
bool oled_initialized = false;
static const u8g2_cb_t *current_rotation = U8G2_R0;

void oled_init(const char* display_type, bool flip_oled) {
    static bool i2c_initialized = false;
    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.bus.i2c.sda = PIN_SDA;
    u8g2_esp32_hal.bus.i2c.scl = PIN_SCL;
    u8g2_esp32_hal_init(u8g2_esp32_hal);
    
    if (strcmp(display_type, "sh1106") == 0) {
        u8g2_Setup_sh1106_i2c_128x64_noname_f(
            &u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
        } else if (strcmp(display_type, "ssd1306") == 0) {
            u8g2_Setup_ssd1306_i2c_128x32_univision_f(
                &u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
            } else {
                ESP_LOGE(TAG, "Unsupported display type: %s", display_type);
                return;
            }
            

    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    if(!i2c_initialized&&!flip_oled){
        u8g2_InitDisplay(&u8g2);
        u8g2_SetDisplayRotation(&u8g2, U8G2_R0);
        current_rotation = U8G2_R0;
    } else if (!i2c_initialized&&flip_oled){
        u8g2_InitDisplay(&u8g2);
        u8g2_SetDisplayRotation(&u8g2, U8G2_R2);
        current_rotation = U8G2_R2;
    } else if (i2c_initialized&&!flip_oled){
        u8g2_SetDisplayRotation(&u8g2, U8G2_R2);
        current_rotation = U8G2_R2;
    } else{
        u8g2_SetDisplayRotation(&u8g2, U8G2_R0);
        current_rotation = U8G2_R0;
    }
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_SendBuffer(&u8g2);

    oled_initialized = true;
    i2c_initialized = true;
    ESP_LOGI(TAG, "OLED initialized.");
}

void oled_clear_screen(void) {
    if (!oled_initialized) {
        ESP_LOGW(TAG, "OLED not initialized, skipping operation");
        return;
    }
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
}

void oled_power_save(void) {
    if (!oled_initialized) {
        ESP_LOGW(TAG, "OLED not initialized, skipping operation");
        return;
    }
    u8g2_SetPowerSave(&u8g2, 1);
}

void oled_flip(bool flip_oled) {
    if (!oled_initialized) {
        ESP_LOGW(TAG, "OLED not initialized, skipping operation");
        return;
    }

    // Set rotation based on input parameter
    if (flip_oled) {
        current_rotation = U8G2_R2;
    } else {
        current_rotation = U8G2_R0;
    }

    u8g2_SetDisplayRotation(&u8g2, current_rotation);
    i2c_task_send_display_text("Rotation updated!");
}

esp_err_t oled_display_text(const char* text) {
    if (!oled_initialized) {
        ESP_LOGW(TAG, "Attempted to display text before OLED was initialized");
        return ESP_ERR_INVALID_STATE;
    }

    u8g2_ClearBuffer(&u8g2);

    const int line_height = 12;
    const int max_width = 128;   // Display width in pixels
    const int max_lines = 5;     // Maximum number of lines
    
    int y = 10;  // Starting y position
    int line_count = 0;
    
    // Create a copy of the text that we can modify
    char* text_copy = strdup(text);
    if (!text_copy) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return ESP_ERR_NO_MEM;
    }
    
    char* line_start = text_copy;
    char* current = text_copy;
    
    while (*current && line_count < max_lines) {
        char* line_break = NULL;
        char* word_break = NULL;
        
        // Find the next natural line break or calculate where to wrap
        while (*current) {
            if (*current == '\n') {
                *current = '\0';  // Terminate at newline
                line_break = current;
                current++;        // Move past the newline
                break;
            }
            
            // Check if adding the next character would exceed the display width
            char tmp = *(current + 1);
            *(current + 1) = '\0';  // Temporarily terminate string
            int width = u8g2_GetStrWidth(&u8g2, line_start);
            *(current + 1) = tmp;   // Restore character
            
            if (width > max_width) {
                // Need to wrap - use the last word break if available
                if (word_break) {
                    line_break = word_break;
                    current = word_break + 1;  // Skip the space
                } else {
                    // Force break at current position if no word break available
                    line_break = current;
                    current++;
                }
                break;
            }
            
            // Mark potential word breaks (spaces)
            if (*current == ' ') {
                word_break = current;
            }
            
            current++;
        }
        
        // If we reached the end without finding a break point
        if (!line_break && *line_start) {
            line_break = current;
        }
        
        if (line_break) {
            char temp = *line_break;
            *line_break = '\0';  // Temporarily terminate the string
            
            // Draw the line
            u8g2_DrawStr(&u8g2, 0, y, line_start);
            y += line_height;
            line_count++;
            
            // Restore character if it wasn't a newline
            if (temp != '\0') {
                *line_break = temp;
            }
            
            // Move to next line
            line_start = (temp == '\0' || temp == '\n') ? line_break + 1 : line_break;
            
            // Skip leading spaces on new line
            while (*line_start == ' ') {
                line_start++;
            }
        } else {
            break;  // No more text to process
        }
    }
    
    // Handle case where there's more text than can fit
    if (*current && line_count >= max_lines) {
        // Display ellipsis on the last line to indicate more text
        int last_line_y = y - line_height;
        u8g2_DrawStr(&u8g2, max_width - u8g2_GetStrWidth(&u8g2, "..."), last_line_y, "...");
    }
    
    free(text_copy);
    u8g2_SendBuffer(&u8g2);
    return ESP_OK;
}