/* cmd_sniffer example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdlib.h>
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <arpa/inet.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "esp_app_trace.h"
#include "sniffer.h"
#include "pcap_lib.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "config.h"
#include "esp_wifi_types.h"
#include "i2c_oled.h"
#include <time.h>
#include "display_queue.h"
#include "driver/gpio.h"
#include "battery.h"

#define SNIFFER_PAYLOAD_FCS_LEN             (4)
#define SNIFFER_PROCESS_PACKET_TIMEOUT_MS   (100)
#define SNIFFER_RX_FCS_ERR                  (0X41)
#define SNIFFER_DECIMAL_NUM                 (10)
#define UPDATE_INTERVAL_MS                  1000

#define HEARTBEAT_MAC_ADDR {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define HEARTBEAT_INTERVAL_MS 10000  //(300000) 5 minutes in milliseconds
#define BATTERY_INTERVAL_MS 10000  //(300000) 5 minutes in milliseconds

static uint8_t heartbeat_mac[6] = HEARTBEAT_MAC_ADDR;
static uint32_t heartbeat_interval_ms = HEARTBEAT_INTERVAL_MS;
static TickType_t last_heartbeat_time = 0;
static TickType_t last_battery_time = 0;
static int csv_log_interval_ms = BATTERY_INTERVAL_MS;

static const char *SNIFFER_TAG = "sniffer";
static char filename[CONFIG_FATFS_MAX_LFN];

#if CONFIG_SNIFFER_USE_MAC_FILTER
static const uint8_t filter_mac[6] = {CONFIG_SNIFFER_MAC_FILTER_B1, 
                                      CONFIG_SNIFFER_MAC_FILTER_B2, 
                                      CONFIG_SNIFFER_MAC_FILTER_B3, 
                                      CONFIG_SNIFFER_MAC_FILTER_B4, 
                                      CONFIG_SNIFFER_MAC_FILTER_B5, 
                                      CONFIG_SNIFFER_MAC_FILTER_B6};
#endif

int request_index = 1;
int max_request_rank = TOP_REQUESTS_COUNT/2;
int rssi_ranges[10] = {0};
int clear_time_threshold = 30;
bool display_battery_data = true;

typedef struct {
    bool is_running;
    sniffer_intf_t interf;
    uint32_t interf_num;
    uint32_t channel;
    TaskHandle_t task;
    QueueHandle_t work_queue;
    SemaphoreHandle_t sem_task_over;
} sniffer_runtime_t;

static sniffer_runtime_t snf_rt = {0};

typedef struct {
    void *payload;
    uint32_t length;
    uint32_t seconds;
    uint32_t microseconds;
} sniffer_packet_info_t;

typedef struct {
	int16_t frame_ctrl;
	int16_t duration;
	uint8_t addr1[6];
	uint8_t addr2[6];
	uint8_t addr3[6];
	int16_t sequence_number;
	unsigned char payload[];
} packet_control_header_t;

// Initialize top_requests array with very low RSSI values dynamically based on TOP_REQUESTS_COUNT
top_request_t top_requests[TOP_REQUESTS_COUNT] = {0};  // Initialize the array with zeroes.

void init_top_requests() {
    
    for (int i = 0; i < TOP_REQUESTS_COUNT; i++) {
        top_requests[i].rssi = -101;  // Initialize with a very low RSSI value
        memset(top_requests[i].mac_address, 0, sizeof(top_requests[i].mac_address));
        top_requests[i].timestamp = 0;
    }
}

static wifi_promiscuous_pkt_t* create_heartbeat_packet(void)
{
    // Allocate memory for a synthetic packet
    wifi_promiscuous_pkt_t* pkt = malloc(sizeof(wifi_promiscuous_pkt_t) + sizeof(packet_control_header_t));
    if (pkt == NULL) {
        ESP_LOGE(SNIFFER_TAG, "Failed to allocate memory for heartbeat packet");
        return NULL;
    }
    
    // Fill the rx_ctrl structure
    pkt->rx_ctrl.sig_len = sizeof(packet_control_header_t);
    pkt->rx_ctrl.rssi = -1;  // Special RSSI value for heartbeat packets
    
    // Create a fake probe request header
    packet_control_header_t* hdr = (packet_control_header_t*)pkt->payload;
    
    // Set frame control to probe request (0x4000)
    hdr->frame_ctrl = htons(0x4000);
    
    // Set MAC address to the heartbeat MAC
    memcpy(hdr->addr2, heartbeat_mac, 6);
    
    return pkt;
}

static esp_err_t write_heartbeat_packet(void)
{
    esp_err_t ret = ESP_OK;
    struct timeval tv;
    time_t current_time;
    struct tm* timeinfo;
    char str_buf[64];
    
    FILE *file;

    file = fopen(filename, "a");
    if (file == NULL) {
        ESP_LOGE(SNIFFER_TAG, "Failed to open file for writing heartbeat packet");
        return ESP_FAIL;
    }

    gettimeofday(&tv, NULL);
    // Remove the +3600 to fix the time offset issue
    current_time = tv.tv_sec;
    timeinfo = localtime(&current_time);
    strftime(str_buf, sizeof(str_buf), "%c", timeinfo);

    // Write heartbeat packet: Time, MAC (00:00:00:00:00:00), RSSI (using -1 as a marker for heartbeat)
    fprintf(file, "%s, %02X:%02X:%02X:%02X:%02X:%02X, %d [HEARTBEAT]\n", 
            str_buf, 
            heartbeat_mac[0], heartbeat_mac[1], heartbeat_mac[2],
            heartbeat_mac[3], heartbeat_mac[4], heartbeat_mac[5],
            -1);  // Using -1 as a special RSSI value to mark heartbeat packets
    
    fclose(file);
    
    // Create a synthetic packet for PCAP capture
    wifi_promiscuous_pkt_t* pkt = create_heartbeat_packet();
    if (pkt != NULL) {
        // Create packet info for PCAP capture
        sniffer_packet_info_t packet_info;
        packet_info.seconds = tv.tv_sec;
        packet_info.microseconds = tv.tv_usec;
        packet_info.length = sizeof(wifi_promiscuous_pkt_t) + sizeof(packet_control_header_t);
        packet_info.payload = pkt;
        
        // Save to PCAP file
        if (packet_capture(packet_info.payload, packet_info.length, packet_info.seconds, packet_info.microseconds) != ESP_OK) {
            ESP_LOGW(SNIFFER_TAG, "Save heartbeat packet in pcap format failed");
        }
        
        free(pkt);
    }
    
    ESP_LOGI(SNIFFER_TAG, "Heartbeat packet written at %s", str_buf);
    
    return ret;
}

esp_err_t sniffer_set_heartbeat_interval(uint32_t interval_ms)
{
    if (interval_ms == 0) {
        ESP_LOGW(SNIFFER_TAG, "Heartbeat disabled");
    } else {
        ESP_LOGI(SNIFFER_TAG, "Setting heartbeat interval to %lu ms", interval_ms);
    }
    
    heartbeat_interval_ms = interval_ms;
    return ESP_OK;
}

// Comparator function for sorting by RSSI in descending order
int compare_rssi(const void *a, const void *b) {
    top_request_t *request_a = (top_request_t *)a;
    top_request_t *request_b = (top_request_t *)b;
    return request_b->rssi - request_a->rssi;
    }

// Updated function to handle RSSI ranges correctly
void update_rssi_ranges(int rssi) {
    if (rssi >= -10 && rssi <= -1) {
        rssi_ranges[0]++;  // Strongest signal
    } else if (rssi >= -20 && rssi <= -11) {
        rssi_ranges[1]++;
    } else if (rssi >= -30 && rssi <= -21) {
        rssi_ranges[2]++;
    } else if (rssi >= -40 && rssi <= -31) {
        rssi_ranges[3]++;
    } else if (rssi >= -50 && rssi <= -41) {
        rssi_ranges[4]++;
    } else if (rssi >= -60 && rssi <= -51) {
        rssi_ranges[5]++;
    } else if (rssi >= -70 && rssi <= -61) {
        rssi_ranges[6]++;
    } else if (rssi >= -80 && rssi <= -71) {
        rssi_ranges[7]++;
    } else if (rssi >= -90 && rssi <= -81) {
        rssi_ranges[8]++;
    } else if (rssi >= -100 && rssi <= -91) {
        rssi_ranges[9]++;  // Weakest signal
    } else {
        ESP_LOGW(SNIFFER_TAG, "Received RSSI out of expected range: %d", rssi);
    }
    
    // Debugging line to check the RSSI value and range count updates
    ESP_LOGI(SNIFFER_TAG, "Updated RSSI range for %d, Range counts: %d %d %d %d %d %d %d %d %d %d", 
             rssi, rssi_ranges[0], rssi_ranges[1], rssi_ranges[2], rssi_ranges[3], 
             rssi_ranges[4], rssi_ranges[5], rssi_ranges[6], rssi_ranges[7], 
             rssi_ranges[8], rssi_ranges[9]);
}

// Function to display RSSI ranges dynamically
void display_rssi_ranges() {
    printf("\n--- RSSI Range Counts ---\n");
    for (int i = 0; i < 10; i++) {
        // Print ranges from -0 to -9, -10 to -19, etc.
        printf("Range -%d to -%d dBm: %d packets\n", i * 10, (i + 1) * 10 - 1, rssi_ranges[i]);
    }
}

void clear_top_requests() {
    time_t current_time = time(NULL);  // Get the current time in seconds

    for (int i = 0; i < TOP_REQUESTS_COUNT; i++) {
        if (top_requests[i].timestamp > 0 && (current_time - top_requests[i].timestamp) > clear_time_threshold) {
            top_requests[i].rssi = -101;  // Reset RSSI to mark as empty
            memset(top_requests[i].mac_address, 0, sizeof(top_requests[i].mac_address));  // Clear MAC address
            top_requests[i].timestamp = 0;  // Reset timestamp
            // Sort the top_requests array by RSSI in descending order
            qsort(top_requests, TOP_REQUESTS_COUNT, sizeof(top_request_t), compare_rssi);
        }
    }

    #if SHOW_SNIFFER_DEBUG
    ESP_LOGW(SNIFFER_TAG, "Old top requests cleared");
    #endif
}

// Function to update top requests dynamically, with duplicate MAC check
static void update_top_requests(int rssi, const uint8_t *mac_address, time_t timestamp) {
    int min_rssi_idx = 0;
    int duplicate_idx = -1;
    char mac_address_str[18];  // Temporary string to store the formatted MAC address

    // Convert `mac_address` (uint8_t array) to a string in XX:XX:XX:XX:XX:XX format
    snprintf(mac_address_str, sizeof(mac_address_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_address[0], mac_address[1], mac_address[2],
             mac_address[3], mac_address[4], mac_address[5]);

    // Check if the MAC address is already in the top_requests list
    for (int i = 0; i < TOP_REQUESTS_COUNT; i++) {
        if (strcmp(top_requests[i].mac_address, mac_address_str) == 0) {
            duplicate_idx = i;
            break;
        }
        if (top_requests[i].rssi < top_requests[min_rssi_idx].rssi) {
            min_rssi_idx = i;
        }
    }

    // If the MAC address is already in the list and the new RSSI is stronger, update it
    if (duplicate_idx != -1) {
        if (rssi > top_requests[duplicate_idx].rssi) {
            top_requests[duplicate_idx].rssi = rssi;
            top_requests[duplicate_idx].timestamp = timestamp;

            // Sort the top_requests array by RSSI in descending order
            qsort(top_requests, TOP_REQUESTS_COUNT, sizeof(top_request_t), compare_rssi);

            // Debug print to confirm update
            ESP_LOGI("UPDATE_TOP_REQUESTS", "Updated existing entry %d: MAC %s, RSSI %d",
                     duplicate_idx, mac_address_str, rssi);
        }
        // If the RSSI is not stronger, do nothing
        return;
    }

    // If it's not a duplicate and the RSSI is stronger than the lowest, replace the lowest entry
    if (rssi > top_requests[min_rssi_idx].rssi) {
        top_requests[min_rssi_idx].rssi = rssi;
        strncpy(top_requests[min_rssi_idx].mac_address, mac_address_str, sizeof(top_requests[min_rssi_idx].mac_address) - 1);
        top_requests[min_rssi_idx].mac_address[sizeof(top_requests[min_rssi_idx].mac_address) - 1] = '\0';
        top_requests[min_rssi_idx].timestamp = timestamp;

        // Sort the top_requests array by RSSI in descending order
        qsort(top_requests, TOP_REQUESTS_COUNT, sizeof(top_request_t), compare_rssi);

        // Debug print to confirm update
        ESP_LOGI("UPDATE_TOP_REQUESTS", "Added new entry %d: MAC %s, RSSI %d",
                 min_rssi_idx, mac_address_str, rssi);
    }
}

void display_top_requests_oled() {
    if (powering_down){
        return;
    } 

    // Check if request_index is within valid bounds
    if (request_index < 1 || request_index > max_request_rank) {
        printf("Invalid request index: %d, setting to default\n", request_index);
        request_index = 1;
        return;  // Exit if the index is invalid
    }

    char disp_delay[8];
    if (delay_state == 0) {
        snprintf(disp_delay, sizeof(disp_delay), "1h");
    } else if (delay_state == 1) {
        snprintf(disp_delay, sizeof(disp_delay), "30s");
    } else {
        snprintf(disp_delay, sizeof(disp_delay), "10s");
    }

    // Determine the index of the request to display
    int index_to_display = request_index - 1;  // Convert to zero-based index

    // Ensure that the index is within the valid range of top_requests
    if (index_to_display >= 0 && index_to_display < max_request_rank) {
        if (top_requests[index_to_display].rssi != -101) {  // Check if it's a valid entry
            struct tm *timeinfo = localtime(&top_requests[index_to_display].timestamp);
            char time_buf[64];
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", timeinfo);

            char display_text[256];  // Adjust size as needed
            
            if (display_battery_data) {
                // Include battery data in the display
                snprintf(display_text, sizeof(display_text),
                         "%u mV  %d mA  [%s]\n(%d/%d) Top RSSI\nRSSI: %d\nTime: %s\n%s", 
                         volts, current, disp_delay, request_index, max_request_rank, 
                         top_requests[index_to_display].rssi, time_buf, 
                         top_requests[index_to_display].mac_address);
            } else {
                // Omit battery data as requested
                snprintf(display_text, sizeof(display_text),
                         "(%d/%d) Top RSSI [%s]\nRSSI: %d\nTime: %s\n%s",
                         request_index, max_request_rank, disp_delay, 
                         top_requests[index_to_display].rssi, time_buf, 
                         top_requests[index_to_display].mac_address);
            }
                                
            // Display the selected top request on the OLED
            i2c_task_send_display_text(display_text);

        } else {
            // Display "Entry empty" if the entry is cleared
            char display_text[128];
            
            if (display_battery_data) {
                snprintf(display_text, sizeof(display_text), 
                         "%u mV  %d mA  [%s]\n(%d/%d) Top RSSI\nEntry empty", 
                         volts, current, disp_delay, request_index, max_request_rank);
            } else {
                snprintf(display_text, sizeof(display_text), 
                         "(%d/%d) Top RSSI [%s]\nEntry empty", 
                         request_index, max_request_rank, disp_delay);
            }
            
            i2c_task_send_display_text(display_text);
        }
    } else {
        printf("Index out of range for top requests: %d\n", index_to_display);
    }
}

// Function to print all top requests to the serial monitor dynamically
static void display_top_requests() {
    printf("\n--- Top %d Requests by RSSI ---\n", TOP_REQUESTS_COUNT);
    for (int i = 0; i < TOP_REQUESTS_COUNT; i++) {
        if (top_requests[i].rssi != -999) {  // Check if it's a valid entry
            struct tm *timeinfo = localtime(&top_requests[i].timestamp);
            char time_buf[64];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
            printf("Time: %s, MAC: %s, RSSI: %d\n", time_buf, top_requests[i].mac_address, top_requests[i].rssi);
        }
    }
}

static void queue_packet(void *recv_packet, sniffer_packet_info_t *packet_info)
{
    /* Copy a packet from Link Layer driver and queue the copy to be processed by sniffer task */
    void *packet_to_queue = malloc(packet_info->length);
    if (packet_to_queue)
    {
        memcpy(packet_to_queue, recv_packet, packet_info->length);
        packet_info->payload = packet_to_queue;
        if (snf_rt.work_queue)
        {
            /* send packet_info */
            if (xQueueSend(snf_rt.work_queue, packet_info, pdMS_TO_TICKS(SNIFFER_PROCESS_PACKET_TIMEOUT_MS)) != pdTRUE)
            {
                ESP_LOGE(SNIFFER_TAG, "sniffer work queue full");
                free(packet_info->payload);
            }
        }
    }
    else
    {
        ESP_LOGE(SNIFFER_TAG, "No enough memory for promiscuous packet");
    }
}

