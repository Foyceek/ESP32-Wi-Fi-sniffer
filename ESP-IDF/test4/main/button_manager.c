#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "button.h"
#include "button_manager.h"
#include "sniffer.h"  // Include sniffer header to access global_index
#include "config.h"
#include "i2c_oled.h"

// Define your button structure
static button_t button_s1;

// Callback functions for each button event
static void button_s1_single_callback(void *arg) {
    printf("Button S1 single press detected!\n");
    request_index++;  // Increment the global index
    // Reset request_index back to 1 if it reaches 5
    if (request_index > 5) {
        request_index = 1;
    }
    printf("Request index: %d\n", request_index);  // Print the current value of the index
    lv_disp_t *current_disp = get_display_handle();  // Get the display handle
    display_top_requests_oled(current_disp);  // Display only the top request on the OLED
}

static void button_s1_medium_callback(void *arg) {
    printf("Button S1 medium press detected!\n");
}

static void button_s1_long_callback(void *arg) {
    printf("Button S1 long press detected!\n");
}

static void button_s1_double_callback(void *arg) {
    printf("Button S1 double click detected!\n");
    request_index--;  // Increment the global index
    // Reset request_index back to 1 if it reaches 5
    if (request_index < 1) {
        request_index = 5;
    }
    printf("Request index: %d\n", request_index);  // Print the current value of the index
    lv_disp_t *current_disp = get_display_handle();  // Get the display handle
    display_top_requests_oled(current_disp);  // Display only the top request on the OLED
}

// Background task to handle button events
static void button_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay to reduce CPU usage
    }
}

esp_err_t button_manager_init(gpio_num_t button_pin) {
    // Initialize button component with specified GPIO pin
    esp_err_t ret = button_init(&button_s1, button_pin, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button on GPIO %d: %s\n", button_pin, esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks for each button event type
    ret = button_add_cb(&button_s1, BUTTON_CLICK_SINGLE, button_s1_single_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add single press callback: %s\n", esp_err_to_name(ret));
    }

    ret = button_add_cb(&button_s1, BUTTON_CLICK_MEDIUM, button_s1_medium_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add medium press callback: %s\n", esp_err_to_name(ret));
    }

    ret = button_add_cb(&button_s1, BUTTON_CLICK_LONG, button_s1_long_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add long press callback: %s\n", esp_err_to_name(ret));
    }

    ret = button_add_cb(&button_s1, BUTTON_CLICK_DOUBLE, button_s1_double_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add double click callback: %s\n", esp_err_to_name(ret));
    }

    // Create a task for button handling
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    return ESP_OK;
}
