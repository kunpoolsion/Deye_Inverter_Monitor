#include "DeyeInverter.h"

DeyeInverter::DeyeInverter(SolarmanV5 *solarman) {
    _solarman = solarman;
}

bool DeyeInverter::isRegisterSigned(uint16_t register_addr) {
    // Lista de registros que contienen valores signed (2's complement)
    switch(register_addr) {
        case 0x00BE: // Battery Power
        case 0x00BF: // Battery Current
        case 0x00A9: // Grid Power
        case 0x00AF: // Total Power
        case 0x00AD: // Inverter L1 Power
        case 0x00AE: // Inverter L2 Power
            return true;
        default:
            return false;
    }
}

String DeyeInverter::getBatteryStatus(uint16_t status) {
    switch(status) {
        case 0: return "Charge";
        case 1: return "Stand-by";
        case 2: return "Discharge";
        default: return "Unknown";
    }
}

String DeyeInverter::getRunningStatus(uint16_t status) {
    switch(status) {
        case 0: return "Stand-by";
        case 1: return "Self-checking";
        case 2: return "Normal";
        case 3: return "FAULT";
        default: return "Unknown";
    }
}

String DeyeInverter::getWorkMode(uint16_t mode) {
    switch(mode) {
        case 0: return "Selling First";
        case 1: return "Zero-Export to Load&Solar Sell";
        case 2: return "Zero-Export to Home&Solar Sell";
        case 3: return "Zero-Export to Load";
        case 4: return "Zero-Export to Home";
        default: return "Unknown";
    }
}

float DeyeInverter::applyScaleAndOffset(uint16_t value, float scale, int16_t offset, bool is_signed) {
    if (is_signed) {
        int16_t signed_value = (int16_t)value;
        return (signed_value * scale) + offset;
    } else {
        return (value * scale) + offset;
    }
}

bool DeyeInverter::readAllData(InverterData *data) {
    data->timestamp = millis();
    data->data_valid = true;
    
    // Leer datos en grupos
    if (!readSolarData(data)) data->data_valid = false;
    if (!readBatteryData(data)) data->data_valid = false;
    if (!readGridData(data)) data->data_valid = false;
    if (!readLoadData(data)) data->data_valid = false;
    if (!readInverterData(data)) data->data_valid = false;
    
    return data->data_valid;
}

bool DeyeInverter::readSolarData(InverterData *data) {
    uint16_t value;
    bool is_signed;
    
    // PV1 Voltage (0x006D)
    if (_solarman->readRegister(0x006D, &value)) {
        data->pv1_voltage = value * 0.1;
    } else return false;
    
    // PV1 Current (0x006E)
    if (_solarman->readRegister(0x006E, &value)) {
        data->pv1_current = value * 0.1;
    } else return false;
    
    // PV2 Voltage (0x006F)
    if (_solarman->readRegister(0x006F, &value)) {
        data->pv2_voltage = value * 0.1;
    } else return false;
    
    // PV2 Current (0x0070)
    if (_solarman->readRegister(0x0070, &value)) {
        data->pv2_current = value * 0.1;
    } else return false;
    
    // PV1 Power (0x00BA)
    if (_solarman->readRegister(0x00BA, &value)) {
        data->pv1_power = value;
    } else return false;
    
    // PV2 Power (0x00BB)
    if (_solarman->readRegister(0x00BB, &value)) {
        data->pv2_power = value;
    } else return false;
    
    // Daily Production (0x006C)
    if (_solarman->readRegister(0x006C, &value)) {
        data->daily_production = value * 0.1;
    } else return false;
    
    return true;
}

bool DeyeInverter::readBatteryData(InverterData *data) {
    uint16_t value;
    bool is_signed;
    
    // Battery Voltage (0x00B7)
    if (_solarman->readRegister(0x00B7, &value)) {
        data->battery_voltage = value * 0.01;
    } else return false;
    
    // Battery SOC (0x00B8)
    if (_solarman->readRegister(0x00B8, &value)) {
        data->battery_soc = value;
    } else return false;
    
    // Battery Power (0x00BE) - SIGNED
    if (_solarman->readRegister(0x00BE, &value, &is_signed)) {
        data->battery_power = applyScaleAndOffset(value, 1.0, 0, true);
    } else return false;
    
    // Battery Current (0x00BF) - SIGNED
    if (_solarman->readRegister(0x00BF, &value, &is_signed)) {
        data->battery_current = applyScaleAndOffset(value, 0.01, 0, true);
    } else return false;
    
    // Battery Status (0x00BD)
    if (_solarman->readRegister(0x00BD, &value)) {
        data->battery_status = getBatteryStatus(value);
    } else return false;
    
    // Battery Temperature (0x00B6)
    if (_solarman->readRegister(0x00B6, &value)) {
        data->battery_temperature = (value * 0.1) - 100.0;
    } else return false;
    
    return true;
}

bool DeyeInverter::readGridData(InverterData *data) {
    uint16_t value;
    bool is_signed;
    
    // Grid Power (0x00A9) - SIGNED
    if (_solarman->readRegister(0x00A9, &value, &is_signed)) {
        data->grid_power = applyScaleAndOffset(value, 1.0, 0, true);
    } else return false;
    
    // Grid Voltage L1 (0x0096)
    if (_solarman->readRegister(0x0096, &value)) {
        data->grid_voltage_l1 = value * 0.1;
    } else return false;
    
    // Grid Current L1 (0x00A0)
    if (_solarman->readRegister(0x00A0, &value)) {
        data->grid_current_l1 = value * 0.01;
    } else return false;
    
    // Grid Frequency (0x004F)
    if (_solarman->readRegister(0x004F, &value)) {
        data->grid_frequency = value * 0.01;
    } else return false;
    
    // Daily Energy Bought (0x004C)
    if (_solarman->readRegister(0x004C, &value)) {
        data->daily_energy_bought = value * 0.1;
    } else return false;
    
    // Daily Energy Sold (0x004D)
    if (_solarman->readRegister(0x004D, &value)) {
        data->daily_energy_sold = value * 0.1;
    } else return false;
    
    return true;
}

bool DeyeInverter::readLoadData(InverterData *data) {
    uint16_t value;
    
    // Load Power (0x00B2)
    if (_solarman->readRegister(0x00B2, &value)) {
        data->load_power = value;
    } else return false;
    
    // Load L1 Power (0x00B0)
    if (_solarman->readRegister(0x00B0, &value)) {
        data->load_l1_power = value;
    } else return false;
    
    // Daily Load Consumption (0x0054)
    if (_solarman->readRegister(0x0054, &value)) {
        data->daily_load_consumption = value * 0.1;
    } else return false;
    
    return true;
}

bool DeyeInverter::readInverterData(InverterData *data) {
    uint16_t value;
    bool is_signed;
    
    // Running Status (0x003B)
    if (_solarman->readRegister(0x003B, &value)) {
        data->running_status = getRunningStatus(value);
    } else return false;
    
    // Work Mode (0x00F4)
    if (_solarman->readRegister(0x00F4, &value)) {
        data->work_mode = getWorkMode(value);
    } else return false;
    
    // Inverter Temperature (0x005A) - DC Temperature
    if (_solarman->readRegister(0x005A, &value)) {
        data->inverter_temperature = (value * 0.1) - 100.0;
    } else return false;
    
    return true;
}