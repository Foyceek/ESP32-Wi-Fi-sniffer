/******************************************************************************
SparkFunBQ27441.cpp
BQ27441 Arduino Library Main Source File
Jim Lindblom @ SparkFun Electronics
May 9, 2016
https://github.com/sparkfun/SparkFun_BQ27441_Arduino_Library
Implementation of all features of the BQ27441 LiPo Fuel Gauge.
Hardware Resources:
- Arduino Development Board
- SparkFun Battery Babysitter
Development environment specifics:
Arduino 1.6.7
SparkFun Battery Babysitter v1.0
Arduino Uno (any 'duino should do)
******************************************************************************/

#include "bq27441.h"
#include "bq27441_constants.h"
#include "esp_log.h"

static const char* TAG = "BQ27441";

i2c_port_t _port_num;
bool _sealFlag; // Global to identify that IC was previously sealed
bool _userConfigControl; // Global to identify that user has control over

// Arduino constrain macro
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

// We may actually want to wait one tick, rather than the 0 this is gonna
// resolve to.  Making a macro so that it's easy to make that change
#define vTaskPoll() vTaskDelay(1 / portTICK_PERIOD_MS);

// Initializes I2C and verifies communication with the BQ27441.
bool bq27441Begin(i2c_port_t port_num)
{
    _port_num = port_num;
    uint16_t deviceID = 0;
    deviceID = bq27441DeviceType(); // Read bq27441DeviceType from BQ27441
    if (deviceID == BQ27441_DEVICE_ID)
    {
        ESP_LOGI(TAG,"BQ27441 Detected - Device ID: 0x%x",deviceID);
        return true; // If device ID is valid, return true
    }
    ESP_LOGE(TAG,"BQ27441 Not Found - Device ID: 0x%x",deviceID);
    return false; // Otherwise return false
}

// Configures the design capacity of the connected battery.
// [2 Section 6.4.2.3.6]
bool bq27441SetCapacity(uint16_t capacity)
{
    // Write to STATE subclass (82) of BQ27441 extended memory.
    // Offset 0x0A (10)
    // Design capacity is a 2-byte piece of data - MSB first
    // Unit: mAh
    uint8_t capMSB = capacity >> 8;
    uint8_t capLSB = capacity & 0x00FF;
    uint8_t capacityData[2] = {capMSB, capLSB};
    return bq27441WriteExtendedData(BQ27441_ID_STATE, 10, capacityData, 2);
}

// Configures the design energy of the connected battery.
// [2 Section 6.4.2.3.6]
bool bq27441SetDesignEnergy(uint16_t energy)
{
    // Write to STATE subclass (82) of BQ27441 extended memory.
    // Offset 0x0C (12)
    // Design energy is a 2-byte piece of data - MSB first
    // Unit: mWh
    uint8_t enMSB = energy >> 8;
    uint8_t enLSB = energy & 0x00FF;
    uint8_t energyData[2] = {enMSB, enLSB};
    return bq27441WriteExtendedData(BQ27441_ID_STATE, 12, energyData, 2);
}

// Configures the terminate voltage.
bool bq27441SetTerminateVoltage(uint16_t voltage)
{
    // Write to STATE subclass (82) of BQ27441 extended memory.
    // Offset 0x0F (16)
    // Termiante voltage is a 2-byte piece of data - MSB first
    // Unit: mV
    // Min 2500, Max 3700
    if(voltage<2500) voltage=2500;
    if(voltage>3700) voltage=3700;

    uint8_t tvMSB = voltage >> 8;
    uint8_t tvLSB = voltage & 0x00FF;
    uint8_t tvData[2] = {tvMSB, tvLSB};
    return bq27441WriteExtendedData(BQ27441_ID_STATE, 16, tvData, 2);
}

// Configures taper rate of connected battery.
bool bq27441SetTaperRate(uint16_t rate)
{
    // Write to STATE subclass (82) of BQ27441 extended memory.
    // Offset 0x1B (27)
    // Termiante voltage is a 2-byte piece of data - MSB first
    // Unit: 0.1h
    // Max 2000
    if(rate>2000) rate=2000;
    uint8_t trMSB = rate >> 8;
    uint8_t trLSB = rate & 0x00FF;
    uint8_t trData[2] = {trMSB, trLSB};
    return bq27441WriteExtendedData(BQ27441_ID_STATE, 27, trData, 2);
}

