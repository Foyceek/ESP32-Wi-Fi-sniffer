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

static const char *html_page_template =
    "<html><head><title>ESP32 Portal - Home</title>"
    "<style>"
    "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background-color: #f5f5f5; }"
    "h1 { color: #333; text-align: center; margin-bottom: 30px; }"
    "h2 { color: #555; border-bottom: 2px solid #007bff; padding-bottom: 5px; margin-top: 30px; }"
    ".nav-buttons { display: flex; flex-direction: column; gap: 15px; margin: 30px 0; }"
    ".btn { "
    "  background: linear-gradient(135deg, #007bff, #0056b3); "
    "  color: white; "
    "  border: none; "
    "  border-radius: 8px; "
    "  padding: 18px 30px; "
    "  font-size: 18px; "
    "  font-weight: 600; "
    "  cursor: pointer; "
    "  transition: all 0.2s ease; "
    "  text-align: center; "
    "  text-decoration: none; "
    "  display: inline-block; "
    "  box-shadow: 0 4px 8px rgba(0,0,0,0.1); "
    "}"
    ".btn:hover { transform: translateY(-2px); box-shadow: 0 6px 12px rgba(0,0,0,0.15); background: linear-gradient(135deg, #0056b3, #004085); }"
    ".btn:active { transform: translateY(0); }"
    ".btn-danger { background: linear-gradient(135deg, #dc3545, #c82333); }"
    ".btn-danger:hover { background: linear-gradient(135deg, #c82333, #a71e2a); }"
    "table { width: 100%; border-collapse: collapse; margin: 15px 0; background: white; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
    "th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }"
    "th { background-color: #007bff; color: white; font-weight: 600; }"
    "th:nth-child(1) { width: 8%; }"  /* Rank column */
    "th:nth-child(2) { width: 25%; min-width: 160px; white-space: nowrap; }"  /* Time column - wider with min-width and no-wrap */
    "th:nth-child(3) { width: 57%; }"  /* MAC Address column */
    "th:nth-child(4) { width: 10%; }"  /* RSSI column */
    "tr:nth-child(even) { background-color: #f8f9fa; }"
    "tr:hover { background-color: #e9ecef; }"
    "pre { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); overflow-x: auto; }"
    "@media (min-width: 600px) { .nav-buttons { flex-direction: row; justify-content: center; } }"
    "</style></head>"
    "<body>"
    "<h1>ESP32 Captive Portal</h1>"
    "<div class=\"nav-buttons\">"
    "<button class=\"btn\" onclick=\"location.href='/browse_sd'\">Browse SD Card</button>"
    "<button class=\"btn\" onclick=\"location.href='/settings'\">Settings</button>"
    "<button class=\"btn btn-danger\" onclick=\"location.href='/stop_server'\">Stop Server</button>"
    "</div>"
    "<h2>Top Requests</h2>"
    "<table><tr><th>Rank</th><th>Time</th><th>MAC Address</th><th>RSSI</th></tr>%s</table>"
    "<h2>RSSI Bar Graph</h2><pre>%s</pre>"
    "</body></html>";