// Function to process packets from sniffer callback and update RSSI ranges
static void wifi_sniffer_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type)
{
    struct timeval tv;
    sniffer_packet_info_t packet_info;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)recv_buf;
    packet_control_header_t *hdr = (packet_control_header_t *)pkt->payload;

    int32_t fc = ntohs(hdr->frame_ctrl);

    // Check only for Probe requests
    #if CONFIG_SNIFFER_USE_MAC_FILTER
        if ((fc & 0xFF00) == 0x4000 && memcmp(hdr->addr2, filter_mac, 6) == 0)
    #else
        if ((fc & 0xFF00) == 0x4000)
    #endif
    {
        gettimeofday(&tv, NULL);

        packet_info.seconds = tv.tv_sec;
        packet_info.microseconds = tv.tv_usec;
        packet_info.length = pkt->rx_ctrl.sig_len + sizeof(wifi_pkt_rx_ctrl_t);
        packet_info.length -= SNIFFER_PAYLOAD_FCS_LEN;

        int rssi = pkt->rx_ctrl.rssi;
        
        ESP_LOGI("SNIFFER_CB", "Captured request with RSSI %d from MAC %02X:%02X:%02X:%02X:%02X:%02X",
                 rssi, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2],
                 hdr->addr2[3], hdr->addr2[4], hdr->addr2[5]);
        
        queue_packet(pkt, &packet_info);  // Queue the packet for processing
    }
}