/*****************************************************************************
 ********************** Battery Characteristics Functions ********************
 *****************************************************************************/

// Reads and returns the battery voltage
uint16_t bq27441Voltage(void)
{
    return bq27441ReadWord(BQ27441_COMMAND_VOLTAGE);
}

// Reads and returns the specified current measurement
int16_t bq27441Current(current_measure type)
{
    int16_t current = 0;
    switch (type)
    {
    case AVG:
        current = (int16_t) bq27441ReadWord(BQ27441_COMMAND_AVG_CURRENT);
        break;
    case STBY:
        current = (int16_t) bq27441ReadWord(BQ27441_COMMAND_STDBY_CURRENT);
        break;
    case MAX:
        current = (int16_t) bq27441ReadWord(BQ27441_COMMAND_MAX_CURRENT);
        break;
    }

    return current;
}

// Reads and returns the specified capacity measurement
uint16_t bq27441Capacity(capacity_measure type)
{
    uint16_t capacity = 0;
    switch (type)
    {
    case REMAIN:
        return bq27441ReadWord(BQ27441_COMMAND_REM_CAPACITY);
        break;
    case FULL:
        return bq27441ReadWord(BQ27441_COMMAND_FULL_CAPACITY);
        break;
    case AVAIL:
        capacity = bq27441ReadWord(BQ27441_COMMAND_NOM_CAPACITY);
        break;
    case AVAIL_FULL:
        capacity = bq27441ReadWord(BQ27441_COMMAND_AVAIL_CAPACITY);
        break;
    case REMAIN_F:
        capacity = bq27441ReadWord(BQ27441_COMMAND_REM_CAP_FIL);
        break;
    case REMAIN_UF:
        capacity = bq27441ReadWord(BQ27441_COMMAND_REM_CAP_UNFL);
        break;
    case FULL_F:
        capacity = bq27441ReadWord(BQ27441_COMMAND_FULL_CAP_FIL);
        break;
    case FULL_UF:
        capacity = bq27441ReadWord(BQ27441_COMMAND_FULL_CAP_UNFL);
        break;
    case DESIGN:
        capacity = bq27441ReadWord(BQ27441_EXTENDED_CAPACITY);
    }

    return capacity;
}

// Reads and returns measured average power
int16_t bq27441Power(void)
{
    return (int16_t) bq27441ReadWord(BQ27441_COMMAND_AVG_POWER);
}

// Reads and returns specified state of charge measurement
uint16_t bq27441Soc(soc_measure type)
{
    uint16_t socRet = 0;
    switch (type)
    {
    case FILTERED:
        socRet = bq27441ReadWord(BQ27441_COMMAND_SOC);
        break;
    case UNFILTERED:
        socRet = bq27441ReadWord(BQ27441_COMMAND_SOC_UNFL);
        break;
    }

    return socRet;
}

// Reads and returns specified state of health measurement
uint8_t bq27441Soh(soh_measure type)
{
    uint16_t sohRaw = bq27441ReadWord(BQ27441_COMMAND_SOH);
    uint8_t sohStatus = sohRaw >> 8;
    uint8_t sohPercent = sohRaw & 0x00FF;

    if (type == PERCENT)
        return sohPercent;
    else
        return sohStatus;
}

// Reads and returns specified temperature measurement
uint16_t bq27441Temperature(temp_measure type)
{
    uint16_t temp = 0;
    switch (type)
    {
    case BATTERY:
        temp = bq27441ReadWord(BQ27441_COMMAND_TEMP);
        break;
    case INTERNAL_TEMP:
        temp = bq27441ReadWord(BQ27441_COMMAND_INT_TEMP);
        break;
    }
    return temp;
}

/*****************************************************************************
 ************************** GPOUT Control Functions **************************
 *****************************************************************************/
