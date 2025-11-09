#include "SolarmanV5.h"
#include <WiFi.h>

SolarmanV5::SolarmanV5(const char* datalogger_ip, uint32_t datalogger_sn, uint8_t mb_slave_id, uint16_t datalogger_port) {
    _datalogger_ip = datalogger_ip;
    _datalogger_sn = datalogger_sn;
    _mb_slave_id = mb_slave_id;
    _datalogger_port = datalogger_port;
    _sequence_number = 0x45;
}

void SolarmanV5::begin() {
    // Inicialización si es necesaria
    // Podemos agregar verificación de conexión aquí en el futuro
}

uint16_t SolarmanV5::calculateCRC(uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

uint8_t SolarmanV5::calculateV5Checksum(uint8_t *data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 1; i < length - 2; i++) {
        checksum += data[i];
    }
    return checksum;
}

size_t SolarmanV5::buildV5Frame(uint8_t *v5_frame, uint16_t start_addr, uint16_t reg_count) {
    uint8_t modbus_data[] = {
        _mb_slave_id,
        0x03, // Function: Read Holding Registers
        (uint8_t)((start_addr >> 8) & 0xFF),
        (uint8_t)(start_addr & 0xFF),
        (uint8_t)((reg_count >> 8) & 0xFF),
        (uint8_t)(reg_count & 0xFF)
    };
    
    uint16_t crc_modbus = calculateCRC(modbus_data, sizeof(modbus_data));
    
    int pos = 0;
    v5_frame[pos++] = 0xA5; // Start
    v5_frame[pos++] = 0x17; // Length low
    v5_frame[pos++] = 0x00; // Length high
    v5_frame[pos++] = 0x10; // Control code low
    v5_frame[pos++] = 0x45; // Control code high
    v5_frame[pos++] = _sequence_number++;
    v5_frame[pos++] = 0x00;
    v5_frame[pos++] = _datalogger_sn & 0xFF;
    v5_frame[pos++] = (_datalogger_sn >> 8) & 0xFF;
    v5_frame[pos++] = (_datalogger_sn >> 16) & 0xFF;
    v5_frame[pos++] = (_datalogger_sn >> 24) & 0xFF;
    
    // Payload
    v5_frame[pos++] = 0x02; // Frame Type: Solar Inverter
    for(int i = 0; i < 14; i++) {
        v5_frame[pos++] = 0x00;
    }
    
    memcpy(&v5_frame[pos], modbus_data, sizeof(modbus_data));
    pos += sizeof(modbus_data);
    v5_frame[pos++] = crc_modbus & 0xFF;
    v5_frame[pos++] = (crc_modbus >> 8) & 0xFF;
    
    // Calcular checksum V5
    uint8_t v5_checksum = calculateV5Checksum(v5_frame, pos + 2);
    v5_frame[pos++] = v5_checksum;
    v5_frame[pos++] = 0x15; // End
    
    return pos;
}

bool SolarmanV5::sendReceive(uint8_t *request_frame, size_t frame_len, uint8_t *response, size_t *response_len) {
    WiFiClient client;
    
    if (!client.connect(_datalogger_ip, _datalogger_port, 10000)) {
        return false;
    }
    
    client.write(request_frame, frame_len);
    client.flush();
    
    unsigned long start_time = millis();
    while (client.connected() && !client.available()) {
        if (millis() - start_time > 5000) {
            client.stop();
            return false;
        }
        delay(10);
    }
    
    *response_len = 0;
    start_time = millis();
    
    while (client.connected() || client.available()) {
        if (client.available()) {
            response[(*response_len)++] = client.read();
            start_time = millis();
            
            if (*response_len >= 2 && response[*response_len-1] == 0x15) {
                delay(50);
                if (!client.available()) break;
            }
            if (*response_len >= 300) break;
        }
        if (millis() - start_time > 1000) break;
    }
    
    client.stop();
    
    return (*response_len > 0);
}

bool SolarmanV5::parseResponse(uint8_t *response, size_t len, uint16_t *value, bool *is_signed) {
    if (len < 15) {
        return false;
    }
    
    // Buscar la trama Modbus en la respuesta
    for (int i = len - 8; i >= 5; i--) {
        if (response[i] == _mb_slave_id && response[i+1] == 0x03) {
            uint8_t data_bytes = response[i+2];
            if (data_bytes == 0x02 && i + 5 < len) {
                *value = (response[i+3] << 8) | response[i+4];
                
                // Verificar CRC Modbus
                uint16_t received_crc = (response[i+6] << 8) | response[i+5];
                uint16_t calculated_crc = calculateCRC(&response[i], 5);
                
                if (received_crc == calculated_crc) {
                    // Detectar si el valor es signed
                    if (is_signed != nullptr) {
                        *is_signed = (response[i+3] & 0x80) != 0;
                    }
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool SolarmanV5::readRegister(uint16_t register_addr, uint16_t *value, bool *is_signed) {
    uint8_t request_frame[40];
    uint8_t response[300];
    size_t response_len;
    
    size_t frame_len = buildV5Frame(request_frame, register_addr, 1);
    
    if (!sendReceive(request_frame, frame_len, response, &response_len)) {
        return false;
    }
    
    return parseResponse(response, response_len, value, is_signed);
}

bool SolarmanV5::readHoldingRegisters(uint16_t start_addr, uint16_t count, uint16_t *values) {
    // Implementación para leer múltiples registros
    // Por simplicidad, por ahora leemos uno por uno
    for (uint16_t i = 0; i < count; i++) {
        if (!readRegister(start_addr + i, &values[i])) {
            return false;
        }
        delay(100); // Pequeña pausa entre lecturas
    }
    return true;
}