/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "config.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"
#include "wifi_connect.h"
#include "esp_sntp.h"
#include "pcap_lib.h"
#include "sniffer.h"
#include "esp_mac.h"
#include "i2c_oled.h"
#include "button_manager.h"
#include "server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "display_queue.h"
#include "battery.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "driver/rtc_io.h"
#include "esp_cpu.h"
#include "esp_private/rtc_clk.h"
#include "soc/rtc_periph.h"

/* Defines -------------------------------------------------------------------*/
#define ESP_INTR_FLAG_DEFAULT 0

/* Global variables-----------------------------------------------------------*/
static const char *TAG = "main";

bool start_server = false;
volatile bool stop_sniffer = false;

static volatile bool change_file = false;

static bool sd_mounted = false;

bool initial_selection = false;

bool time_selection_active = false;
bool use_wifi_time = true;

bool server_setup_selection_active = false;
bool use_server_setup = true;

bool settings_selection_active = false;
bool load_json = true;

bool sniffer_running = false;
bool server_running = false;

bool enter_deep_sleep_flag = false;

bool flip_oled = FLIP_OLED;

char wifi_ssid[64] = CONFIG_WIFI_SSID;
char wifi_password[64] = CONFIG_WIFI_PASSWORD;

char server_wifi_ssid[64] = SERVER_WIFI_SSID;
char server_wifi_password[64] = SERVER_WIFI_PASSWORD;

/* Function prototypes -------------------------------------------------------*/
static void obtain_time(void);
static void initialize_LED(void);
static void initialize_OLED_gpio(void);
static void initialize_BAT_gpio(void);
static void initialize_nvs(void);
static void initialize_sntp(void);
static void initialize_wifi(void);
static bool mount_sd(void);
static bool unmount_sd(void);
static uint32_t get_file_index(uint32_t max_files);
void set_default_time(void);
void enter_deep_sleep(void);
void check_wake_up_reason(void);
void get_current_rtc_time_string(char* buffer, size_t buffer_size, bool rtc_valid);

/* Task prototypes -----------------------------------------------------------*/
static void save_task(void* arg);

