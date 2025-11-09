#ifndef DEYEINVERTER_H
#define DEYEINVERTER_H

#include "SolarmanV5.h"
#include <Arduino.h>

struct InverterData {
    // Timestamp
    unsigned long timestamp;
    
    // Solar
    float pv1_voltage;
    float pv1_current;
    float pv1_power;
    float pv2_voltage;
    float pv2_current;
    float pv2_power;
    float daily_production;
    float total_production;
    
    // Battery
    float battery_voltage;
    float battery_current;
    float battery_power;
    float battery_soc;
    float battery_temperature;
    String battery_status;
    
    // Grid
    float grid_voltage_l1;
    float grid_current_l1;
    float grid_power;
    float grid_frequency;
    float daily_energy_bought;
    float daily_energy_sold;
    
    // Load
    float load_power;
    float load_l1_power;
    float daily_load_consumption;
    
    // Inverter
    String running_status;
    String work_mode;
    float inverter_temperature;
    
    // Flags
    bool data_valid;
};

class DeyeInverter {
private:
    SolarmanV5 *_solarman;
    
    // Registros signed (según YAML)
    bool isRegisterSigned(uint16_t register_addr);
    String getBatteryStatus(uint16_t status);
    String getRunningStatus(uint16_t status);
    String getWorkMode(uint16_t mode);
    
public:
    DeyeInverter(SolarmanV5 *solarman);
    
    bool readAllData(InverterData *data);
    bool readSolarData(InverterData *data);
    bool readBatteryData(InverterData *data);
    bool readGridData(InverterData *data);
    bool readLoadData(InverterData *data);
    bool readInverterData(InverterData *data);
    
    // Métodos de utilidad
    static float applyScaleAndOffset(uint16_t value, float scale, int16_t offset = 0, bool is_signed = false);
};

#endif