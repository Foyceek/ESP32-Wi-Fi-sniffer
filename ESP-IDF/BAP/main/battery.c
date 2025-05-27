#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_mac.h"
#include "config.h"

#include "bq27441.h"  // Make sure your C version has C-friendly headers

#define I2C_SCL_IO              PIN_SCL
#define I2C_SDA_IO              PIN_SDA
#define I2C_FREQ_HZ             50000
#define I2C_PORT_NUM            I2C_NUM_0
#define I2C_TX_BUF_DISABLE      0
#define I2C_RX_BUF_DISABLE      0
#define ACK_CHECK_EN            0x1
#define TIMEOUT_APB_TICKS       0x1f

// Battery Configuration Parameters for 3.7V 1000mAh LiPo
#define BATTERY_CAPACITY        1000    // Design capacity in mAh
#define BATTERY_DESIGN_ENERGY   3700    // Design energy in mWh (capacity * nominal voltage)
#define BATTERY_TERMINATION_V   3000    // Termination voltage in mV (safe cutoff)
#define BATTERY_TAPER_RATE      175     // Taper rate for end-of-charge detection (in 0.1h)
#define BATTERY_SOC_LOW_THRESH  15      // Low battery warning threshold (%)
#define BATTERY_SOC_CRIT_THRESH 5       // Critical battery threshold (%)

#define BQ27441_I2C_ADDRESS     0x55

static const char* TAG = "BATTERY";

uint16_t volts = 0;
int16_t current = 0;

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    esp_err_t res = i2c_param_config(I2C_PORT_NUM, &conf);
    if (res != ESP_OK) return res;
    return i2c_driver_install(I2C_PORT_NUM, conf.mode, I2C_RX_BUF_DISABLE, I2C_TX_BUF_DISABLE, 0);
}

void i2c_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    for (uint8_t addr = 1; addr < 127; addr++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at 0x%02X", addr);
        }
    }
}

bool configure_battery_params(void)
{
    ESP_LOGI(TAG, "Configuring BQ27441 for 3.7V 1000mAh LiPo battery");
    
    // Enter configuration mode
    if (!bq27441EnterConfig(true)) {
        ESP_LOGE(TAG, "Failed to enter config mode");
        return false;
    }
    
    // Set the battery capacity (in mAh)
    if (!bq27441SetCapacity(BATTERY_CAPACITY)) {
        ESP_LOGE(TAG, "Failed to set battery capacity");
        bq27441ExitConfig(false);
        return false;
    }
    
    // Set the design energy (in mWh)
    if (!bq27441SetDesignEnergy(BATTERY_DESIGN_ENERGY)) {
        ESP_LOGE(TAG, "Failed to set design energy");
        bq27441ExitConfig(false);
        return false;
    }
    
    // Set the termination voltage (in mV)
    if (!bq27441SetTerminateVoltage(BATTERY_TERMINATION_V)) {
        ESP_LOGE(TAG, "Failed to set termination voltage");
        bq27441ExitConfig(false);
        return false;
    }
    
    // Set the taper rate
    if (!bq27441SetTaperRate(BATTERY_TAPER_RATE)) {
        ESP_LOGE(TAG, "Failed to set taper rate");
        bq27441ExitConfig(false);
        return false;
    }
    
    // Set SOC thresholds for alerts
    if (!bq27441SetSOC1Thresholds(BATTERY_SOC_LOW_THRESH, BATTERY_SOC_LOW_THRESH + 1)) {
        ESP_LOGE(TAG, "Failed to set SOC1 thresholds");
        bq27441ExitConfig(false);
        return false;
    }
    
    if (!bq27441SetSOCFThresholds(BATTERY_SOC_CRIT_THRESH, BATTERY_SOC_CRIT_THRESH + 1)) {
        ESP_LOGE(TAG, "Failed to set SOCF thresholds");
        bq27441ExitConfig(false);
        return false;
    }
    
    // Exit configuration with resimulation
    if (!bq27441ExitConfig(true)) {
        ESP_LOGE(TAG, "Failed to exit config mode");
        return false;
    }
    
    ESP_LOGI(TAG, "Battery configuration complete");
    return true;
}

void print_battery_status(void)
{
    uint16_t soc = bq27441Soc(FILTERED);
    volts = bq27441Voltage();
    current = bq27441Current(AVG);
    uint16_t fullCapacity = bq27441Capacity(FULL);
    uint16_t remainCapacity = bq27441Capacity(REMAIN);
    int16_t power = bq27441Power();
    uint8_t health = bq27441Soh(PERCENT);
    uint16_t temperature = bq27441Temperature(BATTERY);
    
    // Temperature is reported in units of 0.1K, convert to Celsius
    float tempC = (temperature / 10.0) - 273.15;
    #if PRINT_BATTERY_STATUS
    ESP_LOGI(TAG, "--------- Battery Status ---------");
    ESP_LOGI(TAG, "State of Charge: %u%%", soc);
    ESP_LOGI(TAG, "Voltage: %u mV", volts);
    ESP_LOGI(TAG, "Current: %d mA", current);
    ESP_LOGI(TAG, "Capacity: %u / %u mAh", remainCapacity, fullCapacity);
    ESP_LOGI(TAG, "Power: %d mW", power);
    ESP_LOGI(TAG, "Health: %d%%", health);
    ESP_LOGI(TAG, "Temperature: %.1f°C", tempC);
    // Check battery alert flags
    if (bq27441SocFlag()) {
        ESP_LOGW(TAG, "WARNING: Battery low (%d%%)", soc);
    }
    if (bq27441SocfFlag()) {
        ESP_LOGW(TAG, "WARNING: Battery critically low (%d%%)", soc);
    }
    if (bq27441FcFlag()) {
        ESP_LOGI(TAG, "Battery fully charged");
    }
    
    ESP_LOGI(TAG, "---------------------------------");
    #endif
}

void battery_init(void)
{
    ESP_LOGI(TAG, "Initializing battery monitoring system");
    
    // Initialize BQ27441
    if (!bq27441Begin(I2C_PORT_NUM)) {
        ESP_LOGE(TAG, "BQ27441 not detected. Check wiring!");
        return;
    }
    
    ESP_LOGI(TAG, "BQ27441 detected!");
    
    // Configure BQ27441 for our specific battery
    if (!configure_battery_params()) {
        ESP_LOGE(TAG, "Failed to configure battery parameters");
        return;
    }
    
        print_battery_status();
}