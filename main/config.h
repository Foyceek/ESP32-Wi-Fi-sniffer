#ifndef CONFIG
#define CONFIG

#include <stdint.h>
#include <time.h>  // Add this line

// Global variable for index
extern int request_index;
extern int max_request_rank;

#define CONFIG_GPIO_BUTTON_PIN_1 2
#define CONFIG_GPIO_BUTTON_PIN_2 19
#define CONFIG_GPIO_LED_PIN 21

#define TOP_REQUESTS_COUNT 10

typedef struct {
    int rssi;
    char mac_address[18];  // For MAC in XX:XX:XX:XX:XX:XX format
    time_t timestamp;
} top_request_t;

extern top_request_t top_requests[TOP_REQUESTS_COUNT];

extern int rssi_ranges[10];

#define CONFIG_GPIO_LED_ON 0
#define CONFIG_GPIO_LED_OFF 1

#define CONFIG_WIFI_SSID "esp32wifi"
#define CONFIG_WIFI_PASSWORD "esp32test"

// #define CONFIG_WIFI_SSID "TP-LINK_BDFF54"
// #define CONFIG_WIFI_PASSWORD "18079500"

#define CONFIG_SD_MOUNT_POINT "/sdcard"
#define CONFIG_SD_1_LINE true

#define CONFIG_PCAP_FILENAME_MASK "file_%06lu.pcap"
#define CONFIG_OUTPUT_FILE "REDUCED_DATA.csv"

#define CONFIG_SNIFFER_TASK_STACK_SIZE 4096
#define CONFIG_SNIFFER_TASK_PRIORITY 2
#define CONFIG_SNIFFER_WORK_QUEUE_LEN 128
#define CONFIG_SNIFFER_DEFAULT_CHANNEL 2

#define CONFIG_SNIFFER_USE_MAC_FILTER 0
#define CONFIG_SNIFFER_MAC_FILTER_B1 0x58
#define CONFIG_SNIFFER_MAC_FILTER_B2 0xBF
#define CONFIG_SNIFFER_MAC_FILTER_B3 0x25
#define CONFIG_SNIFFER_MAC_FILTER_B4 0x82
#define CONFIG_SNIFFER_MAC_FILTER_B5 0xF2
#define CONFIG_SNIFFER_MAC_FILTER_B6 0x74

#define CONFIG_ALL_CHANNEL_SCAN 0

#define CONFIG_SAVE_FREQUENCY_MINUTES 30

#endif