#ifndef SERVER_H
#define SERVER_H

#include "esp_err.h"

// Function to start the web server
esp_err_t start_captive_server(void);
esp_err_t stop_captive_server(void);
esp_err_t load_settings_from_sd(const char *path);

#endif // SERVER_H
