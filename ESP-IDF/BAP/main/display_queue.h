#ifndef DISPLAY_QUEUE_H
#define DISPLAY_QUEUE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Define message types
#define DISPLAY_MSG_CLEAR 1
#define DISPLAY_MSG_TEXT  2

// Message structure for display queue
typedef struct {
    int type;           // Message type (DISPLAY_MSG_*)
    char text[128];     // Text to display (for DISPLAY_MSG_TEXT)
} display_message_t;

void i2c_task_init(void);

void i2c_task_send_display_text(const char* text);

void i2c_task_send_display_clear(void);

void i2c_task_send_battery_status(void);

void oled_countdown_task(void *pvParameter);

#endif /* DISPLAY_QUEUE_H */