static esp_err_t sniffer_write_reduced_data(void *payload, uint32_t length)
{
    esp_err_t ret = ESP_OK;
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)payload;
    packet_control_header_t *hdr = (packet_control_header_t *)pkt->payload;
    struct timeval tv;
    time_t current_time;
    struct tm* timeinfo;
    char str_buf[64];
    
    FILE *file;

    file = fopen(filename, "a");
    if (file == NULL)
    {
        ESP_LOGE(SNIFFER_TAG, "Failed to open file for writing");
        return -1;
    }

    gettimeofday(&tv, NULL);
    // Remove the +3600 to fix the time offset issue
    current_time = tv.tv_sec;
    timeinfo = localtime(&current_time);
    strftime(str_buf, sizeof(str_buf), "%c", timeinfo);

    // Time, MAC, RSSI
    fprintf(file, "%s, %02X:%02X:%02X:%02X:%02X:%02X, %d\n", str_buf, hdr->addr2[0], hdr->addr2[1], hdr->addr2[2], hdr->addr2[3], hdr->addr2[4], hdr->addr2[5], pkt->rx_ctrl.rssi);
    fclose(file);

    return ret;
}

esp_err_t sniffer_write_battery_data(void)
{
    i2c_task_send_battery_status();
    esp_err_t ret = ESP_OK;
    struct timeval tv;
    time_t current_time;
    struct tm* timeinfo;
    char time_str[64];

    FILE *file = fopen(CONFIG_SD_MOUNT_POINT "/" CONFIG_BATTERY_FILE, "a");
    if (file == NULL)
    {
        ESP_LOGE(SNIFFER_TAG, "Failed to open battery data file for writing");
        return ESP_FAIL;
    }

    gettimeofday(&tv, NULL);
    current_time = tv.tv_sec;
    timeinfo = localtime(&current_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Write timestamp, voltage (mV), and current (mA)
    fprintf(file, "%s, %u, %d\n", time_str, volts, current);
    fclose(file);
    ESP_LOGI(SNIFFER_TAG, "Saved battery data [%u mV  %d mA]", volts, current);

    return ret;
}
static void sniffer_task(void *parameters)
{
    sniffer_packet_info_t packet_info;
    sniffer_runtime_t *sniffer = (sniffer_runtime_t *)parameters;
    TickType_t last_update_time = xTaskGetTickCount();
    last_heartbeat_time = xTaskGetTickCount();  // Initialize heartbeat timer

    init_top_requests();

    snprintf(filename, sizeof(filename), CONFIG_SD_MOUNT_POINT "/" CONFIG_OUTPUT_FILE);

    // Write initial heartbeat packet when starting the sniffer
    write_heartbeat_packet();

    while (sniffer->is_running)
    {
        // Receive packet info from queue
        if (xQueueReceive(sniffer->work_queue, &packet_info, pdMS_TO_TICKS(SNIFFER_PROCESS_PACKET_TIMEOUT_MS)) == pdTRUE)
        {
            wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)packet_info.payload;
            packet_control_header_t *hdr = (packet_control_header_t *)pkt->payload;

            ESP_LOGI("SNIFFER_TASK", "Processing packet with RSSI %d", pkt->rx_ctrl.rssi);

            // Save captured data in reduced format
            if (sniffer_write_reduced_data(packet_info.payload, packet_info.length) != ESP_OK)
            {
                ESP_LOGW(SNIFFER_TAG, "Save captured packet in reduced format failed");
            }

            // Save captured data in pcap format
            if (packet_capture(packet_info.payload, packet_info.length, packet_info.seconds, packet_info.microseconds) != ESP_OK)
            {
                ESP_LOGW(SNIFFER_TAG, "Save captured packet in pcap format failed");
            }

            // Update top requests
            update_top_requests(pkt->rx_ctrl.rssi, hdr->addr2, packet_info.seconds);
            // Update RSSI range counters
            update_rssi_ranges(pkt->rx_ctrl.rssi);

            // Free the payload memory
            if (packet_info.payload != NULL)
            {
                free(packet_info.payload);
                packet_info.payload = NULL;  // Set it to NULL after freeing
            }
        }

        // Check if heartbeat interval has elapsed
        if (xTaskGetTickCount() - last_heartbeat_time >= pdMS_TO_TICKS(heartbeat_interval_ms))
        {
            write_heartbeat_packet();
            last_heartbeat_time = xTaskGetTickCount();  // Reset heartbeat timer
        }

        // Check if heartbeat interval has elapsed
        if (xTaskGetTickCount() - last_battery_time >= pdMS_TO_TICKS(BATTERY_INTERVAL_MS))
        {
            sniffer_write_battery_data();
            last_battery_time = xTaskGetTickCount();  // Reset heartbeat timer
        }
        

        // Check if UPDATE_INTERVAL_MS have elapsed for display updates
        if (xTaskGetTickCount() - last_update_time >= pdMS_TO_TICKS(UPDATE_INTERVAL_MS))
        {
            #if SHOW_SNIFFER_DEBUG
            display_top_requests();  // Print all top requests to the serial monitor
            display_rssi_ranges();
            #endif

            #if PRINT_BATTERY_STATUS
            i2c_task_send_battery_status();
            #endif

            #if USE_OLED
            if(oled_initialized){
                i2c_task_send_battery_status();
                display_top_requests_oled();  // Display top requests on OLED
            }
            clear_top_requests();
            #endif

            last_update_time = xTaskGetTickCount();
        }
     
    }

    // Notify that sniffer task is over
    xSemaphoreGive(sniffer->sem_task_over);
    vTaskDelete(NULL);
    vTaskDelay(pdMS_TO_TICKS(100));
}

