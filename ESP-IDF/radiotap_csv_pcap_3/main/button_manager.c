#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "button.h"
#include "button_manager.h"
#include "sniffer.h"
#include "config.h"
#include "i2c_oled.h"
#include "server.h"
#include "esp_timer.h"

static const char *TAG = "button_manager";

static button_t button_s1;
static button_t button_s2;

extern volatile bool stop_probing;

// Timer handle for the 5-second timer
static TimerHandle_t oled_timer = NULL;
bool oled_initialized = true;

// bool off_extended_delay = false; // Tracks toggle state
int delay_state = 0; // 0: 5s, 1: 30s, 2: 1h

// Variables to track button press timing
static int64_t buttons_pressed_start_time = 0; // Time when both buttons were first pressed
static const int64_t PRESS_DURATION_MS = 2000; // 2 seconds in milliseconds

// Timer handles for spam protection
static TimerHandle_t spam_timer = NULL;
static bool spam_timer_active = false;

// Debounce timer callback function
static void spam_timer_callback(TimerHandle_t xTimer) {
    spam_timer_active = false;
}

// Reset the timer each time a button is pressed
static void reset_oled_timer(void) {
    if (oled_timer != NULL) {
        xTimerReset(oled_timer, 0);
    }
}

// Function to update the display
static void update_display(void) {
    if (!oled_initialized) {
        request_index = 1;
        max_request_rank = 5;
        oled_initialized = true;
    }
    lv_disp_t *current_disp = get_display_handle();
    display_top_requests_oled(current_disp);
    reset_oled_timer();
}

// Timer callback function to disconnect OLED after 5 seconds
static void oled_disconnect_callback(TimerHandle_t xTimer) {
    printf("5 seconds passed, disconnecting OLED.\n");
    
    // Check if OLED was initialized before trying to disconnect
    if (oled_initialized) {
        oled_initialized = false;
        
        // Get the current display handle
        lv_disp_t *current_disp = get_display_handle();
        // Clear display content
        oled_display_text(current_disp, "", false);
        // ESP_ERROR_CHECK(lvgl_port_deinit());

        printf("OLED disconnected to save power.\n");
    } else {
        printf("OLED is already disconnected.\n");
    }

}

