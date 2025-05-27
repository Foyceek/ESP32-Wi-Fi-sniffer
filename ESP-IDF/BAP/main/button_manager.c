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
#include "esp_log.h"
#include "display_queue.h"

static const char *TAG = "button_manager";

static button_t button_s1;
static button_t button_s2;

extern volatile bool stop_sniffer;
bool stop_sniffer_called = false;
bool double_hold_active = false;

static TimerHandle_t oled_timer = NULL;
int delay_state = 0; // 0: 5s, 1: 30s, 2: 1h

static int64_t buttons_pressed_start_time = 0;
static const int64_t PRESS_DURATION_MS = 2000;

static TimerHandle_t spam_timer = NULL;
static bool spam_timer_active = false;

static TaskHandle_t oled_task_handle = NULL;
bool powering_down = false;
bool cancel_powerdown = false;

int short_oled_period = CONFIG_SHORT_OLED_PERIOD;
int medium_oled_period = CONFIG_MEDIUM_OLED_PERIOD;
int long_oled_period = CONFIG_LONG_OLED_PERIOD;

static void reset_oled_timer(void) {
    if (oled_timer != NULL) {
        xTimerReset(oled_timer, 0);
    }
}

void update_display(void) {
    if (powering_down) {
        cancel_powerdown = true;
    }

    if (oled_task_handle != NULL) {
        xTaskNotifyGive(oled_task_handle);
    }

    reset_oled_timer();
}

// Helper function to check if button action should be ignored
static bool should_ignore_button_action(const char* action) {
    if (spam_timer_active || stop_sniffer_called) {
        ESP_LOGW(TAG, "%s ignored", action);
        return true;
    }
    return false;
}

// Helper function to activate spam protection and update display
static void activate_spam_protection_and_update(void) {
    spam_timer_active = true;
    xTimerReset(spam_timer, 0);
    update_display();
}

// Helper function to handle selection states
static bool handle_selection_states_s1(void) {
    if (settings_selection_active) {
        load_json = true;
        settings_selection_active = false;
        ESP_LOGI(TAG, "User selected to load json.");
        return true;
    }
    if (server_setup_selection_active) {
        use_server_setup = true;
        server_setup_selection_active = false;
        ESP_LOGI(TAG, "User selected server setup.");
        return true;
    }
    if (time_selection_active) {
        use_wifi_time = true;
        time_selection_active = false;
        ESP_LOGI(TAG, "User selected Wi-Fi time.");
        return true;
    }
    return false;
}

static bool handle_selection_states_s2(void) {
    if (settings_selection_active) {
        load_json = false;
        settings_selection_active = false;
        ESP_LOGI(TAG, "User selected not to load json.");
        return true;
    }
    if (server_setup_selection_active) {
        use_server_setup = false;
        server_setup_selection_active = false;
        ESP_LOGI(TAG, "User selected button setup.");
        return true;
    }
    if (time_selection_active) {
        use_wifi_time = false;
        time_selection_active = false;
        ESP_LOGI(TAG, "User selected default time.");
        return true;
    }
    return false;
}

static void spam_timer_callback(TimerHandle_t xTimer) {
    spam_timer_active = false;
}

static void oled_disconnect_callback(TimerHandle_t xTimer) {
    if (oled_initialized) {
        powering_down = true;
        printf("Starting OLED power-off countdown...\n");
        xTaskCreate(oled_countdown_task, "oled_countdown", 2048, NULL, 5, NULL);
    } else {
        printf("OLED is already disconnected.\n");
    }
}

