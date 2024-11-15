#include "server.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "mdns.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "sniffer.h"
#include "config.h"

#define TAG "Captive_Portal"
#define WIFI_SSID "ESP32_Captive_AP"
#define HOSTNAME "esp32.local"
#define ESP_AP_IP "192.168.4.1"
#define NUM_BARS 10
#define MAX_BAR_HEIGHT 200

static const char *html_page_template =
    "<html><head><title>Welcome</title></head>"
    "<body><h1>Welcome to ESP32 Captive Portal</h1>"
    "<h2>Top Requests</h2>"
    "<table border='1'><tr><th>Rank</th><th>Time</th><th>MAC Address</th><th>RSSI</th></tr>%s</table>"
    "<h2>RSSI Bar Graph</h2><pre>%s</pre>"
    "</body></html>";

static char* generate_top_requests_table(void) {
    static char table_rows[2048];
    int len = 0;
    table_rows[0] = '\0';

    for (int i = 0; i < TOP_REQUESTS_COUNT && i <= 20; i++) {
        if (top_requests[i].mac_address[0] != '\0' && top_requests[i].rssi != 0) {
            char timestamp_str[32];
            struct tm timeinfo;
            localtime_r(&top_requests[i].timestamp, &timeinfo);
            strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

            len += snprintf(table_rows + len, sizeof(table_rows) - len,
                             "<tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td></tr>", 
                             i + 1, timestamp_str, top_requests[i].mac_address, top_requests[i].rssi);
        }
    }

    return len == 0 ? NULL : table_rows;
}

static char* generate_svg_bar_graph(void) {
    static char svg_buffer[2048];
    int len = 0;

    // Define the RSSI ranges
    const int rssi_range_starts[NUM_BARS] = {-0, -10, -20, -30, -40, -50, -60, -70, -80, -90};
    const int rssi_range_ends[NUM_BARS] = {-9, -19, -29, -39, -49, -59, -69, -79, -89, -99};
    
    // Start of the SVG (Width is now based on number of bars and max bar height)
    len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                    "<svg width=\"%d\" height=\"500\" xmlns=\"http://www.w3.org/2000/svg\">\n", NUM_BARS * 50);

    // Define bar height and gap
    int bar_height = 40;
    int gap = 10;

    // Loop through the rssi_ranges array to draw bars and add labels
    for (int i = 0; i < NUM_BARS; i++) {
        int bar_width = (rssi_ranges[i] > MAX_BAR_HEIGHT) ? MAX_BAR_HEIGHT : rssi_ranges[i];  // Make sure width doesn't exceed max width
        int y_position = i * (bar_height + gap);  // Y position for the bar
        int x_position = 0;  // X position for the left of the bar

        // Draw the bar (rotate by changing coordinates)
        len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                         "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"steelblue\" />\n", 
                         x_position, y_position, bar_width, bar_height);

        // Add the label for the RSSI range (e.g., "Range 0-9 dBm: 1 packets")
        // Adjust the x position of the text to move it to the right of the bar
        len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                         "<text x=\"%d\" y=\"%d\" font-family=\"Arial\" font-size=\"12\" fill=\"black\">Range %d to %d dBm: %d packets</text>\n",
                         bar_width + 5, y_position + bar_height / 2, rssi_range_starts[i], rssi_range_ends[i], rssi_ranges[i]);
    }

    // End of the SVG
    snprintf(svg_buffer + len, sizeof(svg_buffer) - len, "</svg>\n");

    return svg_buffer;
}

esp_err_t root_get_handler(httpd_req_t *req) {
    char buffer[4096];
    int len = 0;

    // Generate the top requests table
    const char* table_rows = generate_top_requests_table();

    // Generate the SVG bar graph
    const char* svg_bar_graph = generate_svg_bar_graph();

    // Create the final HTML page by embedding the table and the SVG
    len += snprintf(buffer + len, sizeof(buffer) - len, html_page_template, table_rows ? table_rows : "", svg_bar_graph);

    // Send the response
    httpd_resp_send(req, buffer, len);
    return ESP_OK;
}

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;  // Increased stack size for larger buffer allocations
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_register_uri_handler(server, &root_uri);
    }

    return server;
}

esp_err_t start_captive_server(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = 1,
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_handle_t server = start_webserver();
    if (server) {
        ESP_LOGI(TAG, "Captive portal server started.");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start captive portal server.");
        return ESP_FAIL;
    }
}