// Get GPOUT polarity setting (active-high or active-low)
bool bq27441GPOUTPolarity(void)
{
    uint16_t opConfigRegister = bq27441OpConfig();

    return (opConfigRegister & BQ27441_OPCONFIG_GPIOPOL);
}

// Set GPOUT polarity to active-high or active-low
bool bq27441SetGPOUTPolarity(bool activeHigh)
{
    uint16_t oldOpConfig = bq27441OpConfig();

    // Check to see if we need to update opConfig:
    if ((activeHigh && (oldOpConfig & BQ27441_OPCONFIG_GPIOPOL)) ||
            (!activeHigh && !(oldOpConfig & BQ27441_OPCONFIG_GPIOPOL)))
        return true;

    uint16_t newOpConfig = oldOpConfig;
    if (activeHigh)
        newOpConfig |= BQ27441_OPCONFIG_GPIOPOL;
    else
        newOpConfig &= ~(BQ27441_OPCONFIG_GPIOPOL);

    return bq27441WriteOpConfig(newOpConfig);
}

// Get GPOUT function (BAT_LOW or SOC_INT)
bool bq27441GPOUTFunction(void)
{
    uint16_t opConfigRegister = bq27441OpConfig();

    return (opConfigRegister & BQ27441_OPCONFIG_BATLOWEN);
}

// Set GPOUT function to BAT_LOW or SOC_INT
bool bq27441SetGPOUTFunction(gpout_function function)
{
    uint16_t oldOpConfig = bq27441OpConfig();

    // Check to see if we need to update opConfig:
    if ((function && (oldOpConfig & BQ27441_OPCONFIG_BATLOWEN)) ||
            (!function && !(oldOpConfig & BQ27441_OPCONFIG_BATLOWEN)))
        return true;

    // Modify BATLOWN_EN bit of opConfig:
    uint16_t newOpConfig = oldOpConfig;
    if (function)
        newOpConfig |= BQ27441_OPCONFIG_BATLOWEN;
    else
        newOpConfig &= ~(BQ27441_OPCONFIG_BATLOWEN);

    // Write new opConfig
    return bq27441WriteOpConfig(newOpConfig);
}

// Get SOC1_Set Threshold - threshold to set the alert flag
uint8_t bq27441SOC1SetThreshold(void)
{
    return bq27441ReadExtendedData(BQ27441_ID_DISCHARGE, 0);
}

// Get SOC1_Clear Threshold - threshold to clear the alert flag
uint8_t bq27441SOC1ClearThreshold(void)
{
    return bq27441ReadExtendedData(BQ27441_ID_DISCHARGE, 1);
}

// Set the SOC1 set and clear thresholds to a percentage
bool bq27441SetSOC1Thresholds(uint8_t set, uint8_t clear)
{
    uint8_t thresholds[2];
    thresholds[0] = constrain(set, 0, 100);
    thresholds[1] = constrain(clear, 0, 100);
    return bq27441WriteExtendedData(BQ27441_ID_DISCHARGE, 0, thresholds, 2);
}

// Get SOCF_Set Threshold - threshold to set the alert flag
uint8_t bq27441SOCFSetThreshold(void)
{
    return bq27441ReadExtendedData(BQ27441_ID_DISCHARGE, 2);
}

// Get SOCF_Clear Threshold - threshold to clear the alert flag
uint8_t bq27441SOCFClearThreshold(void)
{
    return bq27441ReadExtendedData(BQ27441_ID_DISCHARGE, 3);
}

// Set the SOCF set and clear thresholds to a percentage
bool bq27441SetSOCFThresholds(uint8_t set, uint8_t clear)
{
    uint8_t thresholds[2];
    thresholds[0] = constrain(set, 0, 100);
    thresholds[1] = constrain(clear, 0, 100);
    return bq27441WriteExtendedData(BQ27441_ID_DISCHARGE, 2, thresholds, 2);
}

// Check if the SOC1 flag is set
bool bq27441SocFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_SOC1;
}

// Check if the SOCF flag is set
bool bq27441SocfFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_SOCF;

}

// Check if the ITPOR flag is set
bool bq27441ItporFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_ITPOR;
}

// Check if the FC flag is set
bool bq27441FcFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_FC;
}