// S1 Button Callbacks
static void button_s1_single_callback(void *arg) {
    if (handle_selection_states_s1()) return;
    
    if (!initial_selection) {
        if (should_ignore_button_action("S1 single press")) return;
        
        printf("Button S1 single press detected!\n");
        if (!powering_down && oled_initialized) {
            request_index++;
            if (request_index > max_request_rank) {
                request_index = 1;
            }
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

static void button_s1_medium_callback(void *arg) {
    if (!initial_selection) {
        if (should_ignore_button_action("S1 medium press")) return;
        
        printf("Button S1 medium press detected!\n");
        if (!powering_down && oled_initialized) {
            request_index = 1;
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

static void button_s1_long_callback(void *arg) {
    if (!initial_selection) {
        if (should_ignore_button_action("S1 long press")) return;
        
        printf("Button S1 long press detected!\n");
        if (!powering_down && oled_initialized) {
            delay_state = (delay_state + 1) % 3;
            
            TickType_t oled_period;
            int delay_seconds;
            
            switch (delay_state) {
                case 0: 
                    oled_period = pdMS_TO_TICKS(long_oled_period);
                    delay_seconds = long_oled_period / 1000;
                    break;
                case 1: 
                    oled_period = pdMS_TO_TICKS(medium_oled_period);
                    delay_seconds = medium_oled_period / 1000;
                    break;
                case 2: 
                    oled_period = pdMS_TO_TICKS(short_oled_period);
                    delay_seconds = short_oled_period / 1000;
                    break;
                default:
                    oled_period = pdMS_TO_TICKS(long_oled_period);
                    delay_seconds = long_oled_period / 1000;
                    break;
            }
            
            if (delay_seconds >= 3600) {
                printf("OLED disconnect delay set to %d hours.\n", delay_seconds / 3600);
            } else if (delay_seconds >= 60) {
                printf("OLED disconnect delay set to %d minutes.\n", delay_seconds / 60);
            } else {
                printf("OLED disconnect delay set to %d seconds.\n", delay_seconds);
            }
            
            if (oled_timer != NULL) {
                if (xTimerIsTimerActive(oled_timer)) {
                    xTimerStop(oled_timer, 0);
                }
                xTimerChangePeriod(oled_timer, oled_period, 0);
                xTimerStart(oled_timer, 0);
            }
        }
        activate_spam_protection_and_update();
    }
}

static void button_s1_double_callback(void *arg) {
    if (!initial_selection) {
        if (should_ignore_button_action("S1 double click")) return;
        
        printf("Button S1 double click detected!\n");
        if (!powering_down && oled_initialized) {
            request_index--;
            if (request_index < 1) {
                request_index = max_request_rank;
            }
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

// S2 Button Callbacks
static void button_s2_single_callback(void *arg) {
    if (handle_selection_states_s2()) return;
    
    if (!initial_selection) {
        if (should_ignore_button_action("S2 single press")) return;
        
        printf("Button S2 single press detected!\n");
        if (!powering_down && oled_initialized) {
            max_request_rank++;
            if (max_request_rank > TOP_REQUESTS_COUNT) {
                max_request_rank = 1;
            }
            if (request_index > max_request_rank) {
                request_index = max_request_rank;
            }
            printf("Max Request rank: %d\n", max_request_rank);    
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

static void button_s2_medium_callback(void *arg) {
    if (!initial_selection) {
        if (should_ignore_button_action("S2 medium press")) return;
        
        printf("Button S2 medium press detected!\n");
        if (!powering_down && oled_initialized) {
            if (request_index > max_request_rank) {
                request_index = TOP_REQUESTS_COUNT / 2;
            }
            max_request_rank = TOP_REQUESTS_COUNT / 2;   
            printf("Max Request rank: %d\n", max_request_rank);
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

static void button_s2_long_callback(void *arg) {       
    if (!initial_selection) {          
        if (should_ignore_button_action("S2 long press")) return;
        
        printf("Button S2 long press detected!\n");
        if (!powering_down && oled_initialized) {
            start_server = !start_server;
            printf("%s\n", start_server ? 
                "Stopping sniffer and starting webserver..." : 
                "Stopping webserver and resuming sniffer...");
        }
        activate_spam_protection_and_update();
    }
}

static void button_s2_double_callback(void *arg) {
    if (!initial_selection) {
        if (should_ignore_button_action("S2 double click")) return;
        
        printf("Button S2 double click detected!\n");
        if (!powering_down && oled_initialized) {
            max_request_rank--;
            if (max_request_rank < 1) {
                max_request_rank = TOP_REQUESTS_COUNT;
            }
            if (request_index > max_request_rank) {
                request_index = max_request_rank;
            }
            printf("Max Request rank: %d\n", max_request_rank);
            printf("Request index: %d\n", request_index);
        }
        activate_spam_protection_and_update();
    }
}

static void button_task(void *arg) {
    while (1) {
        int button_s1_state = gpio_get_level(CONFIG_GPIO_BUTTON_PIN_1);
        int button_s2_state = gpio_get_level(CONFIG_GPIO_BUTTON_PIN_2);

        if (button_s1_state == 0 && button_s2_state == 0) {
            if (buttons_pressed_start_time == 0) {
                buttons_pressed_start_time = esp_timer_get_time() / 1000;
            } else {
                int64_t elapsed_time = (esp_timer_get_time() / 1000) - buttons_pressed_start_time;
                if (elapsed_time >= PRESS_DURATION_MS && !stop_sniffer_called) {
                    printf("Both buttons pressed for %lld ms.\n", elapsed_time);
                    stop_sniffer = !stop_sniffer;
                    stop_sniffer_called = true;
                    double_hold_active = true;
                }
            }
        } else {
            buttons_pressed_start_time = 0;
            stop_sniffer_called = false;
            double_hold_active = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void oled_task(void *arg) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!oled_initialized) {
            gpio_set_level(OLED_POWER_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            oled_init("sh1106", flip_oled);
        }

        display_top_requests_oled();
        reset_oled_timer();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t button_manager_init(gpio_num_t button_pin_s1, gpio_num_t button_pin_s2) {
    // Initialize buttons and add callbacks
    esp_err_t ret = button_init(&button_s1, button_pin_s1, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S1 on GPIO %d: %s\n", button_pin_s1, esp_err_to_name(ret));
        return ret;
    }

    // Register S1 callbacks
    button_add_cb(&button_s1, BUTTON_CLICK_SINGLE, button_s1_single_callback, NULL);
    button_add_cb(&button_s1, BUTTON_CLICK_MEDIUM, button_s1_medium_callback, NULL);
    button_add_cb(&button_s1, BUTTON_CLICK_LONG, button_s1_long_callback, NULL);
    button_add_cb(&button_s1, BUTTON_CLICK_DOUBLE, button_s1_double_callback, NULL);

    ret = button_init(&button_s2, button_pin_s2, BUTTON_EDGE_FALLING, 10, 2048);
    if (ret != ESP_OK) {
        printf("Failed to initialize button S2 on GPIO %d: %s\n", button_pin_s2, esp_err_to_name(ret));
        return ret;
    }

    // Register S2 callbacks
    button_add_cb(&button_s2, BUTTON_CLICK_SINGLE, button_s2_single_callback, NULL);
    button_add_cb(&button_s2, BUTTON_CLICK_MEDIUM, button_s2_medium_callback, NULL);
    button_add_cb(&button_s2, BUTTON_CLICK_LONG, button_s2_long_callback, NULL);
    button_add_cb(&button_s2, BUTTON_CLICK_DOUBLE, button_s2_double_callback, NULL);

    #if USE_OLED
    xTaskCreate(oled_task, "oled_task", 4096, NULL, 5, &oled_task_handle);
    oled_timer = xTimerCreate("OLED Timer", pdMS_TO_TICKS(long_oled_period), pdFALSE, NULL, oled_disconnect_callback);
    if (oled_timer == NULL) {
        printf("Failed to create OLED timer\n");
        return ESP_FAIL;
    }
    xTimerStart(oled_timer, 0);
    #endif

    spam_timer = xTimerCreate("Spam Timer", pdMS_TO_TICKS(SPAM_BUTTON_PERIOD), pdFALSE, NULL, spam_timer_callback);
    if (spam_timer == NULL) {
        printf("Failed to create debounce timer for S1\n");
        return ESP_FAIL;
    }

    xTaskCreate(button_task, "button_task", 8192, NULL, 10, NULL);
    return ESP_OK;
}