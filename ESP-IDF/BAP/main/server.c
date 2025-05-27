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
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "miniz.h" 
#include "i2c_oled.h"
#include "cJSON.h"

#define TAG "Captive_Portal"
#define HOSTNAME "esp32.local"
#define NUM_BARS 10
#define MAX_BAR_HEIGHT 200
#define ZIP_FILE_PATH CONFIG_SD_MOUNT_POINT "/sd_files.zip"
#define MAX_FILES_TO_ADD 1000

const char *get_content_type(const char *filename);

static httpd_handle_t server_handle = NULL;
static esp_netif_t *ap_netif = NULL;

esp_err_t stop_server_handler(httpd_req_t *req);
void stop_server_task(void *pvParameters);
esp_err_t save_settings_to_sd(const char *path);

size_t get_file_size(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file); 
    fclose(file);
    return file_size;
}

static const char *html_page_template =
    "<html><head><title>Welcome</title>"
    "<style>"
    "  .button-row { margin-bottom: 10px; }"
    "  button {"
    "    padding: 15px 30px;"
    "    font-size: 18px;"
    "  }"
    "</style></head>"
    "<body><h1>Welcome to ESP32 Captive Portal</h1>"
    "<div class=\"button-row\"><button onclick=\"location.href='/browse_sd'\">Browse SD Card</button></div>"
    "<div class=\"button-row\"><button onclick=\"location.href='/settings'\">Settings</button></div>"
    "<div class=\"button-row\"><button onclick=\"location.href='/stop_server'\">Stop Server</button></div>"
    "<h2>Top Requests</h2>"
    "<table border='1'><tr><th>Rank</th><th>Time</th><th>MAC Address</th><th>RSSI</th></tr>%s</table>"
    "<h2>RSSI Bar Graph</h2><pre>%s</pre>"
    "</body></html>";

static char* generate_top_requests_table(void) {
    // Initial allocation for table rows
    size_t buffer_size = 4096;
    char *table_rows = malloc(buffer_size);
    if (!table_rows) {
        ESP_LOGE(TAG, "Failed to allocate memory for table rows");
        return NULL;
    }
    
    table_rows[0] = '\0';
    int len = 0;

    for (int i = 0; i < TOP_REQUESTS_COUNT && i <= 20; i++) {
        if (top_requests[i].mac_address[0] != '\0' && top_requests[i].rssi != 0) {
            char timestamp_str[32];
            struct tm timeinfo;
            localtime_r(&top_requests[i].timestamp, &timeinfo);
            strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

            // Check if we need more space
            size_t needed_size = len + 256;  // Reasonable estimate per row
            if (needed_size >= buffer_size) {
                buffer_size *= 2;
                char *new_buffer = realloc(table_rows, buffer_size);
                if (!new_buffer) {
                    ESP_LOGE(TAG, "Failed to reallocate memory for table rows");
                    free(table_rows);
                    return NULL;
                }
                table_rows = new_buffer;
            }

            len += snprintf(table_rows + len, buffer_size - len,
                            "<tr><td>%d</td><td>%s</td><td>%s</td><td>%d</td></tr>", 
                            i + 1, timestamp_str, top_requests[i].mac_address, top_requests[i].rssi);
        }
    }

    return table_rows;
}

// Updated function to use dynamic allocation for SVG graph
static char* generate_svg_bar_graph(void) {
    // Initial allocation for SVG
    size_t buffer_size = 4096;
    char *svg_buffer = malloc(buffer_size);
    if (!svg_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for SVG buffer");
        return NULL;
    }
    
    int len = 0;

    const int rssi_range_starts[NUM_BARS] = {-0, -10, -20, -30, -40, -50, -60, -70, -80, -90};
    const int rssi_range_ends[NUM_BARS] = {-9, -19, -29, -39, -49, -59, -69, -79, -89, -99};
    
    len += snprintf(svg_buffer + len, buffer_size - len,
                    "<svg width=\"%d\" height=\"500\" xmlns=\"http://www.w3.org/2000/svg\">\n", NUM_BARS * 50);

    for (int i = 0; i < NUM_BARS; i++) {
        // Check if we need to expand the buffer
        size_t needed_size = len + 512;  // Estimate for each bar and text
        if (needed_size >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = realloc(svg_buffer, buffer_size);
            if (!new_buffer) {
                ESP_LOGE(TAG, "Failed to reallocate memory for SVG buffer");
                free(svg_buffer);
                return NULL;
            }
            svg_buffer = new_buffer;
        }

        int bar_width = (rssi_ranges[i] > MAX_BAR_HEIGHT) ? MAX_BAR_HEIGHT : rssi_ranges[i];
        int y_position = i * 50;

        len += snprintf(svg_buffer + len, buffer_size - len,
                        "<rect x=\"0\" y=\"%d\" width=\"%d\" height=\"40\" fill=\"steelblue\" />\n", 
                        y_position, bar_width);

        len += snprintf(svg_buffer + len, buffer_size - len,
                        "<text x=\"%d\" y=\"%d\" font-family=\"Arial\" font-size=\"12\" fill=\"black\">Range %d to %d dBm: %d packets</text>\n",
                        bar_width + 5, y_position + 20, rssi_range_starts[i], rssi_range_ends[i], rssi_ranges[i]);
    }

    // Add closing SVG tag
    size_t needed_size = len + 10;  // Just need a bit more for the closing tag
    if (needed_size >= buffer_size) {
        buffer_size += 10;
        char *new_buffer = realloc(svg_buffer, buffer_size);
        if (!new_buffer) {
            ESP_LOGE(TAG, "Failed to reallocate memory for SVG buffer");
            free(svg_buffer);
            return NULL;
        }
        svg_buffer = new_buffer;
    }
    
    len += snprintf(svg_buffer + len, buffer_size - len, "</svg>\n");
    
    return svg_buffer;
}

