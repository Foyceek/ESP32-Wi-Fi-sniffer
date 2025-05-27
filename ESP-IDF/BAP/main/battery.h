#ifndef BATTERY_H
#define BATTERY_H

#include "esp_err.h"
#include <stdbool.h>

// Battery Configuration Constants
#define BATTERY_CAPACITY        1000    // mAh
#define BATTERY_DESIGN_ENERGY   3700    // mWh
#define BATTERY_TERMINATION_V   3000    // mV
#define BATTERY_TAPER_RATE      175     // in 0.1h
#define BATTERY_SOC_LOW_THRESH  15      // %
#define BATTERY_SOC_CRIT_THRESH 5       // %

#define BQ27441_I2C_ADDRESS     0x55

/**
 * @brief Initialize the battery monitoring system, including I2C and BQ27441.
 */
void battery_init(void);

/**
 * @brief Print current battery status including voltage, SoC, current, etc.
 */
void print_battery_status(void);

bool configure_battery_params(void);
void i2c_scan(void);

// Declare the global variables that will be accessible from other modules
extern uint16_t volts;  // Battery voltage in mV
extern int16_t current; // Battery current in mA

#endif // BATTERY_H