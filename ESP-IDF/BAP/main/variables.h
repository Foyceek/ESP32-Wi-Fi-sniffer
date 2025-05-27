#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define TOP_REQUESTS_COUNT 10

extern int request_index;
extern int max_request_rank;

extern int short_oled_period;
extern int medium_oled_period;
extern int long_oled_period;

extern bool start_server;

extern bool oled_initialized;
extern bool powering_down;

extern int delay_state;
extern bool cancel_powerdown;

extern bool initial_selection;

extern bool time_selection_active;
extern bool use_wifi_time;

// extern bool setup_selection_active;
// extern bool use_default_setup;

extern bool server_setup_selection_active;
extern bool use_server_setup;

extern bool settings_selection_active;
extern bool load_json;

extern bool flip_oled;
extern bool display_battery_data;

extern char wifi_ssid[64];
extern char wifi_password[64];

extern char server_wifi_ssid[64];
extern char server_wifi_password[64];

typedef struct {
    int rssi;
    char mac_address[18];
    time_t timestamp;
} top_request_t;

extern top_request_t top_requests[TOP_REQUESTS_COUNT];

extern int rssi_ranges[10];

#endif