// Check if the CHG flag is set
bool bq27441ChgFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_CHG;
}

// Check if the DSG flag is set
bool bq27441DsgFlag(void)
{
    uint16_t flagState = bq27441Flags();

    return flagState & BQ27441_FLAG_DSG;
}

// Get the SOC_INT interval delta
uint8_t bq27441SociDelta(void)
{
    return bq27441ReadExtendedData(BQ27441_ID_STATE, 26);
}

// Set the SOC_INT interval delta to a value between 1 and 100
bool bq27441SetSOCIDelta(uint8_t delta)
{
    uint8_t soci = constrain(delta, 0, 100);
    return bq27441WriteExtendedData(BQ27441_ID_STATE, 26, &soci, 1);
}

// Pulse the GPOUT pin - must be in SOC_INT mode
bool bq27441PulseGPOUT(void)
{
    return bq27441ExecuteControlWord(BQ27441_CONTROL_PULSE_SOC_INT);
}

/*****************************************************************************
 *************************** Control Sub-Commands ****************************
 *****************************************************************************/

// Read the device type - should be 0x0421
uint16_t bq27441DeviceType(void)
{
    return bq27441ReadControlWord(BQ27441_CONTROL_DEVICE_TYPE);
}

// Enter configuration mode - set userControl if calling from an Arduino sketch
// and you want control over when to bq27441exitConfig
bool bq27441EnterConfig(bool userControl)
{
    if (userControl) _userConfigControl = true;

    if (bq27441Sealed())
    {
        _sealFlag = true;
        bq27441Unseal(); // Must be unsealed before making changes
    }

    if (bq27441ExecuteControlWord(BQ27441_CONTROL_SET_CFGUPDATE))
    {
        int16_t timeout = BQ72441_I2C_TIMEOUT;
        while ((timeout--) && (!(bq27441Flags() & BQ27441_FLAG_CFGUPMODE)))
            vTaskPoll();

        if (timeout > 0)
            return true;
    }

    return false;
}

// Exit configuration mode with the option to perform a resimulation
bool bq27441ExitConfig(bool resim)
{
    // There are two methods for exiting config mode:
    //    1. Execute the EXIT_CFGUPDATE command
    //    2. Execute the SOFT_RESET command
    // EXIT_CFGUPDATE exits config mode _without_ an OCV (open-circuit voltage)
    // measurement, and without resimulating to update unfiltered-SoC and SoC.
    // If a new OCV measurement or resimulation is desired, SOFT_RESET or
    // EXIT_RESIM should be used to exit config mode.
    if (resim)
    {
        if (bq27441SoftReset())
        {
            int16_t timeout = BQ72441_I2C_TIMEOUT;
            while ((timeout--) && ((bq27441Flags() & BQ27441_FLAG_CFGUPMODE)))
                vTaskPoll();

            if (timeout > 0)
            {
                if (_sealFlag) bq27441Seal(); // Seal back up if we IC was sealed coming in
                return true;
            }
        }
        return false;
    }
    else
    {
        return bq27441ExecuteControlWord(BQ27441_CONTROL_EXIT_CFGUPDATE);
    }
}

// Read the bq27441Flags() command
uint16_t bq27441Flags(void)
{
    return bq27441ReadWord(BQ27441_COMMAND_FLAGS);
}

// Read the CONTROL_STATUS subcommand of control()
uint16_t bq27441Status(void)
{
    return bq27441ReadControlWord(BQ27441_CONTROL_STATUS);
}

/***************************** Private Functions *****************************/

// Check if the BQ27441-G1A is sealed or not.
bool bq27441Sealed(void)
{
    uint16_t stat = bq27441Status();
    return stat & BQ27441_STATUS_SS;
}

// Seal the BQ27441-G1A
bool bq27441Seal(void)
{
    return bq27441ReadControlWord(BQ27441_CONTROL_SEALED);
}

// UNseal the BQ27441-G1A
bool bq27441Unseal(void)
{
    // To unseal the BQ27441, write the key to the control
    // command. Then immediately write the same key to control again.
    if (bq27441ReadControlWord(BQ27441_UNSEAL_KEY))
    {
        return bq27441ReadControlWord(BQ27441_UNSEAL_KEY);
    }
    return false;
}

