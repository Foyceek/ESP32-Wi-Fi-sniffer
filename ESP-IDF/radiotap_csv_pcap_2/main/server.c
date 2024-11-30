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

#define TAG "Captive_Portal"
#define WIFI_SSID "ESP32_Captive_AP"
#define HOSTNAME "esp32.local"
#define ESP_AP_IP "192.168.4.1"
#define NUM_BARS 10
#define MAX_BAR_HEIGHT 200
#define ZIP_FILE_PATH CONFIG_SD_MOUNT_POINT "/sd_files.zip"
#define MAX_FILES_TO_ADD 10

const char *get_content_type(const char *filename);

static httpd_handle_t server_handle = NULL;
static esp_netif_t *ap_netif = NULL;

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
    "<html><head><title>Welcome</title></head>"
    "<body><h1>Welcome to ESP32 Captive Portal</h1>"
    "<h2>Top Requests</h2>"
    "<table border='1'><tr><th>Rank</th><th>Time</th><th>MAC Address</th><th>RSSI</th></tr>%s</table>"
    "<h2>RSSI Bar Graph</h2><pre>%s</pre>"
    "<button onclick=\"location.href='/browse_sd'\">Browse SD Card</button>"
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

    const int rssi_range_starts[NUM_BARS] = {-0, -10, -20, -30, -40, -50, -60, -70, -80, -90};
    const int rssi_range_ends[NUM_BARS] = {-9, -19, -29, -39, -49, -59, -69, -79, -89, -99};
    
    len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                    "<svg width=\"%d\" height=\"500\" xmlns=\"http://www.w3.org/2000/svg\">\n", NUM_BARS * 50);

    for (int i = 0; i < NUM_BARS; i++) {
        int bar_width = (rssi_ranges[i] > MAX_BAR_HEIGHT) ? MAX_BAR_HEIGHT : rssi_ranges[i];
        int y_position = i * 50;

        len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                         "<rect x=\"0\" y=\"%d\" width=\"%d\" height=\"40\" fill=\"steelblue\" />\n", 
                         y_position, bar_width);

        len += snprintf(svg_buffer + len, sizeof(svg_buffer) - len,
                         "<text x=\"%d\" y=\"%d\" font-family=\"Arial\" font-size=\"12\" fill=\"black\">Range %d to %d dBm: %d packets</text>\n",
                         bar_width + 5, y_position + 20, rssi_range_starts[i], rssi_range_ends[i], rssi_ranges[i]);
    }

    snprintf(svg_buffer + len, sizeof(svg_buffer) - len, "</svg>\n");
    return svg_buffer;
}