static char* generate_top_requests_table(void) {
    // First, check if we have any valid data to display
    bool has_data = false;
    for (int i = 0; i < TOP_REQUESTS_COUNT && i <= 20; i++) {
        if (top_requests[i].mac_address[0] != '\0' && top_requests[i].rssi != 0) {
            has_data = true;
            break;
        }
    }
    
    // If no data, return a simple "no data" message
    if (!has_data) {
        char *no_data_msg = malloc(128);
        if (!no_data_msg) {
            ESP_LOGE(TAG, "Failed to allocate memory for no data message");
            return NULL;
        }
        strcpy(no_data_msg, "<tr><td colspan=\"4\" style=\"text-align: center; color: #6c757d; font-style: italic;\">No request data available yet</td></tr>");
        return no_data_msg;
    }
    
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
            size_t needed_size = len + 256; 
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

static char* generate_svg_bar_graph(void) {
    // First, check if for RSSI data to display
    bool has_data = false;
    int total_packets = 0;
    
    for (int i = 0; i < NUM_BARS; i++) {
        if (rssi_ranges[i] > 0) {
            has_data = true;
            total_packets += rssi_ranges[i];
        }
    }
    
    // If no data, return a simple "no data" message
    if (!has_data) {
        char *no_data_msg = malloc(512);
        if (!no_data_msg) {
            ESP_LOGE(TAG, "Failed to allocate memory for no data message");
            return NULL;
        }
        strcpy(no_data_msg, 
            "<div style=\"text-align: center; color: #6c757d; font-style: italic; padding: 40px; "
            "background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);\">"
            "No RSSI data available yet<br>"
            "<small>Start probing to collect WiFi signal strength data</small>"
            "</div>");
        return no_data_msg;
    }
    
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
                    "<svg width=\"%d\" height=\"500\" xmlns=\"http://www.w3.org/2000/svg\" "
                    "style=\"background: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);\">\n", 
                    NUM_BARS * 50);

    // Add a title to the graph
    len += snprintf(svg_buffer + len, buffer_size - len,
                    "<text x=\"10\" y=\"20\" font-family=\"Arial\" font-size=\"14\" font-weight=\"bold\" fill=\"#333\">"
                    "RSSI Distribution (Total: %d packets)</text>\n", total_packets);

    for (int i = 0; i < NUM_BARS; i++) {
        // Skip bars with no data
        if (rssi_ranges[i] == 0) continue;
        
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
        int y_position = i * 50 + 30; // Offset for title

        // Add gradient colors based on RSSI strength
        const char* bar_color;
        if (i < 2) bar_color = "#28a745";      // Strong signal - green
        else if (i < 5) bar_color = "#ffc107"; // Medium signal - yellow
        else bar_color = "#dc3545";            // Weak signal - red

        len += snprintf(svg_buffer + len, buffer_size - len,
                        "<rect x=\"0\" y=\"%d\" width=\"%d\" height=\"40\" fill=\"%s\" "
                        "stroke=\"#fff\" stroke-width=\"1\" />\n", 
                        y_position, bar_width, bar_color);

        len += snprintf(svg_buffer + len, buffer_size - len,
                        "<text x=\"%d\" y=\"%d\" font-family=\"Arial\" font-size=\"12\" fill=\"#333\">"
                        "%d to %d dBm: %d packets</text>\n",
                        bar_width + 5, y_position + 25, rssi_range_starts[i], rssi_range_ends[i], rssi_ranges[i]);
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

esp_err_t create_zip_archive(const char *directory_path, const char *zip_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    int file_count = 0;

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
    size_t total_required = template_len + table_len + svg_len + 100;
    
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
    
    // Check available heap before proceeding
    size_t free_heap = esp_get_free_heap_size();
    ESP_LOGI("SETTINGS", "Free heap: %d bytes", free_heap);
    
    if (free_heap < 20000) {
        ESP_LOGW("SETTINGS", "Low memory - cannot generate settings page");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Low memory");
        return ESP_FAIL;
    }
    
    // Allocate HTML buffer dynamically from heap instead of stack
    const size_t html_size = 12288;
    char* html = (char*)malloc(html_size);
    if (!html) {
        ESP_LOGE("SETTINGS", "Failed to allocate memory for HTML");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }
    
    ESP_LOGI("SETTINGS", "Allocated %d bytes for HTML buffer", html_size);
    
    // Get current ESP32 time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char esp32_time_str[64];
    strftime(esp32_time_str, sizeof(esp32_time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

    // Build the HTML content using the allocated buffer
    int result = snprintf(html, html_size,
        "<html><head><title>ESP32 Settings</title>"
        "<style>"
        "body{font-family:Arial;max-width:800px;margin:0 auto;padding:20px;background:#f5f5f5}"
        "h1{color:#333;text-align:center;margin-bottom:30px}"
        "h2{color:#555;border-bottom:2px solid #007bff;padding-bottom:5px;margin:30px 0 20px}"
        ".sec{background:white;padding:20px;margin:15px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        ".row{display:flex;align-items:center;gap:10px;margin:10px 0;flex-wrap:wrap}"
        "label{font-weight:600;color:#555;min-width:100px}"
        "input{padding:10px;border:2px solid #e0e0e0;border-radius:6px;font-size:16px;flex:1;min-width:180px}"
        "input:focus{outline:none;border-color:#007bff}"
        ".btn{background:linear-gradient(135deg,#007bff,#0056b3);color:white;border:none;border-radius:8px;"
        "padding:12px 20px;font-size:16px;font-weight:600;cursor:pointer;transition:all 0.2s;"
        "box-shadow:0 3px 6px rgba(0,0,0,0.1);min-width:120px}"
        ".btn:hover{transform:translateY(-1px);background:linear-gradient(135deg,#0056b3,#004085)}"
        ".btn-ok{background:linear-gradient(135deg,#28a745,#1e7e34)}"
        ".btn-ok:hover{background:linear-gradient(135deg,#1e7e34,#155724)}"
        ".btn-warn{background:linear-gradient(135deg,#ffc107,#e0a800);color:#212529}"
        ".btn-warn:hover{background:linear-gradient(135deg,#e0a800,#d39e00)}"
        ".btn-back{background:linear-gradient(135deg,#6c757d,#545b62);margin-bottom:20px}"
        ".time-box{display:flex;flex-direction:column;gap:15px;background:#f8f9fa;padding:20px;"
        "border-radius:6px;margin:10px 0;border-left:4px solid #007bff}"
        ".time-item{display:flex;justify-content:space-between;align-items:center;padding:10px;"
        "background:white;border-radius:4px;border:1px solid #dee2e6}"
        ".time-label{font-weight:600;color:#495057;min-width:120px}"
        ".time-val{font-family:monospace;font-size:16px;font-weight:bold;color:#007bff}"
        ".msg{padding:8px 12px;border-radius:4px;margin-top:8px;font-weight:500}"
        ".msg.ok{background:#d4edda;color:#155724}"
        ".msg.err{background:#f8d7da;color:#721c24}"
        ".period{display:flex;align-items:center;gap:10px;padding:12px;background:#f8f9fa;"
        "border-radius:6px;margin:10px 0;border-left:4px solid #28a745}"
        ".period span{font-weight:600;min-width:80px}"
        ".period code{color:#007bff;font-weight:bold}"
        ".status-item{display:flex;justify-content:space-between;align-items:center;padding:15px;"
        "background:#f8f9fa;border-radius:6px;margin:10px 0;border-left:4px solid #007bff}"
        ".status-label{font-weight:600;color:#495057}"
        ".status-value{font-weight:bold;padding:6px 12px;border-radius:4px;color:white;"
        "background:linear-gradient(135deg,#28a745,#1e7e34)}"
        ".status-value.flipped{background:linear-gradient(135deg,#fd7e14,#e85d04)}"
        ".status-value.hidden{background:linear-gradient(135deg,#dc3545,#b02a37)}"
        "@media (max-width:600px){.row,.period{flex-direction:column;gap:10px}}"
        "@media (max-width:600px){.time-item,.status-item{flex-direction:column;text-align:center;gap:5px}}"
        "</style>"
        "<script>"
        "let esp32TimeOffset=0;"
        "function syncTime(){"
        "fetch('/sync_time?timestamp='+Math.floor(Date.now()/1000))"
        ".then(r=>r.text()).then(()=>{showMsg('sync-result','Time synchronized!','ok');initializeTimeOffset();})"
        ".catch(e=>showMsg('sync-result','Error: '+e,'err'));"
        "}"
        "function updateTimes(){"
        "const now=new Date();"
        "const clientTime=now.toLocaleString('en-CA',{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'}).replace(/,/g,'');"
        "document.getElementById('client-time').textContent=clientTime;"
        "const esp32Time=new Date(now.getTime()+esp32TimeOffset);"
        "const esp32TimeStr=esp32Time.toLocaleString('en-CA',{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'}).replace(/,/g,'');"
        "document.getElementById('esp32-time').textContent=esp32TimeStr;"
        "}"
        "function initializeTimeOffset(){"
        "const clientTime=new Date().getTime();"
        "fetch('/get_esp32_time').then(r=>r.text()).then(timeStr=>{"
        "const esp32Time=new Date(timeStr.replace(' ','T')).getTime();"
        "esp32TimeOffset=esp32Time-clientTime;updateTimes();"
        "}).catch(e=>{console.error('Error getting ESP32 time:',e);esp32TimeOffset=0;updateTimes();});"
        "}"
        "window.onload=function(){initializeTimeOffset();setInterval(updateTimes,1000);};"
        "function setPeriod(id,url){"
        "let v=document.getElementById(id+'_input').value;"
        "if(isNaN(v)||v<1000)return showMsg(id+'-result','Enter >= 1000','err');"
        "fetch(url+v).then(r=>r.text()).then(()=>showMsg(id+'-result','Updated!','ok'))"
        ".catch(e=>showMsg(id+'-result','Error: '+e,'err'));"
        "}"
        "function setWifi(){"
        "let s=encodeURIComponent(document.getElementById('ssid_input').value);"
        "let p=encodeURIComponent(document.getElementById('pass_input').value);"
        "fetch('/set_wifi?ssid='+s+'&password='+p)"
        ".then(r=>r.text()).then(()=>showMsg('wifi-result','WiFi updated!','ok'))"
        ".catch(e=>showMsg('wifi-result','Error: '+e,'err'));"
        "}"
        "function setServerWifi(){"
        "let s=encodeURIComponent(document.getElementById('server_ssid_input').value);"
        "let p=encodeURIComponent(document.getElementById('server_pass_input').value);"
        "fetch('/set_server_wifi?server_ssid='+s+'&server_password='+p)"
        ".then(r=>r.text()).then(()=>showMsg('server-wifi-result','WiFi updated!','ok'))"
        ".catch(e=>showMsg('server-wifi-result','Error: '+e,'err'));"
        "}"
        "function showMsg(id,msg,type){"
        "let e=document.getElementById(id);e.textContent=msg;e.className='msg '+type;"
        "setTimeout(()=>{e.textContent='';e.className='msg';},3000);"
        "}"
        "function updateStatusDisplay(statusId,newStatus,className){"
        "let e=document.getElementById(statusId);e.textContent=newStatus;e.className='status-value '+className;"
        "}"
        "function flipOLED(){"
        "fetch('/oled_flip').then(r=>r.text()).then(()=>{"
        "showMsg('flip-result','OLED Rotated!','ok');"
        "let currentStatus=document.getElementById('oled-status').textContent;"
        "if(currentStatus==='Default'){updateStatusDisplay('oled-status','Flipped','flipped');}"
        "else{updateStatusDisplay('oled-status','Default','');}"
        "}).catch(e=>showMsg('flip-result','Error: '+e,'err'));"
        "}"
        "function batteryStatus(){"
        "fetch('/battery_status').then(r=>r.text()).then(()=>{"
        "showMsg('battery-result','Battery Status Toggled!','ok');"
        "let currentStatus=document.getElementById('battery-status').textContent;"
        "if(currentStatus==='Visible'){updateStatusDisplay('battery-status','Hidden','hidden');}"
        "else{updateStatusDisplay('battery-status','Visible','');}"
        "}).catch(e=>showMsg('battery-result','Error: '+e,'err'));"
        "}"
        "</script>"
        "</head><body>"
        
        "<button class='btn btn-back' onclick=\"location.href='/'\">Back to Home</button>"
        "<h1>ESP32 Settings</h1>"

        "<div class='sec'><h2>Time Sync</h2>"
        "<div class='time-box'>"
        "<div class='time-item'>"
        "<span class='time-label'>Your Device:</span>"
        "<span class='time-val' id='client-time'></span>"
        "</div>"
        "<div class='time-item'>"
        "<span class='time-label'>ESP32:</span>"
        "<span class='time-val' id='esp32-time'></span>"
        "</div>"
        "</div>"
        "<button class='btn btn-ok' onclick='syncTime()'>Sync Time</button>"
        "<div id='sync-result' class='msg'></div></div>"

        "<div class='sec'><h2>Display</h2>"
        "<div class='status-item'>"
        "<span class='status-label'>OLED Orientation:</span>"
        "<span id='oled-status' class='status-value%s'>%s</span>"
        "</div>"
        "<button class='btn btn-warn' onclick='flipOLED()'>Flip OLED</button>"
        "<div id='flip-result' class='msg'></div><br><br>"
        
        "<div class='status-item'>"
        "<span class='status-label'>Battery Display:</span>"
        "<span id='battery-status' class='status-value%s'>%s</span>"
        "</div>"
        "<button class='btn btn-warn' onclick='batteryStatus()'>Toggle Battery</button>"
        "<div id='battery-result' class='msg'></div></div>"

        "<div class='sec'><h2>WiFi Settings</h2>"
        "<h3>Time Sync WiFi</h3>"
        "<div class='row'><label>SSID:</label><input id='ssid_input' value='%s'></div>"
        "<div class='row'><label>Password:</label><input id='pass_input' type='password' value='%s'></div>"
        "<button class='btn' onclick='setWifi()'>Update</button>"
        "<div id='wifi-result' class='msg'></div>"
        
        "<h3>Server WiFi</h3>"
        "<div class='row'><label>SSID:</label><input id='server_ssid_input' value='%s'></div>"
        "<div class='row'><label>Password:</label><input id='server_pass_input' type='password' value='%s'></div>"
        "<button class='btn' onclick='setServerWifi()'>Update</button>"
        "<div id='server-wifi-result' class='msg'></div></div>"

        "<div class='sec'><h2>OLED Periods</h2>"
        "<div class='period'><span>Short:</span><code>%d ms</code><input id='short_input' type='number' min='1000'>"
        "<button class='btn' onclick=\"setPeriod('short','/set_short_period?value=')\">Set</button></div>"
        "<div id='short-result' class='msg'></div>"
        
        "<div class='period'><span>Medium:</span><code>%d ms</code><input id='medium_input' type='number' min='1000'>"
        "<button class='btn' onclick=\"setPeriod('medium','/set_medium_period?value=')\">Set</button></div>"
        "<div id='medium-result' class='msg'></div>"
        
        "<div class='period'><span>Long:</span><code>%d ms</code><input id='long_input' type='number' min='1000'>"
        "<button class='btn' onclick=\"setPeriod('long','/set_long_period?value=')\">Set</button></div>"
        "<div id='long-result' class='msg'></div></div>"

        "</body></html>",
        flip_oled ? " flipped" : "",  // CSS class for OLED status
        flip_oled ? "Flipped" : "Default",  // OLED status text
        display_battery_data ? "" : " hidden",  // CSS class for battery status  
        display_battery_data ? "Visible" : "Hidden",  // Battery status text
        wifi_ssid, wifi_password, server_wifi_ssid, server_wifi_password, 
        short_oled_period, medium_oled_period, long_oled_period);

    // Check if snprintf was successful and didn't truncate
    if (result >= html_size) {
        ESP_LOGW("SETTINGS", "HTML content was truncated! Need %d bytes, have %d", result, html_size);
        free(html);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "HTML too large");
        return ESP_FAIL;
    }
    
    ESP_LOGI("SETTINGS", "Generated HTML: %d bytes", result);
    
    // Send the response
    esp_err_t send_result = httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    
    // Clean up allocated memory
    free(html);
    
    if (send_result != ESP_OK) {
        ESP_LOGE("SETTINGS", "Failed to send HTTP response");
        return ESP_FAIL;
    }
    
    ESP_LOGI("SETTINGS", "Settings page sent successfully");
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
        // File doesn't exist, just redirect back
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
    size_t response_size = 12288; // Increased buffer size
    char *response = malloc(response_size);
    if (!response) {
        ESP_LOGE(TAG, "Failed to allocate memory for SD card response.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t used = snprintf(response, response_size,
        "<html><head><title>ESP32 Portal - SD Card</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background-color: #f5f5f5; }"
        "h1 { color: #333; text-align: center; margin-bottom: 30px; }"
        "h2 { color: #555; border-bottom: 2px solid #007bff; padding-bottom: 5px; margin-top: 30px; margin-bottom: 20px; }"
        ".section { background: white; padding: 25px; margin: 20px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }"
        ".btn { "
        "  background: linear-gradient(135deg, #007bff, #0056b3); "
        "  color: white; "
        "  border: none; "
        "  border-radius: 8px; "
        "  padding: 14px 25px; "
        "  font-size: 16px; "
        "  font-weight: 600; "
        "  cursor: pointer; "
        "  transition: all 0.2s ease; "
        "  text-decoration: none; "
        "  display: inline-block; "
        "  box-shadow: 0 3px 6px rgba(0,0,0,0.1); "
        "  min-width: 140px; "
        "}"
        ".btn:hover { transform: translateY(-1px); box-shadow: 0 4px 8px rgba(0,0,0,0.15); background: linear-gradient(135deg, #0056b3, #004085); }"
        ".btn:active { transform: translateY(0); }"
        ".btn-back { background: linear-gradient(135deg, #6c757d, #545b62); margin-bottom: 20px; }"
        ".btn-back:hover { background: linear-gradient(135deg, #545b62, #3d4245); }"
        ".btn-success { background: linear-gradient(135deg, #28a745, #1e7e34); }"
        ".btn-success:hover { background: linear-gradient(135deg, #1e7e34, #155724); }"
        ".btn-danger { background: linear-gradient(135deg, #dc3545, #c82333); padding: 8px 16px; font-size: 14px; min-width: auto; }"
        ".btn-danger:hover { background: linear-gradient(135deg, #c82333, #a71e2a); }"
        ".info-text { color: #6c757d; margin: 15px 0; font-size: 14px; line-height: 1.5; }"
        ".file-item { "
        "  display: flex; "
        "  align-items: center; "
        "  justify-content: space-between; "
        "  padding: 15px; "
        "  margin: 10px 0; "
        "  background: #f8f9fa; "
        "  border-radius: 6px; "
        "  border-left: 4px solid #007bff; "
        "  transition: all 0.2s ease; "
        "}"
        ".file-item:hover { background: #e9ecef; transform: translateX(5px); }"
        ".file-name { font-weight: 500; color: #495057; flex-grow: 1; }"
        ".file-name a { color: #007bff; text-decoration: none; font-weight: 600; }"
        ".file-name a:hover { text-decoration: underline; }"
        ".directory { font-weight: bold; color: #6c757d; }"
        "ul { list-style: none; padding: 0; }"
        "li { margin: 10px 0; }"
        "li a { "
        "  color: #007bff; "
        "  text-decoration: none; "
        "  font-weight: 600; "
        "  padding: 12px 20px; "
        "  background: #e9ecef; "
        "  border-radius: 6px; "
        "  display: inline-block; "
        "  transition: all 0.2s ease; "
        "}"
        "li a:hover { background: #007bff; color: white; transform: translateY(-1px); }"
        "@media (max-width: 600px) { "
        "  .file-item { flex-direction: column; align-items: stretch; gap: 10px; } "
        "  .file-name { text-align: center; } "
        "}"
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
        
        "<button class='btn btn-back' onclick=\"location.href='/'\">Back to Home</button>"
        
        "<h1>SD Card Browser</h1>"
        );

    // Count .pcap files for display info
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

    used += snprintf(response + used, response_size - used, 
        "<div class='section'>"
        "<h2>Download Archive</h2>"
        "<div class='info-text'>"
        "Create a ZIP archive containing all .pcap files (%d files found). "
        "This allows you to download all packet capture files in a single compressed archive."
        "</div>"
        "<form method='GET' action='/create_zip' style='display:inline;'>"
        "<button class='btn btn-success' type='submit'>Create ZIP Archive</button>"
        "</form>"
        "<ul>", pcap_count);
    
    // Check for existing ZIP parts and display download links
    int part_number = 1;
    char zip_filename[256];
    bool has_zip_parts = false;
    while (true) {
        snprintf(zip_filename, sizeof(zip_filename), "%s/sd_files.zip_part%d.zip", CONFIG_SD_MOUNT_POINT, part_number);
        FILE *file = fopen(zip_filename, "r");
        if (!file) break;
        fclose(file);
        has_zip_parts = true;

        used += snprintf(response + used, response_size - used,
            "<li><a href=\"/download?file=sd_files.zip_part%d.zip\">Download Part %d</a></li>",
            part_number, part_number);
        part_number++;
    }
    
    if (!has_zip_parts) {
        used += snprintf(response + used, response_size - used,
            "<li style='color: #6c757d; font-style: italic;'>No ZIP archive available. Click 'Create ZIP Archive' to generate one.</li>");
    }
    
    used += snprintf(response + used, response_size - used, "</ul></div>");

    used += snprintf(response + used, response_size - used, 
        "<div class='section'>"
        "<h2>Files and Directories</h2>");

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

            // Create file entry with improved styling (removed unicode icons)
            used += snprintf(response + used, response_size - used,
                "<div class=\"file-item\">"
                "<div class=\"file-name\"><a href=\"/download?file=%s\">%s</a></div>"
                "<form method='GET' action='/delete_file' style='display:inline;' "
                "onsubmit='return confirmDelete(\"%s\", this);'>"
                "<input type='hidden' name='file' value='%s'>"
                "<button class='btn btn-danger' type='submit'>Delete</button>"
                "</form>"
                "</div>",
                encoded_name, entry->d_name, entry->d_name, encoded_name);
        } else if (entry->d_type == DT_DIR) {
            used += snprintf(response + used, response_size - used,
                "<div class=\"file-item\">"
                "<div class=\"file-name directory\">%s/</div>"
                "</div>", entry->d_name);
        }
    }
    closedir(dir);

    used += snprintf(response + used, response_size - used, "</div></body></html>");

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    free(response);
    return ESP_OK;
}

esp_err_t create_zip_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Creating ZIP archive of SD card files...");
    
    // Create the ZIP archive
    esp_err_t result = create_zip_archive(CONFIG_SD_MOUNT_POINT, ZIP_FILE_PATH);
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create ZIP archive");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "ZIP archive created successfully");
    
    // Redirect back to the SD browser page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/browse_sd");
    httpd_resp_send(req, NULL, 0);
    
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

esp_err_t get_esp32_time_handler(httpd_req_t *req) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, time_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t stop_server_handler(httpd_req_t *req) {
    
    const char* response_html = 
        "<html><head><title>ESP32 Portal - Starting Probing</title>"
        "<meta http-equiv=\"refresh\" content=\"5;url=/\" />"
        "<style>"
        "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; background-color: #f5f5f5; }"
        "h1 { color: #333; text-align: center; margin-bottom: 30px; }"
        ".section { background: white; padding: 25px; margin: 20px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); text-align: center; }"
        ".status-message { "
        "  background: linear-gradient(135deg, #28a745, #1e7e34); "
        "  color: white; "
        "  padding: 20px; "
        "  border-radius: 8px; "
        "  margin: 20px 0; "
        "  font-size: 18px; "
        "  font-weight: 600; "
        "  box-shadow: 0 3px 6px rgba(0,0,0,0.1); "
        "}"
        ".progress-info { "
        "  color: #6c757d; "
        "  font-size: 16px; "
        "  margin: 15px 0; "
        "  line-height: 1.5; "
        "}"
        ".spinner { "
        "  border: 4px solid #f3f3f3; "
        "  border-top: 4px solid #007bff; "
        "  border-radius: 50%; "
        "  width: 40px; "
        "  height: 40px; "
        "  animation: spin 1s linear infinite; "
        "  margin: 20px auto; "
        "}"
        "@keyframes spin { "
        "  0% { transform: rotate(0deg); } "
        "  100% { transform: rotate(360deg); } "
        "}"
        ".countdown { "
        "  font-size: 14px; "
        "  color: #007bff; "
        "  font-weight: 600; "
        "  margin-top: 15px; "
        "}"
        "</style>"
        "<script>"
        "let countdown = 5;"
        "function updateCountdown() {"
        "  const element = document.getElementById('countdown');"
        "  if (element && countdown > 0) {"
        "    element.textContent = 'Redirecting in ' + countdown + ' second' + (countdown === 1 ? '' : 's') + '...';"
        "    countdown--;"
        "    setTimeout(updateCountdown, 1000);"
        "  } else if (element) {"
        "    element.textContent = 'Redirecting now...';"
        "  }"
        "}"
        "window.onload = function() { updateCountdown(); };"
        "</script>"
        "</head><body>"
        
        "<h1>ESP32 Captive Portal</h1>"
        
        "<div class='section'>"
        "<div class='status-message'>Starting Probing Mode</div>"
        "<div class='spinner'></div>"
        "<div class='progress-info'>"
        "The web server is being stopped and the WiFi packet sniffer is starting up.<br>"
        "This process will take a few moments to complete."
        "</div>"
        "<div class='countdown' id='countdown'>Redirecting in 5 seconds...</div>"
        "</div>"
        
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

httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.max_uri_handlers = 18; // Increased from 17 to accommodate new handler

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/browse_sd", .method = HTTP_GET, .handler = browse_sd_get_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/download", .method = HTTP_GET, .handler = download_file_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/delete_file", .method = HTTP_GET, .handler = delete_file_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/settings", .method = HTTP_GET, .handler = settings_get_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/sync_time", .method = HTTP_GET, .handler = sync_time_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/get_esp32_time", .method = HTTP_GET, .handler = get_esp32_time_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/stop_server", .method = HTTP_GET, .handler = stop_server_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/set_wifi", .method = HTTP_GET, .handler = set_wifi_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/set_short_period", .method = HTTP_GET, .handler = set_period_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/set_medium_period", .method = HTTP_GET, .handler = set_period_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/set_long_period", .method = HTTP_GET, .handler = set_period_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/oled_flip", .method = HTTP_GET, .handler = oled_flip_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/battery_status", .method = HTTP_GET, .handler = battery_status_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/set_server_wifi", .method = HTTP_GET, .handler = set_server_wifi_handler, .user_ctx = NULL});
        httpd_register_uri_handler(server_handle, &(httpd_uri_t){.uri = "/create_zip", .method = HTTP_GET, .handler = create_zip_get_handler, .user_ctx = NULL});

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