// Read the 16-bit opConfig register from extended data
uint16_t bq27441OpConfig(void)
{
    return bq27441ReadWord(BQ27441_EXTENDED_OPCONFIG);
}

// Write the 16-bit opConfig register in extended data
bool bq27441WriteOpConfig(uint16_t value)
{
    uint8_t opConfigMSB = value >> 8;
    uint8_t opConfigLSB = value & 0x00FF;
    uint8_t opConfigData[2] = {opConfigMSB, opConfigLSB};

    // OpConfig register location: BQ27441_ID_REGISTERS id, offset 0
    return bq27441WriteExtendedData(BQ27441_ID_REGISTERS, 0, opConfigData, 2);
}

// Issue a soft-reset to the BQ27441-G1A
bool bq27441SoftReset(void)
{
    return bq27441ExecuteControlWord(BQ27441_CONTROL_SOFT_RESET);
}

// Read a 16-bit command word from the BQ27441-G1A
uint16_t bq27441ReadWord(uint16_t subAddress)
{
    uint8_t data[2];
    bq27441I2cReadBytes(subAddress, data, 2);
    return ((uint16_t) data[1] << 8) | data[0];
}

// Read a 16-bit subcommand() from the BQ27441-G1A's control()
uint16_t bq27441ReadControlWord(uint16_t function)
{
    uint8_t subCommandMSB = (function >> 8);
    uint8_t subCommandLSB = (function & 0x00FF);
    uint8_t command[2] = {subCommandLSB, subCommandMSB};
    uint8_t data[2] = {0, 0};

    bq27441I2cWriteBytes((uint8_t) 0, command, 2);

    if (bq27441I2cReadBytes((uint8_t) 0, data, 2))
    {
        return ((uint16_t)data[1] << 8) | data[0];
    }

    return false;
}

// Execute a subcommand() from the BQ27441-G1A's control()
bool bq27441ExecuteControlWord(uint16_t function)
{
    uint8_t subCommandMSB = (function >> 8);
    uint8_t subCommandLSB = (function & 0x00FF);
    uint8_t command[2] = {subCommandLSB, subCommandMSB};
    uint8_t data[2] = {0, 0};

    if (bq27441I2cWriteBytes((uint8_t) 0, command, 2))
        return true;

    return false;
}

/*****************************************************************************
 ************************** Extended Data Commands ***************************
 *****************************************************************************/

// Issue a BlockDataControl() command to enable BlockData access
bool bq27441BlockDataControl(void)
{
    uint8_t enableByte = 0x00;
    return bq27441I2cWriteBytes(BQ27441_EXTENDED_CONTROL, &enableByte, 1);
}

// Issue a DataClass() command to set the data class to be accessed
bool bq27441BlockDataClass(uint8_t id)
{
    return bq27441I2cWriteBytes(BQ27441_EXTENDED_DATACLASS, &id, 1);
}

// Issue a DataBlock() command to set the data block to be accessed
bool bq27441BlockDataOffset(uint8_t offset)
{
    return bq27441I2cWriteBytes(BQ27441_EXTENDED_DATABLOCK, &offset, 1);
}

// Read the current checksum using BlockDataCheckSum()
uint8_t bq27441BlockDataChecksum(void)
{
    uint8_t csum;
    bq27441I2cReadBytes(BQ27441_EXTENDED_CHECKSUM, &csum, 1);
    return csum;
}

// Use BlockData() to read a byte from the loaded extended data
uint8_t bq27441ReadBlockData(uint8_t offset)
{
    uint8_t ret;
    uint8_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    bq27441I2cReadBytes(address, &ret, 1);
    return ret;
}

// Use BlockData() to write a byte to an offset of the loaded data
bool bq27441WriteBlockData(uint8_t offset, uint8_t data)
{
    uint8_t address = offset + BQ27441_EXTENDED_BLOCKDATA;
    return bq27441I2cWriteBytes(address, &data, 1);
}