/* Main function -------------------------------------------------------------*/
void app_main(void)
{
    check_wake_up_reason();
    
    // Set timezone so RTC time is displayed correctly
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    
    // Initialize RTC and check if time is preserved
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Variables to store RTC time info for display
    bool rtc_time_valid = false;
    
    // If year is still 1970, RTC time was lost
    if (timeinfo.tm_year < (2024 - 1900)) {
        ESP_LOGI(TAG, "RTC time lost, will need to sync time");
        rtc_time_valid = false;
    } else {
        ESP_LOGI(TAG, "RTC time preserved: %04d-%02d-%02d %02d:%02d:%02d",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        rtc_time_valid = true;
    }

    i2c_task_init();
    initialize_BAT_gpio();

    initialize_OLED_gpio();
    vTaskDelay(pdMS_TO_TICKS(100));
    oled_init("sh1106",flip_oled);
    
    // Display initialization message with current RTC time
    char init_message[128];
    if (rtc_time_valid) {
        time_t current_time_init;
        struct tm current_timeinfo;
        time(&current_time_init);
        localtime_r(&current_time_init, &current_timeinfo);
        snprintf(init_message, sizeof(init_message), "Initializing...\nRTC: %02d/%02d %02d:%02d:%02d",
                current_timeinfo.tm_mday, current_timeinfo.tm_mon + 1,
                current_timeinfo.tm_hour, current_timeinfo.tm_min, current_timeinfo.tm_sec);
    } else {
        strcpy(init_message, "Initializing...\nRTC: Time not set");
    }
    i2c_task_send_display_text(init_message);
    
    // Add a small delay to show the RTC time
    vTaskDelay(pdMS_TO_TICKS(1000));

    time_t current_time;
    char strftime_buf[64];
    uint32_t file_idx = 0;

    // State tracking variables
    bool sniffer_running = false;
    bool server_running = false;

    // Initialize peripherals and time
    #if CONFIG_STATUS_LED
    initialize_LED();
    #endif

    #if USE_BUTTONS
    // Define GPIO pins for both buttons
    gpio_num_t button_gpio_pin_s1 = CONFIG_GPIO_BUTTON_PIN_1;
    gpio_num_t button_gpio_pin_s2 = CONFIG_GPIO_BUTTON_PIN_2; 

    // Initialize button manager with both GPIO pins
    esp_err_t ret = button_manager_init(button_gpio_pin_s1, button_gpio_pin_s2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button manager initialization failed on pins %d and %d", button_gpio_pin_s1, button_gpio_pin_s2);
        return;
    }
    #endif

    // SD card setup and file ID check
    sd_mounted = mount_sd();
    
    if (sd_mounted == false)
    {
        const char *text_unmounted = "Failed to mount SD\nCheck and Restart";
        i2c_task_send_display_text(text_unmounted);
        return;
    }
    file_idx = get_file_index(65535);
    initial_selection = true;

    // Check if settings.json exists
    FILE *f = fopen(CONFIG_SD_MOUNT_POINT "/settings.json", "r");
    if (f) {
        fclose(f);

        // Ask user with current RTC time displayed
        char settings_message[128];
        char rtc_time_str[32];
        get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
        snprintf(settings_message, sizeof(settings_message), 
                "Load settings.json from SD card?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
        i2c_task_send_display_text(settings_message);
        
        settings_selection_active = true;
        // Wait for the user to respond - update time display every second
        while (settings_selection_active) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            if (settings_selection_active) { // Check again in case user responded
                get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
                snprintf(settings_message, sizeof(settings_message), 
                        "Load settings.json from SD card?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
                i2c_task_send_display_text(settings_message);
            }
        }

        if (load_json) {
            load_settings_from_sd(CONFIG_SD_MOUNT_POINT "/settings.json");
            printf("Settings loaded from SD card.\n");
        }
    }

    // Server setup selection with updating RTC time
    char server_message[128];
    char rtc_time_str[32];
    get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
    snprintf(server_message, sizeof(server_message), 
            "Launch server to customize settings?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
    i2c_task_send_display_text(server_message);
    
    server_setup_selection_active = true;

    // Wait for the user to respond - update time display every second
    while (server_setup_selection_active) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (server_setup_selection_active) { // Check again in case user responded
            get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
            snprintf(server_message, sizeof(server_message), 
                    "Launch server to customize settings?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
            i2c_task_send_display_text(server_message);
        }
    }

    // Ask about time synchronization with updating RTC time
    char time_message[128];
    get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
    snprintf(time_message, sizeof(time_message), 
            "Use Wi-Fi to get time?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
    i2c_task_send_display_text(time_message);
    
    time_selection_active = true;
    
    // Wait for the user to respond - update time display every second
    while (time_selection_active) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (time_selection_active) { // Check again in case user responded
            get_current_rtc_time_string(rtc_time_str, sizeof(rtc_time_str), rtc_time_valid);
            snprintf(time_message, sizeof(time_message), 
                    "Use Wi-Fi to get time?\n%s\nB1 = Yes\nB2 = No", rtc_time_str);
            i2c_task_send_display_text(time_message);
        }
    }

    // Initialize NVS - needed for both server and sniffer modes
    initialize_nvs();

    if (use_server_setup) {
        // Server mode path - need complete WiFi initialization for AP mode
        start_server = true;
        
        // Get time
        obtain_time();
        
        // Initialize WiFi completely
        time(&current_time);
        // Timezone already set at startup
        
        initialize_wifi();
        initialize_sniffer();
        
    } else {
        // Get time
        obtain_time();

        // update 'current_time' variable with current time
        time(&current_time);

        // Timezone already set at startup
        localtime_r(&current_time, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Europe is: %s", strftime_buf);

        // Open first pcap file and start sniffer
        ESP_ERROR_CHECK(pcap_open(file_idx));
        initialize_wifi();
        initialize_sniffer();
        ESP_ERROR_CHECK(sniffer_start());
        sniffer_running = true;

        const char *text_sniff = "Sniffing...";
        i2c_task_send_display_text(text_sniff);

        #if CONFIG_STATUS_LED
        // Turn off LED when set up ends
        ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_OFF));
        #endif
    }
    
    //start periodical save task
    xTaskCreate(save_task, "save_task", 1024, NULL, 10, NULL);
    initial_selection = false;

    #if USE_OLED
    disconnect_timer_init();
    #endif

    while (true) {
        // Check for deep sleep request first
        if (enter_deep_sleep_flag) {
            enter_deep_sleep(); 
        }

        if (stop_sniffer) {
            ESP_LOGI(TAG, "Stopping application...");
            
            // Stop the sniffer if running
            if (sniffer_running) {
                ESP_ERROR_CHECK(sniffer_stop());
                ESP_ERROR_CHECK(pcap_close());
                sniffer_running = false;
            }

            // Stop the webserver if running
            if (server_running) {
                stop_captive_server();
                server_running = false;
                ESP_LOGI(TAG, "Webserver stopped.");
            }
            
            ESP_ERROR_CHECK(esp_wifi_stop());
            ESP_LOGI(TAG, "Wi-Fi stopped.");

            vTaskDelay(500 / portTICK_PERIOD_MS);

            // Unmount the SD card
            if (sd_mounted) {
                esp_err_t err = unmount_sd();
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "SD card unmounted successfully.");
                } else {
                    ESP_LOGE(TAG, "Failed to unmount SD card. Error: %s", esp_err_to_name(err));
                }
            }
            const char *text_stop = "Application stopped.";
            i2c_task_send_display_text(text_stop);

            break; // Exit the while loop
        }

        if (start_server) {
            // Stop the sniffer if it's running
            if (sniffer_running) {
                ESP_LOGI(TAG, "Stopping sniffer to start server...");
                ESP_ERROR_CHECK(sniffer_stop());
                ESP_ERROR_CHECK(pcap_close());
                sniffer_running = false;
            }

            char text_stop[100];
            sprintf(text_stop, "Webserver started\n%s", ESP_AP_IP);
            i2c_task_send_display_text(text_stop);

            // Start the webserver
            esp_err_t ret = start_captive_server();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start webserver: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "Webserver started successfully.");
                server_running = true;
            }

            // Monitor both start_server and stop_sniffer
            while (start_server && !stop_sniffer) {
                vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for state change
            }

            // If stop_sniffer was set, exit
            if (stop_sniffer) {
                ESP_LOGI(TAG, "Stopping application while in webserver mode...");
                if (server_running) {
                    stop_captive_server();
                    server_running = false;
                }

                // Stop Wi-Fi before unmounting SD card
                ESP_ERROR_CHECK(esp_wifi_stop());
                ESP_LOGI(TAG, "Wi-Fi stopped.");

                
                vTaskDelay(500 / portTICK_PERIOD_MS);

                // Unmount the SD card
                if (sd_mounted) {
                    esp_err_t err = unmount_sd();
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "SD card unmounted successfully.");
                    } else {
                        ESP_LOGE(TAG, "Failed to unmount SD card. Error: %s", esp_err_to_name(err));
                    }
                }

                break;
            }

            // Stop the webserver if start_server was toggled off
            if (server_running) {
                stop_captive_server();
                server_running = false;
                ESP_LOGI(TAG, "Webserver stopped. Preparing to resume sniffer...");
            }

            const char *text_sniff = "Sniffing...";
            i2c_task_send_display_text(text_sniff);

            // Resume the sniffer
            if (!sniffer_running) {
                ESP_LOGI(TAG, "Restarting sniffer...");
                ESP_ERROR_CHECK(pcap_open(get_file_index(65535))); // Get next file index
                ESP_ERROR_CHECK(sniffer_start());
                sniffer_running = true;
            }
        }

        if (change_file) {
            if (sniffer_running) {
                ESP_ERROR_CHECK(sniffer_stop());
                ESP_ERROR_CHECK(pcap_close());
                ESP_ERROR_CHECK(pcap_open(++file_idx));
                ESP_ERROR_CHECK(sniffer_start());
            }
            change_file = false;
        }

        #if CONFIG_ALL_CHANNEL_SCAN
        // Only scan channels if sniffer is running
        if (sniffer_running) {
            for (int chan = 1; chan <= 10; chan++) {
                vTaskDelay(5);
                ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
            }
        }
        #endif

        vTaskDelay(5);
    }
}

