#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "button.h"
#include "button_manager.h"
#include "sniffer.h"  // Include sniffer header to access global_index
#include "config.h"
#include "i2c_oled.h"
#include "server.h"
#include "config.h"

// Define button structures for S1 and S2
static button_t button_s1;
static button_t button_s2;

extern volatile bool stop_probing;
static bool toggle_reset = false;  // To toggle between stop and restart actions

// Function to update the display
static void update_display(void) {
    lv_disp_t *current_disp = get_display_handle();
    display_top_requests_oled(current_disp);
}

// Callback functions for button S1 events
static void button_s1_single_callback(void *arg) {
    printf("Button S1 single press detected!\n");
    request_index++;
    if (request_index > max_request_rank) {
        request_index = 1;
    }
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s1_medium_callback(void *arg) {
    printf("Button S1 medium press detected!\n");
    request_index = 1;
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s1_long_callback(void *arg) {
    printf("Button S1 long press detected!\n");
    clear_top_requests();
    update_display();
}

static void button_s1_double_callback(void *arg) {
    printf("Button S1 double click detected!\n");
    request_index--;
    if (request_index < 1) {
        request_index = max_request_rank;
    }
    printf("Request index: %d\n", request_index);
    update_display();
}

// Callback function for button S2 events
static void button_s2_single_callback(void *arg) {
    printf("Button S2 single press detected!\n");
    max_request_rank++;
    if (max_request_rank > TOP_REQUESTS_COUNT) {
        max_request_rank = 1;
    }
    // Ensure request_index stays within valid bounds if max_request_rank changes
    if (request_index > max_request_rank) {
        request_index = max_request_rank;
    }
    printf("Max Request rank: %d\n", max_request_rank);
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s2_medium_callback(void *arg) {
    printf("Button S2 medium press detected!\n");

    // Reset request_index and max_request_rank if request_index exceeds the threshold
    if (request_index > max_request_rank) {
        request_index = TOP_REQUESTS_COUNT / 2;
    }

    max_request_rank = TOP_REQUESTS_COUNT / 2;

    printf("Request index: %d\n", request_index);
    printf("Max Request rank: %d\n", max_request_rank);

    update_display();
}

// Button callback for long press
static void button_s2_long_callback(void *arg) {
    printf("Button S2 long press detected!\n");

    if (!toggle_reset) {
        // First press: stop probing
        stop_probing = true;
        printf("Sniffer stopped\n");
    } else {
        // Second press: restart the ESP32
        esp_restart();  // This will reset the ESP32
    }
    // Toggle state for the next press
    toggle_reset = !toggle_reset;
}

static void button_s2_double_callback(void *arg) {
    printf("Button S2 double click detected!\n");
    max_request_rank--;
    if (max_request_rank < 1) {
        max_request_rank = TOP_REQUESTS_COUNT;
    }
    // Adjust request_index if it's now out of bounds
    if (request_index > max_request_rank) {
        request_index = max_request_rank;
    }
    printf("Max Request rank: %d\n", max_request_rank);
    printf("Request index: %d\n", request_index);
    update_display();
}

// Background task to handle button events
static void button_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Delay to reduce CPU usage
    }
}

// Initialize button manager for both buttons
esp_err_t button_manager_init(gpio_num_t button_pin_s1, gpio_num_t button_pin_s2) {
    // Initialize button S1
    esp_err_t ret = button_init(&button_s1, button_pin_s1, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S1 on GPIO %d: %s\n", button_pin_s1, esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks for button S1
    ret = button_add_cb(&button_s1, BUTTON_CLICK_SINGLE, button_s1_single_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add single press callback for S1: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_MEDIUM, button_s1_medium_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add medium press callback for S1: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_LONG, button_s1_long_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add long press callback for S1: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_DOUBLE, button_s1_double_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add double click callback for S1: %s\n", esp_err_to_name(ret));
    }

    // Initialize button S2
    ret = button_init(&button_s2, button_pin_s2, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S2 on GPIO %d: %s\n", button_pin_s2, esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks for button S2
    ret = button_add_cb(&button_s2, BUTTON_CLICK_SINGLE, button_s2_single_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add single press callback for S2: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_MEDIUM, button_s2_medium_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add medium press callback for S2: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_LONG, button_s2_long_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add long press callback for S2: %s\n", esp_err_to_name(ret));
    }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_DOUBLE, button_s2_double_callback, NULL);
    if (ret != ESP_OK) {
        printf("Failed to add double click callback for S2: %s\n", esp_err_to_name(ret));
    }

    // Create a task for button handling
    xTaskCreate(button_task, "button_task", 8192, NULL, 10, NULL);

    return ESP_OK;
}
