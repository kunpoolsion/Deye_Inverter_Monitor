#ifndef SOLARMANV5_H
#define SOLARMANV5_H

#include <WiFiClient.h>
#include <stdint.h>
#include <stddef.h>

class SolarmanV5 {
private:
    uint32_t _datalogger_sn;        // Serial Number del datalogger (formato decimal: 2975087801)
    uint8_t _mb_slave_id;           // Slave ID del inversor (normalmente 1)
    uint8_t _sequence_number;       // Contador de secuencia para frames
    const char* _datalogger_ip;     // IP del datalogger en la red local
    uint16_t _datalogger_port;      // Puerto TCP del datalogger (normalmente 8899)
    
    // Métodos privados
    uint16_t calculateCRC(uint8_t *data, size_t length);
    uint8_t calculateV5Checksum(uint8_t *data, size_t length);
    size_t buildV5Frame(uint8_t *v5_frame, uint16_t start_addr, uint16_t reg_count);
    bool sendReceive(uint8_t *request_frame, size_t frame_len, uint8_t *response, size_t *response_len);
    bool parseResponse(uint8_t *response, size_t len, uint16_t *value, bool *is_signed);

public:
    /**
     * @brief Constructor de la clase SolarmanV5
     * 
     * @param datalogger_ip Dirección IP del datalogger en la red local
     * @param datalogger_sn Número de serie del datalogger (formato decimal, ej: 2975087801)
     * @param mb_slave_id Slave ID del inversor (por defecto 1)
     * @param datalogger_port Puerto TCP del datalogger (por defecto 8899)
     */
    SolarmanV5(const char* datalogger_ip, uint32_t datalogger_sn, uint8_t mb_slave_id = 1, uint16_t datalogger_port = 8899);
    
    /**
     * @brief Inicializa la comunicación con el datalogger
     * 
     * Nota: Actualmente este método no realiza ninguna operación específica,
     * pero se mantiene para compatibilidad futura.
     */
    void begin();
    
    /**
     * @brief Lee un solo registro del inversor
     * 
     * @param register_addr Dirección del registro a leer
     * @param value Puntero donde se almacenará el valor leído
     * @param is_signed Puntero opcional para indicar si el valor es signed (2's complement)
     * @return true Si la lectura fue exitosa
     * @return false Si hubo error en la comunicación
     */
    bool readRegister(uint16_t register_addr, uint16_t *value, bool *is_signed = nullptr);
    
    /**
     * @brief Lee múltiples registros consecutivos del inversor
     * 
     * @param start_addr Dirección inicial del primer registro
     * @param count Número de registros a leer
     * @param values Array donde se almacenarán los valores leídos
     * @return true Si todas las lecturas fueron exitosas
     * @return false Si hubo error en alguna lectura
     */
    bool readHoldingRegisters(uint16_t start_addr, uint16_t count, uint16_t *values);
    
    // ============================================================================
    // MÉTODOS DE CONFIGURACIÓN
    // ============================================================================
    
    /**
     * @brief Establece el Slave ID del inversor
     * 
     * @param slave_id Nuevo Slave ID (normalmente 1)
     */
    void setSlaveId(uint8_t slave_id) { _mb_slave_id = slave_id; }
    
    /**
     * @brief Establece el número de secuencia inicial
     * 
     * @param seq Nuevo valor inicial del contador de secuencia
     */
    void setSequenceNumber(uint8_t seq) { _sequence_number = seq; }
    
    /**
     * @brief Establece una nueva IP para el datalogger
     * 
     * @param new_ip Nueva dirección IP del datalogger
     */
    void setDataloggerIP(const char* new_ip) { _datalogger_ip = new_ip; }
    
    /**
     * @brief Establece un nuevo número de serie para el datalogger
     * 
     * @param new_sn Nuevo número de serie (formato decimal)
     */
    void setDataloggerSN(uint32_t new_sn) { _datalogger_sn = new_sn; }
    
    // ============================================================================
    // MÉTODOS DE INFORMACIÓN
    // ============================================================================
    
    /**
     * @brief Obtiene la IP actual del datalogger
     * 
     * @return const char* IP del datalogger
     */
    const char* getDataloggerIP() { return _datalogger_ip; }
    
    /**
     * @brief Obtiene el número de serie actual del datalogger
     * 
     * @return uint32_t Número de serie en formato decimal
     */
    uint32_t getDataloggerSN() { return _datalogger_sn; }
    
    /**
     * @brief Obtiene el Slave ID actual del inversor
     * 
     * @return uint8_t Slave ID
     */
    uint8_t getSlaveId() { return _mb_slave_id; }
    
    /**
     * @brief Obtiene el puerto actual del datalogger
     * 
     * @return uint16_t Puerto TCP
     */
    uint16_t getDataloggerPort() { return _datalogger_port; }
    
    /**
     * @brief Obtiene el número de secuencia actual
     * 
     * @return uint8_t Número de secuencia
     */
    uint8_t getSequenceNumber() { return _sequence_number; }
    
    /**
     * @brief Convierte el número de serie a formato hexadecimal
     * 
     * @param buffer Buffer donde se almacenará el string hexadecimal (mínimo 9 bytes)
     */
    void getDataloggerSNHex(char *buffer) {
        sprintf(buffer, "%08lX", _datalogger_sn);
    }
    
    /**
     * @brief Convierte el número de serie a bytes en little-endian
     * 
     * @param bytes Array de 4 bytes donde se almacenará el resultado
     */
    void getDataloggerSNBytes(uint8_t *bytes) {
        bytes[0] = _datalogger_sn & 0xFF;
        bytes[1] = (_datalogger_sn >> 8) & 0xFF;
        bytes[2] = (_datalogger_sn >> 16) & 0xFF;
        bytes[3] = (_datalogger_sn >> 24) & 0xFF;
    }
    
    /**
     * @brief Obtiene información de configuración en formato string
     * 
     * @param buffer Buffer donde se almacenará la información
     * @param buffer_size Tamaño del buffer
     */
    void getConfigInfo(char *buffer, size_t buffer_size) {
        char sn_hex[9];
        getDataloggerSNHex(sn_hex);
        snprintf(buffer, buffer_size, "IP: %s, SN: %lu (0x%s), Slave: %d, Port: %d", 
                 _datalogger_ip, _datalogger_sn, sn_hex, _mb_slave_id, _datalogger_port);
    }
};

#endif