esp_err_t create_zip_archive(const char *directory_path, const char *zip_path) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_writer_init_file(&zip_archive, zip_path, 0)) {
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
    uint8_t *buffer;
    int file_counter = 0;

    while ((entry = readdir(dir)) != NULL && file_counter < MAX_FILES_TO_ADD) {
        if (entry->d_type == DT_REG) {
            int len = snprintf(full_file_path, sizeof(full_file_path), "%s/%s", directory_path, entry->d_name);
            if (len >= sizeof(full_file_path)) {
                ESP_LOGW(TAG, "Path is too long, skipping file: %s/%s", directory_path, entry->d_name);
                continue;
            }

            if (strstr(entry->d_name, ".zip")) {
                ESP_LOGW(TAG, "Skipping file due to extension: %s", entry->d_name);
                continue;
            }

            ESP_LOGI(TAG, "Adding file: %s", full_file_path);

            FILE *file = fopen(full_file_path, "rb");
            if (!file) {
                ESP_LOGW(TAG, "Failed to open file: %s", full_file_path);
                continue;
            }

            size_t file_size = get_file_size(full_file_path);
            size_t buffer_size = file_size < 1024 ? 1024 : file_size;
            buffer = malloc(buffer_size);

            if (!buffer) {
                ESP_LOGE(TAG, "Memory allocation failed for file: %s", full_file_path);
                fclose(file);
                continue;
            }

            size_t read_bytes;
            while ((read_bytes = fread(buffer, 1, buffer_size, file)) > 0) {
                if (!mz_zip_writer_add_mem(&zip_archive, entry->d_name, buffer, read_bytes, MZ_NO_COMPRESSION)) {
                    ESP_LOGE(TAG, "Failed to add file to ZIP: %s, Error: %s", entry->d_name, mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
                    free(buffer);
                    fclose(file);
                    break;
                }
            }

            free(buffer);
            fclose(file);
            file_counter++;
        }
    }
    closedir(dir);

    if (!mz_zip_writer_finalize_archive(&zip_archive)) {
        ESP_LOGE(TAG, "Failed to finalize ZIP archive: %s", mz_zip_get_error_string(mz_zip_get_last_error(&zip_archive)));
        mz_zip_writer_end(&zip_archive);
        return ESP_FAIL;
    }

    mz_zip_writer_end(&zip_archive);
    ESP_LOGI(TAG, "ZIP archive created successfully at: %s", zip_path);
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
    char buffer[4096];
    int len = 0;

    // Generate the top requests table
    const char* table_rows = generate_top_requests_table();

    // Generate the SVG bar graph
    const char* svg_bar_graph = generate_svg_bar_graph();

    // Create the final HTML page by embedding the table, SVG, and SD card browse button
    len += snprintf(buffer + len, sizeof(buffer) - len,
        html_page_template, table_rows ? table_rows : "", svg_bar_graph);
    strncat(buffer, 
        "<button onclick=\"location.href='/browse_sd'\">Browse SD Card</button>",
        sizeof(buffer) - strlen(buffer) - 1);

    // Send the response
    httpd_resp_send(req, buffer, len);
    return ESP_OK;
}

esp_err_t browse_sd_get_handler(httpd_req_t *req) {
    char *response = malloc(8192);
    if (!response) {
        ESP_LOGE(TAG, "Failed to allocate memory for SD card response.");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Creating ZIP archive of SD card files...");
    // Create the ZIP archive
    if (create_zip_archive(CONFIG_SD_MOUNT_POINT, ZIP_FILE_PATH) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    snprintf(response, 8192, 
        "<html><head><title>SD Card</title></head><body>"
        "<h1>SD Card Contents</h1><ul>"
        "<p>All files have been archived. <a href=\"/download?file=sd_files.zip\">Download Archive</a></p>"
        "</body></html>");

    DIR *dir = opendir(CONFIG_SD_MOUNT_POINT);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open SD card directory");
        httpd_resp_send(req, "Failed to open SD card directory", HTTPD_RESP_USE_STRLEN);
        free(response);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            snprintf(response + strlen(response), 8192 - strlen(response),
                    "<li><a href=\"/download?file=%s\">%s</a></li>", entry->d_name, entry->d_name);
        } else if (entry->d_type == DT_DIR) {
            snprintf(response + strlen(response), 8192 - strlen(response),
                    "<li><b>%s/</b></li>", entry->d_name);
        }
    }
    closedir(dir);

    strncat(response, "</ul></body></html>", 8192 - strlen(response) - 1);
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

    if (httpd_start(&server_handle, &config) == ESP_OK) {
        httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler };
        httpd_uri_t browse_sd_uri = { .uri = "/browse_sd", .method = HTTP_GET, .handler = browse_sd_get_handler };
        httpd_uri_t download_uri = { .uri = "/download", .method = HTTP_GET, .handler = download_file_handler};
        httpd_uri_t favicon_uri = { .uri = "/favicon.ico",.method = HTTP_GET,.handler = favicon_handler,};

        httpd_register_uri_handler(server_handle, &root_uri);
        httpd_register_uri_handler(server_handle, &browse_sd_uri);
        httpd_register_uri_handler(server_handle, &download_uri);
        httpd_register_uri_handler(server_handle, &favicon_uri);

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