esp_err_t sniffer_stop(void)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(snf_rt.is_running, ESP_ERR_INVALID_STATE, err, SNIFFER_TAG, "sniffer is already stopped");

    /* Disable wifi promiscuous mode */
    ESP_GOTO_ON_ERROR(esp_wifi_set_promiscuous(false), err, SNIFFER_TAG, "stop wifi promiscuous failed");

    ESP_LOGI(SNIFFER_TAG, "stop promiscuous ok");

    /* stop sniffer local task */
    snf_rt.is_running = false;
    /* wait for task over */
    xSemaphoreTake(snf_rt.sem_task_over, portMAX_DELAY);

    vSemaphoreDelete(snf_rt.sem_task_over);
    snf_rt.sem_task_over = NULL;
    /* make sure to free all resources in the left items */
    UBaseType_t left_items = uxQueueMessagesWaiting(snf_rt.work_queue);

    sniffer_packet_info_t packet_info;
    while (left_items--)
    {
        xQueueReceive(snf_rt.work_queue, &packet_info, pdMS_TO_TICKS(SNIFFER_PROCESS_PACKET_TIMEOUT_MS));
        free(packet_info.payload);
    }
    vQueueDelete(snf_rt.work_queue);
    snf_rt.work_queue = NULL;

    /* stop pcap session */
    sniff_packet_stop();
