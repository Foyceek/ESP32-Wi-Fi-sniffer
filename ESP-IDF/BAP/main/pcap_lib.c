/* cmd_pcap example.
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
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_console.h"
#include "esp_app_trace.h"
#include "esp_check.h"
#include "sniffer.h"
#include "pcap.h"
#include "sdkconfig.h"
#include "config.h"
#include "pcap_lib.h"

static const char *PCAP_TAG = "pcap";

#define PCAP_MEMORY_BUFFER_SIZE CONFIG_SNIFFER_PCAP_MEMORY_SIZE
#define SNIFFER_PROCESS_APPTRACE_TIMEOUT_US (100)
#define SNIFFER_APPTRACE_RETRY              (10)
#define TRACE_TIMER_FLUSH_INT_MS            (1000)

static pcap_cmd_runtime_t pcap_rt = {0};

esp_err_t pcap_close(void)
{
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(pcap_rt.is_opened, ESP_ERR_INVALID_STATE, err, PCAP_TAG, ".pcap file is already closed");
    ESP_GOTO_ON_ERROR(pcap_del_session(pcap_rt.pcap_handle) != ESP_OK, err, PCAP_TAG, "stop pcap session failed");
    pcap_rt.is_opened = false;
    pcap_rt.link_type_set = false;
    pcap_rt.pcap_handle = NULL;
err:
    return ret;
}

esp_err_t pcap_open(uint32_t idx)
{
    esp_err_t ret = ESP_OK;

    /* Create file to write, binary format */
    snprintf(pcap_rt.filename, sizeof(pcap_rt.filename), CONFIG_SD_MOUNT_POINT"/"CONFIG_PCAP_FILENAME_MASK, idx);
    FILE *fp = NULL;
    fp = fopen(pcap_rt.filename, "wb+");
    ESP_GOTO_ON_FALSE(fp, ESP_FAIL, err, PCAP_TAG, "open file failed");
    pcap_config_t pcap_config = {
        .fp = fp,
        .major_version = PCAP_DEFAULT_VERSION_MAJOR,
        .minor_version = PCAP_DEFAULT_VERSION_MINOR,
        .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
    };
    ESP_GOTO_ON_ERROR(pcap_new_session(&pcap_config, &pcap_rt.pcap_handle), err, PCAP_TAG, "pcap init failed");
    pcap_rt.is_opened = true;
    ESP_LOGI(PCAP_TAG, "open file successfully");
    return ret;
err:
    if (fp) 
    {
        fclose(fp);
    }
    return ret;
}

esp_err_t packet_capture(void *payload, uint32_t length, uint32_t seconds, uint32_t microseconds)
{
    return pcap_capture_packet(pcap_rt.pcap_handle, payload, length, seconds, microseconds);
}

esp_err_t sniff_packet_start(pcap_link_type_t link_type)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_FALSE(pcap_rt.is_opened, ESP_ERR_INVALID_STATE, err, PCAP_TAG, "no .pcap file stream is open");
    if (pcap_rt.link_type_set) 
    {
        ESP_GOTO_ON_FALSE(link_type == pcap_rt.link_type, ESP_ERR_INVALID_STATE, err, PCAP_TAG, "link type error");
        ESP_GOTO_ON_FALSE(!pcap_rt.is_writing, ESP_ERR_INVALID_STATE, err, PCAP_TAG, "still sniffing");
    }
    else 
    {
        pcap_rt.link_type = link_type;
        /* Create file to write, binary format */
        pcap_write_header(pcap_rt.pcap_handle, link_type);
        pcap_rt.link_type_set = true;
    }
    pcap_rt.is_writing = true;
err:
    return ret;
}

esp_err_t sniff_packet_stop(void)
{
    pcap_rt.is_writing = false;
    return ESP_OK;
}