/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include <string.h>
#include "config.h"
#include "wifi_connect.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "display_queue.h"
 
#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)

static int s_active_interfaces = 0;
static SemaphoreHandle_t s_semph_get_ip_addrs;
static esp_netif_t *s_esp_netif = NULL;
const int max_retries = 5;
int retry_count = 0;

static const char *TAG = "wifi_connect";

static esp_netif_t *wifi_start(void);
static void wifi_stop(void);

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

/* set up connection, Wi-Fi and/or Ethernet */
static void start(void)
{
    s_esp_netif = wifi_start();
    s_active_interfaces++;

    /* create semaphore if at least one interface is active */
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0);
}

/* tear down connection, release resources */
static void stop(void)
{
    wifi_stop();
    s_active_interfaces--;
}

static esp_ip4_addr_t s_ip_addr;

static void on_got_ip(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
if (!is_our_netif(TAG, event->esp_netif)) {
ESP_LOGW(TAG, "Got IPv4 from another interface \"%s\": ignored", esp_netif_get_desc(event->esp_netif));
return;
}
ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));

// Signal that we've got an IP address
xSemaphoreGive(s_semph_get_ip_addrs);

// This is critically important - log the IP address we received
ESP_LOGI(TAG, "Connection successful! IP address: " IPSTR, IP2STR(&event->ip_info.ip));

// NOW it's safe to enable power save mode
// WIFI_PS_MIN_MODEM = good balance of power saving while maintaining connection
esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
if (err != ESP_OK) {
ESP_LOGW(TAG, "Failed to set power save mode: %s", esp_err_to_name(err));
} else {
ESP_LOGI(TAG, "Enabled Wi-Fi power save mode (MODEM)");
}
}

esp_err_t wifi_connect(void)
{
    if (s_semph_get_ip_addrs != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    start();
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&stop));
    ESP_LOGI(TAG, "Waiting for IP(s)");
    
    // Wait up to 10 seconds for IP acquisition
    const TickType_t ip_timeout = pdMS_TO_TICKS(10000);

    while (retry_count < max_retries)
    {
        // Wait for the semaphore that signals IP acquisition
        if (xSemaphoreTake(s_semph_get_ip_addrs, ip_timeout)) 
        {
            // Successfully got an IP address
            esp_netif_t *netif = NULL;
            esp_netif_ip_info_t ip;
            for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
                netif = esp_netif_next_unsafe(netif);
                if (is_our_netif(TAG, netif)) {
                    ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
                    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));
                    ESP_LOGI(TAG, "- IPv4 address: " IPSTR, IP2STR(&ip.ip));
                    
                    // We have an IP address - return success
                    return ESP_OK;
                }
            }
            return ESP_OK;
        }

        ESP_LOGW(TAG, "Wi-Fi connection attempt %d failed. Retrying...", retry_count + 1);
        
        // Print current WiFi status for debugging
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Currently connected to SSID: %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
        } else {
            ESP_LOGI(TAG, "Not connected to any AP");
        }
        
        // Disconnect properly
        esp_wifi_disconnect();
        
        // Wait before reconnecting
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Try to reconnect
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to initiate reconnection: %s", esp_err_to_name(err));
        }
        
        retry_count++;
    }

    ESP_LOGE(TAG, "Wi-Fi connection failed after %d attempts. Proceeding with default time.", max_retries);
    return ESP_FAIL;
}

esp_err_t wifi_disconnect(void)
{
    if (s_semph_get_ip_addrs == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    vSemaphoreDelete(s_semph_get_ip_addrs);
    s_semph_get_ip_addrs = NULL;
    stop();
    ESP_ERROR_CHECK(esp_unregister_shutdown_handler(&stop));
    return ESP_OK;
}

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
    return;
    }
    // Instead of ESP_ERROR_CHECK which aborts on any error,
    // log the error but don't abort the program
    if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to reconnect to Wi-Fi: %s", esp_err_to_name(err));
    // Optional: add a delay before trying to reconnect again
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_wifi_connect(); // Try one more time
    } else {
    ESP_LOGI(TAG, "Reconnect initiated successfully");
    }
}

static esp_netif_t *wifi_start(void)
{
    char *desc;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Prefix the interface description with the module TAG
    asprintf(&desc, "%s: %s", TAG, esp_netif_config.if_desc);
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    free(desc);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    wifi_config_t wifi_config = {
        .sta = {
            // .ssid = CONFIG_WIFI_SSID,
            // .password = CONFIG_WIFI_PASSWORD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
            .listen_interval = 3,  // Listen interval for beacon (3 * beacon interval = power save) 
        },
    };
    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));
    
    // Initially disable power save for stable connection
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Add a short delay before connecting
    vTaskDelay(pdMS_TO_TICKS(100));
    
    esp_wifi_connect();
    return netif;
}

static void wifi_stop(void)
{
    esp_netif_t *wifi_netif = get_netif_from_desc("sta");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);
    s_esp_netif = NULL;
}


esp_netif_t *get_netif(void)
{
    return s_esp_netif;
}

esp_netif_t *get_netif_from_desc(const char *desc)
{
    esp_netif_t *netif = NULL;
    char *expected_desc;
    asprintf(&expected_desc, "%s: %s", TAG, desc);

    while ((netif = esp_netif_next_unsafe(netif)) != NULL) {
        if (strcmp(esp_netif_get_desc(netif), expected_desc) == 0) {
            free(expected_desc);
            return netif;
        }
    }
    free(expected_desc);
    return netif;
}