err:
    return ret;
}

esp_err_t sniffer_start(void)
{
    esp_err_t ret = ESP_OK;
    pcap_link_type_t link_type = PCAP_LINK_TYPE_802_11_RADIOTAP;
    wifi_promiscuous_filter_t wifi_filter = {
        .filter_mask = WIFI_EVENT_MASK_AP_PROBEREQRECVED
	};
    ESP_GOTO_ON_FALSE(!(snf_rt.is_running), ESP_ERR_INVALID_STATE, err, SNIFFER_TAG, "sniffer is already running");

    /* init a pcap session */
    ESP_GOTO_ON_ERROR(sniff_packet_start(link_type), err, SNIFFER_TAG, "init pcap session failed");

    snf_rt.is_running = true;
    snf_rt.work_queue = xQueueCreate(CONFIG_SNIFFER_WORK_QUEUE_LEN, sizeof(sniffer_packet_info_t));
    ESP_GOTO_ON_FALSE(snf_rt.work_queue, ESP_FAIL, err_queue, SNIFFER_TAG, "create work queue failed");
    snf_rt.sem_task_over = xSemaphoreCreateBinary();
    ESP_GOTO_ON_FALSE(snf_rt.sem_task_over, ESP_FAIL, err_sem, SNIFFER_TAG, "create work queue failed");
    ESP_GOTO_ON_FALSE(xTaskCreate(sniffer_task, "snifferT", CONFIG_SNIFFER_TASK_STACK_SIZE,
                                  &snf_rt, CONFIG_SNIFFER_TASK_PRIORITY, &snf_rt.task), ESP_FAIL,
                      err_task, SNIFFER_TAG, "create task failed");

    /* Start WiFi Promiscuous Mode */
    esp_wifi_set_promiscuous_filter(&wifi_filter);
    esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
    ESP_GOTO_ON_ERROR(esp_wifi_set_promiscuous(true), err_start, SNIFFER_TAG, "create work queue failed");
    esp_wifi_set_channel(snf_rt.channel, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(SNIFFER_TAG, "start WiFi promiscuous ok");

    return ret;
err_start:
    vTaskDelete(snf_rt.task);
    snf_rt.task = NULL;
err_task:
    vSemaphoreDelete(snf_rt.sem_task_over);
    snf_rt.sem_task_over = NULL;
err_sem:
    vQueueDelete(snf_rt.work_queue);
    snf_rt.work_queue = NULL;
err_queue:
    snf_rt.is_running = false;
err:
    return ret;
}

void initialize_sniffer(void)
{
    snf_rt.interf = SNIFFER_INTF_WLAN;
    snf_rt.channel = CONFIG_SNIFFER_DEFAULT_CHANNEL;
}
