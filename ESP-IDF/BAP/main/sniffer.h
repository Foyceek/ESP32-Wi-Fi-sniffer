/* cmd_sniffer example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "i2c_oled.h"  // Include LVGL or any necessary headers for your OLED functions
#include "config.h"

/**
 * @brief Supported Sniffer Interface
 *
 */
typedef enum {
    SNIFFER_INTF_UNKNOWN = 0,
    SNIFFER_INTF_WLAN, /*!< WLAN interface */
    SNIFFER_INTF_ETH, /*!< Ethernet interface */
} sniffer_intf_t;

/**
 * @brief WLAN Sniffer Filter
 *
 */
typedef enum {
    SNIFFER_WLAN_FILTER_MGMT = 0, /*!< MGMT */
    SNIFFER_WLAN_FILTER_CTRL,     /*!< CTRL */
    SNIFFER_WLAN_FILTER_DATA,     /*!< DATA */
    SNIFFER_WLAN_FILTER_MISC,     /*!< MISC */
    SNIFFER_WLAN_FILTER_MPDU,     /*!< MPDU */
    SNIFFER_WLAN_FILTER_AMPDU,    /*!< AMPDU */
    SNIFFER_WLAN_FILTER_FCSFAIL,  /*!< When this bit is set, the hardware will receive packets for which frame check sequence failed */
    SNIFFER_WLAN_FILTER_MAX
} sniffer_wlan_filter_t;

// Function declaration
void display_top_requests_oled(void);

void initialize_sniffer(void);
void clear_top_requests(void);
esp_err_t sniffer_stop(void);
esp_err_t sniffer_start(void);
esp_err_t sniffer_set_heartbeat_interval(uint32_t interval_ms);

#ifdef __cplusplus
}
#endif