// Read all 32 bytes of the loaded extended data and compute a
// checksum based on the values.
uint8_t bq27441ComputeBlockChecksum(void)
{
    uint8_t data[32];
    bq27441I2cReadBytes(BQ27441_EXTENDED_BLOCKDATA, data, 32);

    uint8_t csum = 0;
    for (int i=0; i<32; i++)
    {
        csum += data[i];
    }
    csum = 255 - csum;

    return csum;
}

// Use the BlockDataCheckSum() command to write a checksum value
bool bq27441WriteBlockChecksum(uint8_t csum)
{
    return bq27441I2cWriteBytes(BQ27441_EXTENDED_CHECKSUM, &csum, 1);
}

// Read a byte from extended data specifying a class ID and position offset
uint8_t bq27441ReadExtendedData(uint8_t classID, uint8_t offset)
{
    uint8_t retData = 0;
    if (!_userConfigControl) bq27441EnterConfig(false);

    if (!bq27441BlockDataControl()) // // enable block data memory control
        return false; // Return false if enable fails
    if (!bq27441BlockDataClass(classID)) // Write class ID using DataBlockClass()
        return false;

    bq27441BlockDataOffset(offset / 32); // Write 32-bit block offset (usually 0)

    bq27441ComputeBlockChecksum(); // Compute checksum going in
    uint8_t oldCsum = bq27441BlockDataChecksum();
    /*for (int i=0; i<32; i++)
    	Serial.print(String(readBlockData(i)) + " ");*/
    retData = bq27441ReadBlockData(offset % 32); // Read from offset (limit to 0-31)

    if (!_userConfigControl) bq27441ExitConfig(false);

    return retData;
}

// Write a specified number of bytes to extended data specifying a
// class ID, position offset.
bool bq27441WriteExtendedData(uint8_t classID, uint8_t offset, uint8_t * data, uint8_t len)
{
    if (len > 32)
        return false;

    if (!_userConfigControl) bq27441EnterConfig(false);

    if (!bq27441BlockDataControl()) // // enable block data memory control
        return false; // Return false if enable fails
    if (!bq27441BlockDataClass(classID)) // Write class ID using DataBlockClass()
        return false;

    bq27441BlockDataOffset(offset / 32); // Write 32-bit block offset (usually 0)
    bq27441ComputeBlockChecksum(); // Compute checksum going in
    uint8_t oldCsum = bq27441BlockDataChecksum();

    // Write data bytes:
    for (int i = 0; i < len; i++)
    {
        // Write to offset, mod 32 if offset is greater than 32
        // The blockDataOffset above sets the 32-bit block
        bq27441WriteBlockData((offset % 32) + i, data[i]);
    }

    // Write new checksum using BlockDataChecksum (0x60)
    uint8_t newCsum = bq27441ComputeBlockChecksum(); // Compute the new checksum
    bq27441WriteBlockChecksum(newCsum);

    if (!_userConfigControl) bq27441ExitConfig(true);

    return true;
}

/*****************************************************************************
 ************************ I2C Read and Write Routines ************************
 *****************************************************************************/

// Read a specified number of bytes over I2C at a given subAddress
int16_t bq27441I2cReadBytes(uint8_t subAddress, uint8_t * dest, uint8_t count)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27441_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, subAddress, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27441_I2C_ADDRESS << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (count > 1)
    {
        i2c_master_read(cmd, dest, count - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, dest + count - 1, NACK_VAL);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(_port_num, cmd, 200 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if(ret == ESP_OK)
        return true;
    else
        return false;
}

// Write a specified number of bytes over I2C to a given subAddress
uint16_t bq27441I2cWriteBytes(uint8_t subAddress, uint8_t * src, uint8_t count)
{
    // We write data incrementally
    // According to the datasheet this is only ok for f_scl up to 100khz
    // 66us idle time is required between commands
    // this seems to be built in and there are no communication problems
    // if needed in future could use eta_delay_us to busy wait for 66us

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BQ27441_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, subAddress, ACK_CHECK_EN);
    for(int i = 0; i < count; i++)
    {
        i2c_master_write_byte(cmd, src[i], ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(_port_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return true;
}