/* Function definitions ------------------------------------------------------*/
void get_current_rtc_time_string(char* buffer, size_t buffer_size, bool rtc_valid) {
    if (rtc_valid) {
        time_t current_time_temp;
        struct tm current_timeinfo;
        time(&current_time_temp);
        localtime_r(&current_time_temp, &current_timeinfo);
        snprintf(buffer, buffer_size, "RTC: %02d/%02d %02d:%02d:%02d",
                current_timeinfo.tm_mday, current_timeinfo.tm_mon + 1,
                current_timeinfo.tm_hour, current_timeinfo.tm_min, current_timeinfo.tm_sec);
    } else {
        strcpy(buffer, "RTC: Time not set");
    }
}

void check_wake_up_reason(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            ESP_LOGI(TAG, "Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            ESP_LOGI(TAG, "Wakeup caused by ULP program");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup was not caused by deep sleep: %d", wakeup_reason);
            break;
    }
}

void enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Preparing for deep sleep...");
    
    // Display shutdown message
    const char *text_sleep = "Entering deep sleep...\nPress B1 to wake";
    i2c_task_send_display_text(text_sleep);
    
    // Give time for display to update
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Stop all running services
    if (sniffer_running) {
        ESP_ERROR_CHECK(sniffer_stop());
        ESP_ERROR_CHECK(pcap_close());
    }
    
    if (server_running) {
        stop_captive_server();
    }
    
    // Stop Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_LOGI(TAG, "Wi-Fi stopped for deep sleep.");
    
    // Unmount SD card
    if (sd_mounted) {
        esp_err_t err = unmount_sd();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "SD card unmounted for deep sleep.");
        } else {
            ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(err));
        }
    }
    
    // Configure GPIO for wake-up
    esp_err_t ret = rtc_gpio_init(CONFIG_GPIO_BUTTON_PIN_1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RTC GPIO: %s", esp_err_to_name(ret));
        return;
    }
    
    // Set the GPIO as input with pull-up
    rtc_gpio_set_direction(CONFIG_GPIO_BUTTON_PIN_1, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(CONFIG_GPIO_BUTTON_PIN_1);
    rtc_gpio_pulldown_dis(CONFIG_GPIO_BUTTON_PIN_1);
    
    // Configure wake-up source: wake when button is pressed
    esp_sleep_enable_ext0_wakeup(CONFIG_GPIO_BUTTON_PIN_1, 0);
    
    ESP_LOGI(TAG, "Entering deep sleep now...");
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

void set_default_time(void)
{
    struct timeval tv = { .tv_sec = 1735693200, .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

static void save_task(void* arg)
{
    const TickType_t xDelay = (1000 * 60 * CONFIG_SAVE_FREQUENCY_MINUTES) / portTICK_PERIOD_MS;

    while(stop_sniffer == false)
    {
        vTaskDelay(xDelay);
        change_file = true;
    }

    vTaskDelete(NULL);
}

static void initialize_LED(void)
{
    // LED PIN configuration
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_GPIO_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static void initialize_OLED_gpio(void)
{
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << OLED_POWER_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&io_conf);
gpio_set_level(OLED_POWER_PIN, 1);
}

static void initialize_BAT_gpio(void)
{
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << BAT_POWER_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&io_conf);
gpio_set_level(BAT_POWER_PIN, 1);
}

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI("SNTP", "Time synchronization event triggered! Waiting for system update...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

static void initialize_sntp(void)
{
    ESP_LOGI("SNTP", "Initializing SNTP...");
    
    ESP_LOGI("SNTP", "Setting operating mode...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    ESP_LOGI("SNTP", "Setting NTP server...");
    esp_sntp_setservername(0, "time.google.com");

    ESP_LOGI("SNTP", "Calling sntp_init()...");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    
    // Confirm SNTP status
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
        ESP_LOGW("SNTP", "SNTP status still RESET after initialization!");
    } else {
        ESP_LOGI("SNTP", "SNTP initialization seems successful.");
    }
}

static void initialize_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

static bool is_time_set()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec > 1700000000;
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (!use_wifi_time) {
        // Check if RTC time is valid before setting default time
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // If year is still 1970, RTC time was lost, so set default time
        if (timeinfo.tm_year < (2024 - 1900)) {
            ESP_LOGI("TIME_SYNC", "Skipping Wi-Fi. RTC time lost, using default time.");
            const char *text_default_time = "Skipping Wi-Fi...\nRTC time lost\nUsing default time.";
            i2c_task_send_display_text(text_default_time);
            set_default_time();
        } else {
            ESP_LOGI("TIME_SYNC", "Skipping Wi-Fi. Using preserved RTC time: %04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            const char *text_rtc_time = "Skipping Wi-Fi...\nUsing preserved\nRTC time.";
            i2c_task_send_display_text(text_rtc_time);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    const char *text_init_wifi = "Initializing...\nConnecting to Wi-Fi";
    i2c_task_send_display_text(text_init_wifi);

    if (wifi_connect() != ESP_OK)
    {
        ESP_LOGW("TIME_SYNC", "Wi-Fi connection failed. Checking RTC time...");
        
        // Check if RTC time is valid before falling back to default
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year < (2024 - 1900)) {
            ESP_LOGW("TIME_SYNC", "Using default time since Wi-Fi is unavailable and RTC time is lost.");
            set_default_time();
        } else {
            ESP_LOGI("TIME_SYNC", "Using preserved RTC time since Wi-Fi is unavailable: %04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        return;
    }

    initialize_sntp();

    const char *text_init_sntp = "Initializing...\nWaiting for SNTP";
    i2c_task_send_display_text(text_init_sntp);

    int wait_retries = 5;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && wait_retries-- > 0)
    {
        ESP_LOGI("TIME_SYNC", "Waiting for SNTP sync... (%d retries left)", wait_retries + 1);
        vTaskDelay(3000 / portTICK_PERIOD_MS);

        if (is_time_set())
        {
            ESP_LOGI("TIME_SYNC", "System time updated! Sync successful.");
            break;
        }
    }

    if (!is_time_set())
    {
        ESP_LOGW("TIME_SYNC", "SNTP sync failed. Checking RTC time...");
        
        // Check if RTC time is valid before falling back to default
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year < (2024 - 1900)) {
            ESP_LOGW("TIME_SYNC", "Using default time since SNTP sync failed and RTC time is lost.");
            set_default_time();
        } else {
            ESP_LOGI("TIME_SYNC", "Using preserved RTC time since SNTP sync failed: %04d-%02d-%02d %02d:%02d:%02d",
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
    }

    wifi_disconnect();
}

static bool mount_sd(void)
{
    esp_err_t ret;
    ESP_LOGI(TAG, "Initializing SD card in SPI mode");

    // Configuration to mount SD card
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024
    };

    // Initialize SD card and mount FAT filesystem
    sdmmc_card_t *card;

    // SPI bus configuration
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 13,
        .miso_io_num = 12,
        .sclk_io_num = 14,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    // Initialize the SPI bus
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus.");
        return false;
    }

    // Device configuration for the SD card
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = 15;
    slot_config.host_id = host.slot;

    // Define the mount point for the SD card
    const char mount_point[] = CONFIG_SD_MOUNT_POINT;
    
    // Mount the filesystem using SPI interface
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. Set format_if_mount_failed = true to format.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Ensure SD card has pull-up resistors.", esp_err_to_name(ret));
        }
        return false;
    }

    // Print card information
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD card mounted successfully.");

    return true;
}

static bool unmount_sd(void)
{
    if (esp_vfs_fat_sdmmc_unmount() != ESP_OK) {
        ESP_LOGE(TAG, "Card unmount failed");
        return sd_mounted;
    }

    ESP_LOGI(TAG, "Card unmounted");
    return false;
}

static uint32_t get_file_index(uint32_t max_files)
{
    uint32_t idx;
    char filename[CONFIG_FATFS_MAX_LFN];

    for(idx = 0; idx < max_files; idx++)
    {
        sprintf(filename, CONFIG_SD_MOUNT_POINT"/"CONFIG_PCAP_FILENAME_MASK, idx);
        if (access(filename, F_OK) != 0)
        {
            break;
        }
    }

    return idx;
}