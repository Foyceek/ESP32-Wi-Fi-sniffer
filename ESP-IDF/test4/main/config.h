#ifndef CONFIG
#define CONFIG

#include <stdint.h>

// Global variable for index
extern int request_index;

#define CONFIG_GPIO_BUTTON_PIN 2
#define CONFIG_GPIO_LED_PIN 21

#define CONFIG_GPIO_LED_ON 0
#define CONFIG_GPIO_LED_OFF 1

#define CONFIG_WIFI_SSID "esp32wifi"
#define CONFIG_WIFI_PASSWORD "esp32test"

// #define CONFIG_WIFI_SSID "TP-LINK_BDFF54"
// #define CONFIG_WIFI_PASSWORD "18079500"

#define CONFIG_SD_MOUNT_POINT "/sdcard"
#define CONFIG_SD_1_LINE true

#define CONFIG_OUTPUT_FILE "REDUCED_DATA.csv"

#define CONFIG_SNIFFER_TASK_STACK_SIZE 4096
#define CONFIG_SNIFFER_TASK_PRIORITY 2
#define CONFIG_SNIFFER_WORK_QUEUE_LEN 128

#endif