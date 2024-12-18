#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define TOP_REQUESTS_COUNT 10

extern int request_index;
extern int max_request_rank;

extern bool oled_initialized;
extern int delay_state;

typedef struct {
    int rssi;
    char mac_address[18];
    time_t timestamp;
} top_request_t;

extern top_request_t top_requests[TOP_REQUESTS_COUNT];

extern int rssi_ranges[10];

#endif