esp_err_t create_zip_archive(const char *directory_path, const char *zip_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    int file_count = 0;  // Moved here to unify type

    // First, count .pcap files
    DIR *count_dir = opendir(directory_path);
    if (!count_dir) {
        ESP_LOGE(TAG, "Failed to open directory for counting");
        return ESP_FAIL;
    }

    struct dirent *count_entry;
    while ((count_entry = readdir(count_dir)) != NULL) {
        if (count_entry->d_type == DT_REG && strstr(count_entry->d_name, ".pcap")) {
            file_count++;
        }
    }
    closedir(count_dir);

    if (file_count < 2) {
        ESP_LOGW(TAG, "Not enough .pcap files to create ZIP archive");
        return ESP_OK;
    }

    // Proceed with ZIP archive creation
    int part_number = 1;
    char zip_filename[256];
    snprintf(zip_filename, sizeof(zip_filename), "%s_part%d.zip", zip_path, part_number);

    if (!mz_zip_writer_init_file(&zip_archive, zip_filename, 0)) {
        ESP_LOGE(TAG, "Failed to initialize ZIP writer: %s", mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
        return ESP_FAIL;
    }

    DIR *dir = opendir(directory_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", directory_path);
        mz_zip_writer_end(&zip_archive);
        return ESP_FAIL;
    }

    struct dirent *entry;
    char full_file_path[256];
    file_count = 0;  // Reset for actual archive content tracking

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".pcap")) {
            int written = snprintf(full_file_path, sizeof(full_file_path), "%s/%s", directory_path, entry->d_name);
            if (written < 0 || written >= sizeof(full_file_path)) {
                ESP_LOGW(TAG, "Path too long, skipping file: %s/%s", directory_path, entry->d_name);
                continue;
            }

            ESP_LOGI(TAG, "Adding file: %s", full_file_path);

            if (!mz_zip_writer_add_file(&zip_archive, entry->d_name, full_file_path, NULL, 0, MZ_NO_COMPRESSION)) {
                ESP_LOGW(TAG, "Failed to add file to ZIP: %s", entry->d_name);

                if (!mz_zip_writer_finalize_archive(&zip_archive)) {
                    ESP_LOGE(TAG, "Failed to finalize ZIP archive: %s", mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
                }
                mz_zip_writer_end(&zip_archive);

                part_number++;
                snprintf(zip_filename, sizeof(zip_filename), "%s_part%d.zip", zip_path, part_number);
                ESP_LOGI(TAG, "Starting new ZIP archive: %s", zip_filename);

                if (!mz_zip_writer_init_file(&zip_archive, zip_filename, 0)) {
                    ESP_LOGE(TAG, "Failed to initialize new ZIP writer");
                    closedir(dir);
                    return ESP_FAIL;
                }

                if (!mz_zip_writer_add_file(&zip_archive, entry->d_name, full_file_path, NULL, 0, MZ_NO_COMPRESSION)) {
                    ESP_LOGE(TAG, "Failed to add file to new ZIP: %s", entry->d_name);
                    continue;
                }
            }

            file_count++;
        }
    }

    closedir(dir);

    if (file_count > 0 && !mz_zip_writer_finalize_archive(&zip_archive)) {
        ESP_LOGE(TAG, "Failed to finalize last ZIP archive");
        mz_zip_writer_end(&zip_archive);
        return ESP_FAIL;
    }

    mz_zip_writer_end(&zip_archive);
    ESP_LOGI(TAG, "ZIP archive(s) created successfully at: %s", zip_path);
    return ESP_OK;
}

