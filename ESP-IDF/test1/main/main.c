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
#include "sniffer.h"

/* Defines -------------------------------------------------------------------*/
#define ESP_INTR_FLAG_DEFAULT 0

/* Global variables-----------------------------------------------------------*/
static const char *TAG = "main";

static volatile bool stop_probing = false;
static volatile bool change_file = false;
static bool sd_mounted = false;
static QueueHandle_t gpio_evt_queue = NULL;

/* Function prototypes -------------------------------------------------------*/
static void obtain_time(void);
static void initialize_gpio(void);
static void initialize_nvs(void);
static void initialize_sntp(void);
static void initialize_wifi(void);
static bool mount_sd(void);
static bool unmount_sd(void);

/* Interrupt service prototypes ----------------------------------------------*/
static void IRAM_ATTR gpio_isr_handler(void* arg);

/* Task prototypes -----------------------------------------------------------*/
static void gpio_task(void* arg);

/* Main function -------------------------------------------------------------*/
void app_main(void)
{
    time_t current_time;
    struct tm timeinfo;
    char strftime_buf[64];

    // Initialize peripherals and time
    initialize_gpio();
    initialize_nvs();
    obtain_time();

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

    // Open first pcap file
    initialize_wifi();
    initialize_sniffer();
    ESP_ERROR_CHECK(sniffer_start());

    // Turn off LED when set up ends
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_OFF));

    while(true)
    {
        if(stop_probing == true)
        {
            stop_probing = false;

            if (sd_mounted == true)
            {
                // Close current pcap and unmount SD
                ESP_ERROR_CHECK(sniffer_stop());
                sd_mounted = unmount_sd();
            }

            // Turn LED ON when SD unmounted
            ESP_ERROR_CHECK(gpio_set_level(CONFIG_GPIO_LED_PIN, CONFIG_GPIO_LED_ON));
            return;
        }

        vTaskDelay(10);
    }
}

/* Interrupt service routines ------------------------------------------------*/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/* Task definitions ----------------------------------------------------------*/
static void gpio_task(void* arg)
{
    uint32_t io_num;

    while(true)
    {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            stop_probing = true;
        }
    }
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

    // Button interrupt configuration
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << CONFIG_GPIO_BUTTON_PIN);
    io_conf.pull_up_en = 1;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    //hook isr handler for specific gpio pin
    ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_GPIO_BUTTON_PIN, gpio_isr_handler, (void*) CONFIG_GPIO_BUTTON_PIN));
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
    ESP_LOGI(TAG, "Initializing SD card");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024
    };

    // initialize SD card and mount FAT filesystem.
    sdmmc_card_t *card;

    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // To use 1-line SD mode, change this to 1:
    slot_config.width = 1;

    if (slot_config.width == 1)
    {
        host.flags = SDMMC_HOST_FLAG_1BIT;
        slot_config.width = 1;
    }
    else
    {
        ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY));  // D1, needed in 4-line mode only
        ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY)); // D2, needed in 4-line mode only
    }

    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY)); // CMD, needed in 4- and 1-line modes
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY));  // D0, needed in 4- and 1-line modes
    ESP_ERROR_CHECK(gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY)); // D3, needed in 4- and 1-line modes

    const char mount_point[] = CONFIG_SD_MOUNT_POINT;

    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                        "If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                        "Make sure SD card lines have pull-up resistors in place.",
                        esp_err_to_name(ret));
        }
        return false;
    }

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
