#pragma once
#include <furi.h>
#include <furi_hal_i2c.h>

// Struktura do przechowywania danych sensora
typedef struct {
    int16_t acc_x, acc_y, acc_z;
    int16_t gyro_x, gyro_y, gyro_z;
    float temp_c;
} Mpu6050Data;

// Klasa do obsługi komunikacji z MPU6050
class Mpu6050 {
public:
    // Domyślny adres I2C (7-bitowy)
    static constexpr uint8_t I2C_ADDR = 0x68; 
    static constexpr uint8_t MPU6050_TIMEOUT = 100;
    
    Mpu6050();
    ~Mpu6050();

    // Inicjalizuje I2C i konfiguruje czujnik (budzi go)
    bool init();

    // Odczytuje 14 bajtów danych z czujnika i wypełnia strukturę
    bool read_data(Mpu6050Data& data);

private:
    // Odczyt pojedynczego rejestru
    bool read_register(uint8_t reg_addr, uint8_t* data, size_t size);
    
    // Zapis do rejestru
    bool write_register(uint8_t reg_addr, uint8_t data);
};