// Callback functions for button S1 events
static void button_s1_single_callback(void *arg) {

    if (spam_timer_active) {
        ESP_LOGW(TAG, "S1 single press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S1 single press detected!\n");
    request_index++;
    if (request_index > max_request_rank) {
        request_index = 1;
    }
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s1_medium_callback(void *arg) {

    if (spam_timer_active) {
        ESP_LOGW(TAG, "S1 medium press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S1 medium press detected!\n");
    request_index = 1;
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s1_long_callback(void *arg) {
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S1 long press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S1 long press detected!\n");

    // Cycle between 5 seconds, 30 seconds, and 1 hour
    delay_state = (delay_state + 1) % 3;

    TickType_t new_period = pdMS_TO_TICKS(SHORT_OLED_PERIOD);
    switch (delay_state) {
        case 0: 
            new_period = pdMS_TO_TICKS(SHORT_OLED_PERIOD); // 5 seconds
            printf("OLED disconnect delay set to 5 seconds.\n");
            break;
        case 1: 
            new_period = pdMS_TO_TICKS(MEDIUM_OLED_PERIOD); // 30 seconds
            printf("OLED disconnect delay set to 30 seconds.\n");
            break;
        case 2: 
            new_period = pdMS_TO_TICKS(LONG_OLED_PERIOD); // 1 hour (3600 seconds)
            printf("OLED disconnect delay set to 1 hour.\n");
            break;
    }

    // Update the timer period
    if (oled_timer != NULL) {
        if (xTimerIsTimerActive(oled_timer)) {
            xTimerStop(oled_timer, 0);  // Stop the timer before changing its period
        }
        xTimerChangePeriod(oled_timer, new_period, 0);  // Update the timer period
        xTimerStart(oled_timer, 0);  // Restart the timer
    }

    update_display();
}

static void button_s1_double_callback(void *arg) {
        
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S1 double click ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

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
            
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S2 single press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S2 single press detected!\n");
    max_request_rank++;
    if (max_request_rank > TOP_REQUESTS_COUNT) {
        max_request_rank = 1;
    }
    if (request_index > max_request_rank) {
        request_index = max_request_rank;
    }
    printf("Max Request rank: %d\n", max_request_rank);    
    printf("Request index: %d\n", request_index);
    update_display();
}

static void button_s2_medium_callback(void *arg) {
                
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S2 medium press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S2 medium press detected!\n");
    if (request_index > max_request_rank) {
        request_index = TOP_REQUESTS_COUNT / 2;
    }
    max_request_rank = TOP_REQUESTS_COUNT / 2;   
    printf("Max Request rank: %d\n", max_request_rank);
    printf("Request index: %d\n", request_index);
    update_display();
}

// Button callback for long press
static void button_s2_long_callback(void *arg) {
                    
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S2 long press ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S2 long press detected!\n");
    stop_probing = !stop_probing;
    if (stop_probing) {
        printf("Stopping sniffer and starting webserver...\n");
    } else {
        printf("Stopping webserver and resuming sniffer...\n");
    }
    update_display();
}

static void button_s2_double_callback(void *arg) {
                        
    if (spam_timer_active) {
        ESP_LOGW(TAG, "S2 double click ignored");
        return;
    }

    spam_timer_active = true;
    xTimerReset(spam_timer, 0);

    printf("Button S2 double click detected!\n");
    max_request_rank--;
    if (max_request_rank < 1) {
        max_request_rank = TOP_REQUESTS_COUNT;
    }
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
        // Read the state of both buttons
        int button_s1_state = gpio_get_level(CONFIG_GPIO_BUTTON_PIN_1);
        int button_s2_state = gpio_get_level(CONFIG_GPIO_BUTTON_PIN_2);

        if (button_s1_state == 0 && button_s2_state == 0) {
            // Both buttons are pressed
            if (buttons_pressed_start_time == 0) {
                // Record the time when both buttons are first pressed
                buttons_pressed_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            } else {
                // Check how long the buttons have been pressed
                int64_t elapsed_time = (esp_timer_get_time() / 1000) - buttons_pressed_start_time;
                if (elapsed_time >= PRESS_DURATION_MS) {
                    printf("Both buttons pressed for %lld ms. Restarting the ESP...\n", elapsed_time);
                    esp_restart(); // Restart the ESP32
                }
            }
        } else {
            // Reset the timer if either button is released
            buttons_pressed_start_time = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Task delay
    }
}

// Initialize button manager for both buttons and the timer
esp_err_t button_manager_init(gpio_num_t button_pin_s1, gpio_num_t button_pin_s2) {
    // Initialize button S1
    esp_err_t ret = button_init(&button_s1, button_pin_s1, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S1 on GPIO %d: %s\n", button_pin_s1, esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks for button S1
    ret = button_add_cb(&button_s1, BUTTON_CLICK_SINGLE, button_s1_single_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S1 single press\n"); }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_MEDIUM, button_s1_medium_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S1 medium press\n"); }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_LONG, button_s1_long_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S1 long press\n"); }
    ret = button_add_cb(&button_s1, BUTTON_CLICK_DOUBLE, button_s1_double_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S1 double click\n"); }

    // Initialize button S2
    ret = button_init(&button_s2, button_pin_s2, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S2 on GPIO %d: %s\n", button_pin_s2, esp_err_to_name(ret));
        return ret;
    }

    // Register callbacks for button S2
    ret = button_add_cb(&button_s2, BUTTON_CLICK_SINGLE, button_s2_single_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S2 single press\n"); }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_MEDIUM, button_s2_medium_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S2 medium press\n"); }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_LONG, button_s2_long_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S2 long press\n"); }
    ret = button_add_cb(&button_s2, BUTTON_CLICK_DOUBLE, button_s2_double_callback, NULL);
    if (ret != ESP_OK) { printf("Failed to add callback for S2 double click\n"); }

    // Create the timer for OLED power saving
    oled_timer = xTimerCreate("OLED Timer", pdMS_TO_TICKS(2*SHORT_OLED_PERIOD), pdFALSE, NULL, oled_disconnect_callback);
    if (oled_timer == NULL) {
        printf("Failed to create OLED timer\n");
        return ESP_FAIL;
    }

    // Create spam timers for each button
    spam_timer = xTimerCreate("Spam Timer", pdMS_TO_TICKS(SPAM_BUTTON_PERIOD), pdFALSE, NULL, spam_timer_callback);
    if (spam_timer == NULL) {
        printf("Failed to create debounce timer for S1\n");
        return ESP_FAIL;
    }

    // Start the timer
    xTimerStart(oled_timer, 0);

    // Create the button task
    xTaskCreate(button_task, "button_task", 8192, NULL, 10, NULL);

    return ESP_OK;
}