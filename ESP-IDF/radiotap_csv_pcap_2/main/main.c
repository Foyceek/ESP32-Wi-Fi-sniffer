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

/* Defines -------------------------------------------------------------------*/
#define ESP_INTR_FLAG_DEFAULT 0

/* Global variables-----------------------------------------------------------*/
static const char *TAG = "main";

volatile bool stop_probing = false;
static volatile bool change_file = false;
static bool sd_mounted = false;
static QueueHandle_t gpio_evt_queue = NULL;

// Add the current request index here
static int current_request_index = 0;

/* Function prototypes -------------------------------------------------------*/
static void obtain_time(void);
static void initialize_gpio(void);
static void initialize_nvs(void);
static void initialize_sntp(void);
static void initialize_wifi(void);
static bool mount_sd(void);
static bool unmount_sd(void);
static uint32_t get_file_index(uint32_t max_files);

/* Task prototypes -----------------------------------------------------------*/
static void save_task(void* arg);

/* Main function -------------------------------------------------------------*/
void app_main(void)
{
    // Initialize the first OLED display
    oled_init();
    lv_disp_t *disp = get_display_handle();
    const char *text_init = "Initializing...";
    oled_display_text(disp, text_init, false);

    time_t current_time;
    struct tm timeinfo;
    char strftime_buf[64];
    uint32_t file_idx = 0;

    // Initialize peripherals and time
    #if CONFIG_STATUS_LED
    initialize_gpio();
    #endif
    initialize_nvs();
    obtain_time();

    // Define GPIO pins for both buttons
    gpio_num_t button_gpio_pin_s1 = CONFIG_GPIO_BUTTON_PIN_1;
    gpio_num_t button_gpio_pin_s2 = CONFIG_GPIO_BUTTON_PIN_2; 

    // Initialize button manager with both GPIO pins
    esp_err_t ret = button_manager_init(button_gpio_pin_s1, button_gpio_pin_s2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Button manager initialization failed on pins %d and %d", button_gpio_pin_s1, button_gpio_pin_s2);
        return;
    }

    // update 'current_time' variable with current time
    time(&current_time);

    // Set timezone
    setenv("TZ", "UTC-1", 1);
    tzset();
    localtime_r(&current_time, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Europe is: %s", strftime_buf);

    // SD card setup and file ID check
    sd_mounted = mount_sd();
    
    if (sd_mounted == false)
    {
        return;
    }
    file_idx = get_file_index(65535);

    // Open first pcap file
    ESP_ERROR_CHECK(pcap_open(file_idx));
    initialize_wifi();
    initialize_sniffer();
    ESP_ERROR_CHECK(sniffer_start());
    const char *text_sniff = "Sniffing...";
    oled_display_text(disp, text_sniff, false);
    
    //start periodical save task
    xTaskCreate(save_task, "save_task", 1024, NULL, 10, NULL);
    #if CONFIG_STATUS_LED
    // Turn off LED when set up ends
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_OFF));
    #endif

    while (true) {
        if (stop_probing) {
            // Stop the sniffer
            ESP_ERROR_CHECK(sniffer_stop());
            ESP_ERROR_CHECK(pcap_close());

            char text_stop[100];
            sprintf(text_stop, "Sniffer stopped\nWebserver started\n%s", ESP_AP_IP);
            oled_display_text(disp, text_stop, false);

            // Start the webserver
            esp_err_t ret = start_captive_server();
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start webserver: %s", esp_err_to_name(ret));
            } else {
                ESP_LOGI(TAG, "Webserver started successfully.");
            }

            // Wait for the user to toggle stop_probing back
            while (stop_probing) {
                vTaskDelay(100 / portTICK_PERIOD_MS); // Wait for state change
            }

            // Stop the webserver
            stop_captive_server();

            ESP_LOGI(TAG, "Webserver stopped. Resuming sniffer...");
            const char *text_sniff = "Sniffing...";
            oled_display_text(disp, text_sniff, false);

            // Resume the sniffer
            ESP_ERROR_CHECK(pcap_open(get_file_index(65535))); // Get next file index
            ESP_ERROR_CHECK(sniffer_start());
        }

        if (change_file) {
            ESP_ERROR_CHECK(sniffer_stop());
            ESP_ERROR_CHECK(pcap_close());
            ESP_ERROR_CHECK(pcap_open(++file_idx));
            ESP_ERROR_CHECK(sniffer_start());
            change_file = false;
        }

        #if CONFIG_ALL_CHANNEL_SCAN
        for (int chan = 1; chan <= 10; chan++) {
            vTaskDelay(5);
            ESP_ERROR_CHECK(esp_wifi_set_channel(chan, WIFI_SECOND_CHAN_NONE));
        }
        #endif

        vTaskDelay(5);
    }

}

static void save_task(void* arg)
{
    const TickType_t xDelay = (1000 * 60 * CONFIG_SAVE_FREQUENCY_MINUTES) / portTICK_PERIOD_MS;

    while(stop_probing == false)
    {
        vTaskDelay(xDelay);
        change_file = true;
    }

    vTaskDelete(NULL);
}

/* Function definitions ------------------------------------------------------*/
static void initialize_gpio(void)
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

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void initialize_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void initialize_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(wifi_connect());

    initialize_sntp();

    // wait for time to be set
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(wifi_disconnect() );
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