esp_err_t favicon_handler(httpd_req_t *req) {
    httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t stop_captive_server(void) {
    esp_err_t ret = ESP_OK;

    // Stop the web server
    if (server_handle) {
        ret = httpd_stop(server_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Webserver stopped successfully.");
        } else {
            ESP_LOGE(TAG, "Failed to stop webserver: %s", esp_err_to_name(ret));
        }
        server_handle = NULL;
    } else {
        ESP_LOGW(TAG, "Webserver handle was NULL. Nothing to stop.");
    }

    // Stop the Wi-Fi AP
    ret = esp_wifi_stop();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi AP stopped successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi AP: %s", esp_err_to_name(ret));
    }

    // Destroy the default AP network interface
    if (ap_netif) {
        esp_netif_destroy_default_wifi(ap_netif);
        ap_netif = NULL;
        ESP_LOGI(TAG, "Default Wi-Fi AP interface destroyed successfully.");
    } else {
        ESP_LOGW(TAG, "No default Wi-Fi AP interface found to destroy.");
    }

    return ret;
}

esp_err_t root_get_handler(httpd_req_t *req) {
    // Generate the top requests table
    char *table_rows = generate_top_requests_table();
    if (!table_rows) {
        ESP_LOGE(TAG, "Failed to generate top requests table");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Generate the SVG bar graph
    char *svg_bar_graph = generate_svg_bar_graph();
    if (!svg_bar_graph) {
        ESP_LOGE(TAG, "Failed to generate SVG bar graph");
        free(table_rows);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Calculate required buffer size (with some extra space for safety)
    size_t table_len = strlen(table_rows);
    size_t svg_len = strlen(svg_bar_graph);
    size_t template_len = strlen(html_page_template);
    size_t total_required = template_len + table_len + svg_len + 100; // Extra padding
    
    // Allocate memory for the full response
    char *buffer = malloc(total_required);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer");
        free(table_rows);
        free(svg_bar_graph);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    // Create the final HTML page by embedding the table and SVG
    int len = snprintf(buffer, total_required, html_page_template, table_rows, svg_bar_graph);
    
    // Send the response
    esp_err_t ret = httpd_resp_send(req, buffer, len);
    
    // Free all allocated memory
    free(buffer);
    free(table_rows);
    free(svg_bar_graph);
    
    return ret;
}

esp_err_t settings_get_handler(httpd_req_t *req) {
    char html[4096];
    snprintf(html, sizeof(html),
        "<html><head><title>ESP32 Settings</title>"
        "<script>"
        "function syncTime(){"
        "fetch('/sync_time?timestamp='+Math.floor(Date.now()/1000))"
        ".then(r=>r.text()).then(()=>showMsg('result','Time synchronized!'))"
        ".catch(e=>showMsg('result','Error: '+e));"
        "}"
        "function updateTime(){"
        "document.getElementById('device-time').textContent=new Date().toLocaleString();"
        "}"
        "setInterval(updateTime,1000);updateTime();"
        "function setPeriod(id,url){"
        "let v=document.getElementById(id+'_input').value;"
        "if(isNaN(v)||v<1000)return showMsg(id+'-result','Enter >= 1000');"
        "fetch(url+v).then(r=>r.text()).then(()=>showMsg(id+'-result','Updated!'))"
        ".catch(e=>showMsg(id+'-result','Error: '+e));"
        "}"
        "function setWifi(){"
        "let s=encodeURIComponent(document.getElementById('ssid_input').value);"
        "let p=encodeURIComponent(document.getElementById('pass_input').value);"
        "fetch('/set_wifi?ssid='+s+'&password='+p)"
        ".then(r=>r.text()).then(()=>showMsg('wifi-result','WiFi updated!'))"
        ".catch(e=>showMsg('wifi-result','Error: '+e));"
        "}"
        "function setServerWifi(){"
        "let s=encodeURIComponent(document.getElementById('server_ssid_input').value);"
        "let p=encodeURIComponent(document.getElementById('server_pass_input').value);"
        "fetch('/set_server_wifi?server_ssid='+s+'&server_password='+p)"
        ".then(r=>r.text()).then(()=>showMsg('server-wifi-result','WiFi updated!'))"
        ".catch(e=>showMsg('server-wifi-result','Error: '+e));"
        "}"
        "function showMsg(id,msg){"
        "let e=document.getElementById(id);e.textContent=msg;"
        "setTimeout(()=>{e.textContent='';},3000);"
        "}"

        "function flipOLED() {"
        "fetch('/oled_flip')"
        ".then(response => response.text())"
        ".then(data => {"
        "const resultDiv = document.getElementById('flip-result');"
        "resultDiv.innerHTML = 'Rotated!';"
        "setTimeout(() => { resultDiv.innerHTML = ''; }, 3000);"
        "})"
        ".catch(error => {"
        "document.getElementById('flip-result').innerHTML = 'Error: ' + error;"
        "});"
        "}"

        "function batteryStatus() {"
        "fetch('/battery_status')"
        ".then(response => response.text())"
        ".then(data => {"
        "const resultDiv = document.getElementById('battery-result');"
        "resultDiv.innerHTML = 'Toggled!';"
        "setTimeout(() => { resultDiv.innerHTML = ''; }, 3000);"
        "})"
        ".catch(error => {"
        "document.getElementById('battery-result').innerHTML = 'Error: ' + error;"
        "});"
        "}"

        "</script></head><body>"
        "<h1>ESP32 Settings</h1>"

        "<h2>Time</h2>"
        "<p>Current: <span id='device-time'></span></p>"
        "<button onclick='syncTime()'>Sync Time</button>"
        "<div id='result' style='color:green;'></div>"

        "<h2>OLED orientation</h2>"
        "<button onclick='flipOLED()'>Flip OLED</button>"
        "<div id='flip-result' style='color:green;'></div>"

        "<h2>Display battery status</h2>"
        "<button onclick='batteryStatus()'>Toggle Battery Status</button>"
        "<div id='battery-result' style='color:green;'></div>"

        "<h2>WiFi for time sync</h2>"
        "<p>SSID: <input id='ssid_input' value='%s'></p>"
        "<p>Password: <input id='pass_input' type='password' value='%s'></p>"
        "<button onclick='setWifi()'>Set WiFi</button>"
        "<div id='wifi-result' style='color:green;'></div>"

        "<h2>Webserver WiFi</h2>"
        "<p>SSID: <input id='server_ssid_input' value='%s'></p>"
        "<p>Password: <input id='server_pass_input' type='password' value='%s'></p>"
        "<button onclick='setServerWifi()'>Set WiFi</button>"
        "<div id='server-wifi-result' style='color:green;'></div>"

        "<h2>OLED Periods</h2>"

        "<p>Short (%d): <input id='short_input' type='number'>"
        "<button onclick=\"setPeriod('short','/set_short_period?value=')\">Set</button>"
        "<div id='short-result' style='color:green;'></div></p>"

        "<p>Medium (%d): <input id='medium_input' type='number'>"
        "<button onclick=\"setPeriod('medium','/set_medium_period?value=')\">Set</button>"
        "<div id='medium-result' style='color:green;'></div></p>"

        "<p>Long (%d): <input id='long_input' type='number'>"
        "<button onclick=\"setPeriod('long','/set_long_period?value=')\">Set</button>"
        "<div id='long-result' style='color:green;'></div></p>"

        "<br><button onclick=\"location.href='/'\">Back</button>"
        "</body></html>",
        wifi_ssid, wifi_password, server_wifi_ssid, server_wifi_password, short_oled_period, medium_oled_period, long_oled_period);

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t battery_status_handler(httpd_req_t *req) {
    display_battery_data = !display_battery_data;
    
    // Save the updated battery display status
    save_settings_to_sd(CONFIG_SD_MOUNT_POINT "/settings.json");
    
    httpd_resp_send(req, "Battery Status Toggled", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t oled_flip_handler(httpd_req_t *req) {
    // Toggle the current rotation
    flip_oled = !flip_oled;
    
    // Apply the new rotation
    oled_flip(flip_oled);
    
    // Save the new rotation state to settings
    save_settings_to_sd(CONFIG_SD_MOUNT_POINT "/settings.json");
    
    httpd_resp_send(req, "OLED rotation updated", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_wifi_handler(httpd_req_t *req) {
    char query[128], ssid[32], password[64];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "ssid", ssid, sizeof(ssid)) == ESP_OK &&
        httpd_query_key_value(query, "password", password, sizeof(password)) == ESP_OK) {

        strncpy(wifi_ssid, ssid, sizeof(wifi_ssid));
        strncpy(wifi_password, password, sizeof(wifi_password));
        ESP_LOGI(TAG, "WiFi updated: SSID=%s, PASS=%s", wifi_ssid, wifi_password);

        save_settings_to_sd(CONFIG_SD_MOUNT_POINT "/settings.json");

        httpd_resp_send(req, "WiFi settings updated", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid or password");
    return ESP_FAIL;
}

esp_err_t set_server_wifi_handler(httpd_req_t *req) {
    char query[128], server_ssid[32], server_password[64];

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "server_ssid", server_ssid, sizeof(server_ssid)) == ESP_OK &&
        httpd_query_key_value(query, "server_password", server_password, sizeof(server_password)) == ESP_OK) {

        strncpy(server_wifi_ssid, server_ssid, sizeof(server_wifi_ssid));
        strncpy(server_wifi_password, server_password, sizeof(server_wifi_password));
        ESP_LOGI(TAG, "Captive Server WiFi updated: SSID=%s, PASS=%s", server_wifi_ssid, server_wifi_password);

        save_settings_to_sd(CONFIG_SD_MOUNT_POINT "/settings.json");

        httpd_resp_send(req, "Server WiFi settings updated", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing server_ssid or server_password");
    return ESP_FAIL;
}

esp_err_t set_period_handler(httpd_req_t *req) {
    char query[64];
    char value_str[16];
    const char *uri = req->uri;

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "value", value_str, sizeof(value_str)) == ESP_OK) {

        int value = atoi(value_str);
        const char *response_msg = NULL;

        if (strstr(uri, "short")) {
            short_oled_period = value;
            response_msg = "Short period updated";
            ESP_LOGI(TAG, "Short period updated to: %d", value);
        } else if (strstr(uri, "medium")) {
            medium_oled_period = value;
            response_msg = "Medium period updated";
            ESP_LOGI(TAG, "Medium period updated to: %d", value);
        } else if (strstr(uri, "long")) {
            long_oled_period = value;
            response_msg = "Long period updated";
            ESP_LOGI(TAG, "Long period updated to: %d", value);
        } else {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown period type");
            return ESP_FAIL;
        }

        save_settings_to_sd(CONFIG_SD_MOUNT_POINT "/settings.json");
        httpd_resp_send(req, response_msg, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid or missing 'value' parameter");
    return ESP_FAIL;
}

esp_err_t save_settings_to_sd(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open settings file for writing");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    // Add server WiFi credentials
    cJSON_AddStringToObject(root, "server_wifi_ssid", server_wifi_ssid);
    cJSON_AddStringToObject(root, "server_wifi_password", server_wifi_password);
    cJSON_AddStringToObject(root, "wifi_ssid", wifi_ssid);
    cJSON_AddStringToObject(root, "wifi_password", wifi_password);
    cJSON_AddNumberToObject(root, "short_oled_period", short_oled_period);
    cJSON_AddNumberToObject(root, "medium_oled_period", medium_oled_period);
    cJSON_AddNumberToObject(root, "long_oled_period", long_oled_period);
    
    // Add display settings
    cJSON_AddBoolToObject(root, "display_battery_data", display_battery_data);
    cJSON_AddBoolToObject(root, "current_rotation", flip_oled);

    char *json_str = cJSON_Print(root);
    fwrite(json_str, 1, strlen(json_str), f);

    fclose(f);
    cJSON_Delete(root);
    free(json_str);

    ESP_LOGI(TAG, "Settings saved to %s", path);
    return ESP_OK;
}

esp_err_t load_settings_from_sd(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "Settings file not found");
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buffer);
    if (root) {
        cJSON *ssid = cJSON_GetObjectItem(root, "wifi_ssid");
        if (cJSON_IsString(ssid)) {
            strncpy(wifi_ssid, ssid->valuestring, sizeof(wifi_ssid));
        }

        cJSON *pass = cJSON_GetObjectItem(root, "wifi_password");
        if (cJSON_IsString(pass)) {
            strncpy(wifi_password, pass->valuestring, sizeof(wifi_password));
        }
        
        // Load server WiFi credentials
        cJSON *server_ssid = cJSON_GetObjectItem(root, "server_wifi_ssid");
        if (cJSON_IsString(server_ssid)) {
            strncpy(server_wifi_ssid, server_ssid->valuestring, sizeof(server_wifi_ssid));
            ESP_LOGI(TAG, "Loaded server_wifi_ssid = %s", server_wifi_ssid);
        }
        
        cJSON *server_pass = cJSON_GetObjectItem(root, "server_wifi_password");
        if (cJSON_IsString(server_pass)) {
            strncpy(server_wifi_password, server_pass->valuestring, sizeof(server_wifi_password));
            ESP_LOGI(TAG, "Loaded server_wifi_password = %s", server_wifi_password);
        }
        
        cJSON *sp = cJSON_GetObjectItem(root, "short_oled_period");
        if (cJSON_IsNumber(sp)) {
            short_oled_period = sp->valueint;
            ESP_LOGI(TAG, "Loaded short_oled_period = %d", short_oled_period);
        }
        
        cJSON *mp = cJSON_GetObjectItem(root, "medium_oled_period");
        if (cJSON_IsNumber(mp)) {
            medium_oled_period = mp->valueint;
            ESP_LOGI(TAG, "Loaded medium_oled_period = %d", medium_oled_period);
        }
        
        cJSON *lp = cJSON_GetObjectItem(root, "long_oled_period");
        if (cJSON_IsNumber(lp)) {
            long_oled_period = lp->valueint;
            ESP_LOGI(TAG, "Loaded long_oled_period = %d", long_oled_period);
        }
        
        // Load display settings
        cJSON *batt_display = cJSON_GetObjectItem(root, "display_battery_data");
        if (cJSON_IsBool(batt_display)) {
            display_battery_data = cJSON_IsTrue(batt_display);
            ESP_LOGI(TAG, "Loaded display_battery_data = %s", display_battery_data ? "true" : "false");
        }
        
        cJSON *rotation = cJSON_GetObjectItem(root, "current_rotation");
        if (cJSON_IsBool(rotation)) {
            flip_oled = rotation->valueint;
            // Apply the loaded rotation immediately if OLED is initialized
            if (oled_initialized) {
                oled_flip(flip_oled);
            }
            ESP_LOGI(TAG, "Loaded and applied flip_oled = %d", flip_oled);

        }
        
        cJSON_Delete(root);
    }

    free(buffer);
    return ESP_OK;
}

esp_err_t sync_time_handler(httpd_req_t *req) {
    char query[32];
    char timestamp_str[16] = {0};
    time_t timestamp = 0;

    // Get query parameter
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (httpd_query_key_value(query, "timestamp", timestamp_str, sizeof(timestamp_str)) == ESP_OK) {
            timestamp = (time_t)atol(timestamp_str);
            
            // Set system time
            struct timeval tv;
            tv.tv_sec = timestamp;
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
            
            // Log the time synchronization
            struct tm timeinfo;
            localtime_r(&timestamp, &timeinfo);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "Time synchronized to: %s", time_str);
            
            httpd_resp_send(req, "Time synchronized successfully", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid timestamp parameter");
    return ESP_FAIL;
}

void url_decode(char *dst, const char *src, size_t dst_size) {
    char *end = dst + dst_size - 1;
    while (*src && dst < end) {
        if (*src == '%' && src[1] && src[2]) {
            int hex_val;
            if (sscanf(src + 1, "%2x", &hex_val) == 1) {
                *dst++ = (char)hex_val;
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

esp_err_t delete_file_handler(httpd_req_t *req) {
    char query[256];
    char filename[128];
    char filepath[256];
    
    // Get query string length
    size_t query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0) {
        ESP_LOGE(TAG, "No query string found");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, 
            "<html><body>"
            "<h2>Error: Missing filename parameter</h2>"
            "<a href='/browse_sd'>Back to SD Card</a>"
            "</body></html>", 
            HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    // Get the query string
    if (query_len >= sizeof(query)) {
        ESP_LOGE(TAG, "Query string too long");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, 
            "<html><body>"
            "<h2>Error: Query string too long</h2>"
            "<a href='/browse_sd'>Back to SD Card</a>"
            "</body></html>", 
            HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get query string");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, 
            "<html><body>"
            "<h2>Error: Failed to parse query</h2>"
            "<a href='/browse_sd'>Back to SD Card</a>"
            "</body></html>", 
            HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Query string: %s", query);

    // Extract filename from query string
    if (httpd_query_key_value(query, "file", filename, sizeof(filename)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to extract filename from query");
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, 
            "<html><body>"
            "<h2>Error: Missing filename parameter</h2>"
            "<a href='/browse_sd'>Back to SD Card</a>"
            "</body></html>", 
            HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Raw filename: %s", filename);

    // URL decode the filename
    char decoded_filename[128];
    url_decode(decoded_filename, filename, sizeof(decoded_filename));
    
    ESP_LOGI(TAG, "Decoded filename: %s", decoded_filename);

    // Construct full path
    snprintf(filepath, sizeof(filepath), "%s/%s", CONFIG_SD_MOUNT_POINT, decoded_filename);
    
    ESP_LOGI(TAG, "Full filepath: %s", filepath);

    // Check if file exists
    if (access(filepath, F_OK) != 0) {
        ESP_LOGE(TAG, "File not found: %s", filepath);
        // File doesn't exist, just redirect back (maybe already deleted)
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/browse_sd");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    // Delete the file
    if (unlink(filepath) == 0) {
        ESP_LOGI(TAG, "File deleted successfully: %s", decoded_filename);
        
        // Redirect back to browse page with cache control headers
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/browse_sd");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to delete file: %s (errno: %d)", filepath, errno);
        
        // Redirect back even on error to avoid header issues
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/browse_sd");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
        httpd_resp_set_hdr(req, "Pragma", "no-cache");
        httpd_resp_set_hdr(req, "Expires", "0");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
}

esp_err_t browse_sd_get_handler(httpd_req_t *req) {
    size_t response_size = 8192;
    char *response = malloc(response_size);
    if (!response) {
        ESP_LOGE(TAG, "Failed to allocate memory for SD card response.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t used = snprintf(response, response_size,
        "<html><head><title>SD Card</title>"
        "<style>"
        ".file-item { margin: 5px 0; padding: 5px; border: 1px solid #ddd; }"
        ".delete-btn { background-color: #ff4444; color: white; padding: 2px 8px; "
        "text-decoration: none; border-radius: 3px; margin-left: 10px; font-size: 12px; "
        "border: none; cursor: pointer; }"
        ".delete-btn:hover { background-color: #cc0000; }"
        ".download-link { margin-right: 10px; }"
        "</style>"
        "<script>"
        "function confirmDelete(filename, form) {"
        "  if (confirm('Are you sure you want to delete \"' + filename + '\"?\\n\\nThis action cannot be undone.')) {"
        "    form.submit();"
        "  }"
        "  return false;"
        "}"
        "</script>"
        "</head><body>"
        "<div style='margin-top: 30px;'>"
        "<button onclick=\"location.href='/'\">Back to Home</button>"
        "</div>"
        "<h1>SD Card Contents</h1>");

    // Count .pcap files before ZIP creation
    int pcap_count = 0;
    DIR *precount = opendir(CONFIG_SD_MOUNT_POINT);
    if (precount) {
        struct dirent *entry;
        while ((entry = readdir(precount)) != NULL) {
            if (entry->d_type == DT_REG && strstr(entry->d_name, ".pcap")) {
                pcap_count++;
            }
        }
        closedir(precount);
    }

    // Create ZIP only if 2 or more files
    if (pcap_count >= 2) {
        ESP_LOGI(TAG, "Creating ZIP archive of SD card files...");
        if (create_zip_archive(CONFIG_SD_MOUNT_POINT, ZIP_FILE_PATH) != ESP_OK) {
            httpd_resp_send_500(req);
            free(response);
            return ESP_FAIL;
        }
    }

    used += snprintf(response + used, response_size - used, "<h2>Download Archive</h2><ul>");
    int part_number = 1;
    char zip_filename[256];
    while (true) {
        snprintf(zip_filename, sizeof(zip_filename), "%s_part%d.zip", CONFIG_SD_MOUNT_POINT "/sd_files.zip", part_number);
        FILE *file = fopen(zip_filename, "r");
        if (!file) break;
        fclose(file);

        used += snprintf(response + used, response_size - used,
            "<li><a href=\"/download?file=sd_files.zip_part%d.zip\">Download Part %d</a></li>",
            part_number, part_number);
        part_number++;
    }
    used += snprintf(response + used, response_size - used, "</ul>");

    used += snprintf(response + used, response_size - used, "<h2>Files and Directories</h2>");

    DIR *dir = opendir(CONFIG_SD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SD card directory");
        httpd_resp_send(req, "Failed to open SD card directory", HTTPD_RESP_USE_STRLEN);
        free(response);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (used + 1024 >= response_size) {
            response_size *= 2;
            response = realloc(response, response_size);
            if (!response) {
                ESP_LOGE(TAG, "Failed to realloc response buffer");
                httpd_resp_send_500(req);
                closedir(dir);
                return ESP_FAIL;
            }
        }

        if (entry->d_type == DT_REG) {
            // URL-encode file name for download link
            char encoded_name[256] = {0};
            const char *src = entry->d_name;
            char *dst = encoded_name;
            while (*src && (dst - encoded_name) < sizeof(encoded_name) - 4) {
                if (isalnum((unsigned char)*src) || *src == '.' || *src == '_' || *src == '-') {
                    *dst++ = *src;
                } else {
                    dst += sprintf(dst, "%%%02X", (unsigned char)*src);
                }
                src++;
            }
            *dst = '\0';

            // Create file entry with improved delete confirmation
            used += snprintf(response + used, response_size - used,
                "<div class=\"file-item\">"
                "<span class=\"download-link\"><a href=\"/download?file=%s\">%s</a></span>"
                "<form method='GET' action='/delete_file' style='display:inline;' "
                "onsubmit='return confirmDelete(\"%s\", this);'>"
                "<input type='hidden' name='file' value='%s'>"
                "<button class='delete-btn' type='submit'>Delete</button>"
                "</form>"
                "</div>",
                encoded_name, entry->d_name, entry->d_name, encoded_name);
        } else if (entry->d_type == DT_DIR) {
            used += snprintf(response + used, response_size - used,
                "<div class=\"file-item\"><b>%s/</b></div>", entry->d_name);
        }
    }
    closedir(dir);

    used += snprintf(response + used, response_size - used, "</body></html>");

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return ESP_OK;
}

esp_err_t download_file_handler(httpd_req_t *req) {
    char filepath[256];
    const char *filename = req->uri + strlen("/download?file=");
    snprintf(filepath, sizeof(filepath), "%s/%s", CONFIG_SD_MOUNT_POINT, filename);

    FILE *file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    const char *content_type = get_content_type(filename);

    httpd_resp_set_type(req, content_type);
    char content_disposition[128];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed.");
            fclose(file);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0);
    ESP_LOGI(TAG, "File %s sent successfully.", filename);
    return ESP_OK;
}

esp_err_t stop_server_handler(httpd_req_t *req) {
    
    const char* response_html = 
        "<html><head><title>Starting Probing</title>"
        "<meta http-equiv=\"refresh\" content=\"5;url=/\" />"
        "</head><body>"
        "<h1>Starting Probing Mode</h1>"
        "<p>Stopping webserver and starting sniffer...</p>"
        "</body></html>";

    // Send the response to the client
    httpd_resp_send(req, response_html, HTTPD_RESP_USE_STRLEN);
    
    // Create a task to stop the server and restart probing
    xTaskCreate(stop_server_task, "stop_server_task", 4096, NULL, 5, NULL);
    
    return ESP_OK;
}

void stop_server_task(void *pvParameters) {
    
    ESP_LOGI(TAG, "Starting task to stop server and restart probing");
    
    // Delay a bit to allow the HTTP response to be sent
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // Stop the webserver
    stop_captive_server();
    
    // Toggle the start_server flag to false to resume sniffing
    start_server = false;
    
    ESP_LOGI(TAG, "Server stopped and server flag reset");
    
    // Delete this task
    vTaskDelete(NULL);
}

const char *get_content_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".csv") == 0) return "text/csv";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    return "application/octet-stream";
}

// Modified start_webserver function to register the delete handler
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 17; // Increased from 16 to accommodate delete handler

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_uri_t browse_sd_uri = { .uri = "/browse_sd", .method = HTTP_GET, .handler = browse_sd_get_handler };
        httpd_uri_t download_uri = { .uri = "/download", .method = HTTP_GET, .handler = download_file_handler};
        httpd_uri_t delete_file_uri = { .uri = "/delete_file", .method = HTTP_GET, .handler = delete_file_handler};
        httpd_uri_t favicon_uri = { .uri = "/favicon.ico",.method = HTTP_GET,.handler = favicon_handler};
        httpd_uri_t settings_uri = { .uri = "/settings", .method = HTTP_GET, .handler = settings_get_handler};
        httpd_uri_t sync_time_uri = { .uri = "/sync_time", .method = HTTP_GET, .handler = sync_time_handler};
        httpd_uri_t stop_server_uri = { .uri = "/stop_server", .method = HTTP_GET, .handler = stop_server_handler};
        httpd_uri_t set_wifi_uri = { .uri = "/set_wifi", .method = HTTP_GET, .handler = set_wifi_handler};
        httpd_uri_t set_short_period_uri = { .uri = "/set_short_period", .method = HTTP_GET, .handler = set_period_handler};
        httpd_uri_t set_medium_period_uri = { .uri = "/set_medium_period", .method = HTTP_GET, .handler = set_period_handler};
        httpd_uri_t set_long_period_uri = { .uri = "/set_long_period", .method = HTTP_GET, .handler = set_period_handler};
        httpd_uri_t uri_oled_flip = { .uri = "/oled_flip", .method = HTTP_GET, .handler= oled_flip_handler};
        httpd_uri_t uri_battery_status = { .uri = "/battery_status", .method = HTTP_GET, .handler= battery_status_handler};
        httpd_uri_t set_server_wifi_uri = { .uri = "/set_server_wifi", .method = HTTP_GET, .handler = set_server_wifi_handler};

        httpd_register_uri_handler(server_handle, &root_uri);
        httpd_register_uri_handler(server_handle, &browse_sd_uri);
        httpd_register_uri_handler(server_handle, &download_uri);
        httpd_register_uri_handler(server_handle, &delete_file_uri);
        httpd_register_uri_handler(server_handle, &favicon_uri);
        httpd_register_uri_handler(server_handle, &settings_uri);
        httpd_register_uri_handler(server_handle, &sync_time_uri);
        httpd_register_uri_handler(server_handle, &stop_server_uri);
        httpd_register_uri_handler(server_handle, &set_wifi_uri);
        httpd_register_uri_handler(server_handle, &set_short_period_uri);
        httpd_register_uri_handler(server_handle, &set_medium_period_uri);
        httpd_register_uri_handler(server_handle, &set_long_period_uri);
        httpd_register_uri_handler(server_handle, &uri_oled_flip);
        httpd_register_uri_handler(server_handle, &uri_battery_status);
        httpd_register_uri_handler(server_handle, &set_server_wifi_uri);

        ESP_LOGI(TAG, "Webserver started successfully.");
        return server_handle;
    }

    ESP_LOGE(TAG, "Failed to start webserver.");
    return NULL;
}

esp_err_t start_captive_server(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    // Destroy existing AP interface if it exists
    if (ap_netif) {
        esp_netif_destroy_default_wifi(ap_netif);
        ap_netif = NULL;
    }

    // Create a new AP interface
    ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Failed to create default Wi-Fi AP interface");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    // Use custom SSID and password if they're set, otherwise use defaults
    if (strlen(server_wifi_ssid) > 0) {
        strncpy((char *)wifi_config.ap.ssid, server_wifi_ssid, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen(server_wifi_ssid);
        ESP_LOGI(TAG, "Using custom captive portal SSID: %s", server_wifi_ssid);
    } else {
        strncpy((char *)wifi_config.ap.ssid, SERVER_WIFI_SSID, sizeof(wifi_config.ap.ssid));
        wifi_config.ap.ssid_len = strlen(wifi_ssid);
        ESP_LOGI(TAG, "Using default captive portal SSID: %s", wifi_ssid);
    }
    
    if (strlen(server_wifi_password) > 0) {
        strncpy((char *)wifi_config.ap.password, server_wifi_password, sizeof(wifi_config.ap.password));
        ESP_LOGI(TAG, "Using custom captive portal password");
    } else {
        strncpy((char *)wifi_config.ap.password, "12345678", sizeof(wifi_config.ap.password));
        ESP_LOGI(TAG, "Using default captive portal password: 12345678");
    }

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