#include "display_queue.h"
#include "i2c_oled.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "config.h"
#include "variables.h"
#include "driver/gpio.h"
#include <string.h>
#include "battery.h"
#include "driver/i2c.h"
#include "bq27441.h"

static const char* TAG = "i2c_task";

typedef enum {
    I2C_TASK_DISPLAY_CLEAR,
    I2C_TASK_DISPLAY_TEXT,
    I2C_TASK_BATTERY_STATUS
} i2c_task_msg_type_t;

typedef struct {
    i2c_task_msg_type_t type;
    char text[256]; // used for DISPLAY_TEXT
} i2c_task_message_t;

static QueueHandle_t i2c_task_queue = NULL;
static TaskHandle_t i2c_task_handle = NULL;

static void i2c_master_init(void) {
    static bool initialized = false;
    if (initialized) return;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
    initialized = true;
    ESP_LOGI(TAG, "I2C master initialized");
}

// Task function
static void i2c_task(void *pvParameters) {
    i2c_task_message_t msg;

    while (1) {
        if (xQueueReceive(i2c_task_queue, &msg, portMAX_DELAY)) {
            switch (msg.type) {
                case I2C_TASK_DISPLAY_CLEAR:
                    oled_clear_screen();
                    break;

                case I2C_TASK_DISPLAY_TEXT:
                    oled_display_text(msg.text);
                    break;

                case I2C_TASK_BATTERY_STATUS:
                    #if !PRINT_BATTERY_STATUS
                    gpio_set_level(BAT_POWER_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    #endif
                 
                    static bool battery_initialized = false;

                    if (!battery_initialized) {
                        if (!bq27441Begin(I2C_NUM_1)) {
                            ESP_LOGE(TAG, "BQ27441 not detected!");
                            break;
                        }
                        ESP_LOGI(TAG, "BQ27441 detected!");

                        if (!configure_battery_params()) {
                            ESP_LOGE(TAG, "Battery configuration failed");
                            break;
                        }

                        battery_initialized = true;
                    }
                    print_battery_status();
                    #if !PRINT_BATTERY_STATUS
                    gpio_set_level(BAT_POWER_PIN, 0);
                    #endif
                    break;

                default:
                    ESP_LOGW(TAG, "Unknown I2C task message type: %d", msg.type);
                    break;
            }
        }
    }
}

void i2c_task_init(void) {
    i2c_master_init();
    i2c_task_queue = xQueueCreate(10, sizeof(i2c_task_message_t));
    if (i2c_task_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C task queue");
        return;
    }

    BaseType_t result = xTaskCreate(
        i2c_task,
        "i2c_task",
        4096,
        NULL,
        5,
        &i2c_task_handle
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create I2C task");
    } else {
        ESP_LOGI(TAG, "I2C task system initialized");
    }
}

void i2c_task_send_display_text(const char* text) {
    #if !USE_OLED
    return;
    #endif
    if (i2c_task_queue == NULL) return;

    i2c_task_message_t msg;
    msg.type = I2C_TASK_DISPLAY_TEXT;
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.text[sizeof(msg.text) - 1] = '\0';

    xQueueSend(i2c_task_queue, &msg, pdMS_TO_TICKS(100));
}

void i2c_task_send_display_clear(void) {
    if (i2c_task_queue == NULL) return;

    i2c_task_message_t msg;
    msg.type = I2C_TASK_DISPLAY_CLEAR;

    xQueueSend(i2c_task_queue, &msg, pdMS_TO_TICKS(100));
}

void i2c_task_send_battery_status(void) {

    if (i2c_task_queue == NULL) return;

    i2c_task_message_t msg;
    msg.type = I2C_TASK_BATTERY_STATUS;

    xQueueSend(i2c_task_queue, &msg, pdMS_TO_TICKS(100));

}

void oled_countdown_task(void *pvParameter) {
    cancel_powerdown = false;

    if (!oled_initialized) {
        printf("OLED not initialized, skipping countdown.\n");
        vTaskDelete(NULL);
        return;
    }

    for (int i = 10; i > 0; i--) {
        if (cancel_powerdown) {
            printf("Power-off canceled by user.\n");
            i2c_task_send_display_text("Power-off canceled");
            powering_down = false;
            vTaskDelete(NULL);
            return;
        }

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "OLED powering off \nin %d...", i);
        i2c_task_send_display_text(buffer);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    i2c_task_send_display_clear();
    oled_initialized = false;
    gpio_set_level(OLED_POWER_PIN, 0);
    powering_down = false;

    printf("OLED disconnected to save power.\n");
    vTaskDelete(NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
}

