/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef I2C_OLED_H
#define I2C_OLED_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "esp_lcd_panel_vendor.h"


// I2C configuration
#define I2C_BUS_PORT                0
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ  (400 * 1000)
#define EXAMPLE_PIN_NUM_SDA         18
#define EXAMPLE_PIN_NUM_SCL         23
#define EXAMPLE_PIN_NUM_RST         -1
#define EXAMPLE_I2C_HW_ADDR         0x3C

// LCD resolution configuration
#define EXAMPLE_LCD_H_RES           128
#define EXAMPLE_LCD_V_RES           CONFIG_EXAMPLE_SSD1306_HEIGHT

// Bit configuration
#define EXAMPLE_LCD_CMD_BITS        8
#define EXAMPLE_LCD_PARAM_BITS      8

// Function declarations
void oled_init(void);
void oled_display_text(lv_disp_t *disp, const char *text, bool scroll);
lv_disp_t* get_display_handle(void);

#endif // I2C_OLED_H