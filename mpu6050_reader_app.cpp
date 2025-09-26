#include "furi.h"
#include "furi_hal.h"
#include "gui/gui.h"
#include "gui/canvas.h"
#include <furi_hal_i2c.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_bus.h>
#include <math.h> 

// MPU-6050 sensor I2C address (7-bit)
// Default address is 0x68 (AD0 pulled low)
#define MPU6050_I2C_ADDR 0x68

// MPU-6050 registers
#define MPU6050_REG_PWR_MGMT_1 0x6B // Power Management 1
#define MPU6050_REG_SMPLRT_DIV 0x19 // Sample Rate Divider
#define MPU6050_REG_CONFIG 0x1A    // Configuration
#define MPU6050_REG_GYRO_CONFIG 0x1B // Gyroscope Configuration
#define MPU6050_REG_ACCEL_CONFIG 0x1C // Accelerometer Configuration
#define MPU6050_REG_ACCEL_XOUT_H 0x3B // High byte of Accel X-axis data

// Power Management 1 settings
#define MPU6050_CLOCK_SEL_PLL_XG 0x01 // PLL with X axis gyroscope reference
#define MPU6050_RESET 0x80            // Reset device

// Configuration settings (DLPF)
#define MPU6050_DLPF_CFG_20HZ 0x04

// I2C operation timeout
#define MPU6050_I2C_TIMEOUT 100

// Enumeration for managing application states (screens)
typedef enum {
    AppState_Main,
    AppState_Settings,
    AppState_About,
    AppState_MaxG, // Added for max G submenu
} AppState;

// Enumeration for options in the settings menu
typedef enum {
    SettingsItem_Address,
    SettingsItem_AccelFS,
    SettingsItem_GyroFS,
    SettingsItem_Count
} SettingsItem;

// Structure to store sensor data
typedef struct {
    int16_t acc_x, acc_y, acc_z;
    float acc_g_x, acc_g_y, acc_g_z;
} Mpu6050Data;

// Structure to store application state
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMutex* mutex;
    AppState current_state;
    bool running;
    bool is_sensor_initialized;
    Mpu6050Data sensor_data;
    float max_g_x, max_g_y, max_g_z; // Added to store maximum G values for each axis

    // Variables for settings
    uint8_t settings_cursor;
    uint8_t i2c_address;
    uint8_t accel_fsr_index; // 0=2g, 1=4g, 2=8g, 3=16g (Default 4g, index 1)
    uint8_t gyro_fsr_index;  // 0=250, 1=500, 2=1000, 3=2000 deg/s (Default 500 deg/s, index 1)
} MPU6050App;

// Function to draw the main screen
static void draw_main_screen(Canvas* canvas, MPU6050App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "MPU-6050 ");

    // Secure access to sensor data
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool sensor_ok = app->is_sensor_initialized;
    float acc_g_x = app->sensor_data.acc_g_x;
    float acc_g_y = app->sensor_data.acc_g_y;
    float acc_g_z = app->sensor_data.acc_g_z;
    furi_mutex_release(app->mutex);

    if (sensor_ok) {
        canvas_set_font(canvas, FontSecondary);
        FuriString* acc_str = furi_string_alloc();
        
        // Draw Accel X
        canvas_draw_str(canvas, 5, 25, "Acc X:");
        furi_string_printf(acc_str, "%.2f g", (double)acc_g_x);
        canvas_draw_str_aligned(canvas, 123, 20, AlignRight, AlignTop, furi_string_get_cstr(acc_str));

        // Draw Accel Y
        canvas_draw_str(canvas, 5, 35, "Acc Y:");
        furi_string_printf(acc_str, "%.2f g", (double)acc_g_y);
        canvas_draw_str_aligned(canvas, 123, 30, AlignRight, AlignTop, furi_string_get_cstr(acc_str));

        // Draw Accel Z
        canvas_draw_str(canvas, 5, 45, "Acc Z:");
        furi_string_printf(acc_str, "%.2f g", (double)acc_g_z);
        canvas_draw_str_aligned(canvas, 123, 40, AlignRight, AlignTop, furi_string_get_cstr(acc_str));
        
        furi_string_free(acc_str);
    } else {
        // Draw "sensor not connected" message
        canvas_set_font(canvas, FontPrimary);
        const char* msg = "Connect sensor";
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, msg);
    }
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "set.. [<] About [>] [ok] Max ");
}

// Function to draw the settings screen
static void draw_settings_screen(Canvas* canvas, MPU6050App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Settings");

    FuriString* value_str = furi_string_alloc();
    canvas_set_font(canvas, FontSecondary);

    const uint8_t row_height = 13; // Ustalona wysokość wiersza
    uint8_t y_pos;
    
    // --- I2C Address Option ---
    y_pos = 20;
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_box(canvas, 0, y_pos - 1, 128, row_height); // Rysuj boks tła
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, 5, y_pos + 9, "I2C Address:");
    furi_string_printf(value_str, "0x%02X", app->i2c_address);
    canvas_draw_str_aligned(canvas, 123, y_pos + 3, AlignRight, AlignTop, furi_string_get_cstr(value_str));
    if (app->settings_cursor == SettingsItem_Address) {
        canvas_draw_str(canvas, 1, y_pos + 9, ">");
    }
    canvas_set_color(canvas, ColorBlack);

    // --- Accel FSR Option ---
    y_pos = 33;
    if (app->settings_cursor == SettingsItem_AccelFS) {
        canvas_draw_box(canvas, 0, y_pos - 1, 128, row_height);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    const char* accel_fsr_values[] = {"+/- 2g", "+/- 4g", "+/- 8g", "+/- 16g"};
    canvas_draw_str(canvas, 5, y_pos + 9, "Accel FSR:");
    canvas_draw_str_aligned(canvas, 123, y_pos + 3, AlignRight, AlignTop, accel_fsr_values[app->accel_fsr_index]);
    if (app->settings_cursor == SettingsItem_AccelFS) {
        canvas_draw_str(canvas, 1, y_pos + 9, ">");
    }
    canvas_set_color(canvas, ColorBlack);

    // --- Gyro FSR Option ---
    y_pos = 46;
    if (app->settings_cursor == SettingsItem_GyroFS) {
        canvas_draw_box(canvas, 0, y_pos - 1, 128, row_height);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    const char* gyro_fsr_values[] = {"+/- 250", "+/- 500", "+/- 1000", "+/- 2000"};
    canvas_draw_str(canvas, 5, y_pos + 9, "Gyro FSR:");
    canvas_draw_str_aligned(canvas, 123, y_pos + 3, AlignRight, AlignTop, gyro_fsr_values[app->gyro_fsr_index]);
    if (app->settings_cursor == SettingsItem_GyroFS) {
        canvas_draw_str(canvas, 1, y_pos + 9, ">");
    }
    canvas_set_color(canvas, ColorBlack);
    
    furi_string_free(value_str);

    // Back button
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[Ok/Back] Back");
}

// Function to draw the about screen
static void draw_about_screen(Canvas* canvas, MPU6050App* app) {
    UNUSED(app); 
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "About");

    // Zmiana na FontSecondary i lepsze pozycjonowanie, aby zmieścić się na ekranie
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "MPU-6050 Reader Application");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "by Dr Mosfet");
    
    // Back button
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "[Ok/Back] Back");
}

// Function to draw the max G screen
static void draw_max_g_screen(Canvas* canvas, MPU6050App* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Max G Values");

    // Secure access to max G data
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    float max_g_x = app->max_g_x;
    float max_g_y = app->max_g_y;
    float max_g_z = app->max_g_z;
    furi_mutex_release(app->mutex);

    canvas_set_font(canvas, FontSecondary);
    FuriString* g_str = furi_string_alloc();
    
    // Draw Max G X
    canvas_draw_str(canvas, 5, 25, "Max X:");
    furi_string_printf(g_str, "%.2f g", (double)max_g_x);
    canvas_draw_str_aligned(canvas, 123, 20, AlignRight, AlignTop, furi_string_get_cstr(g_str));

    // Draw Max G Y
    canvas_draw_str(canvas, 5, 35, "Max Y:");
    furi_string_printf(g_str, "%.2f g", (double)max_g_y);
    canvas_draw_str_aligned(canvas, 123, 30, AlignRight, AlignTop, furi_string_get_cstr(g_str));

    // Draw Max G Z
    canvas_draw_str(canvas, 5, 45, "Max Z:");
    furi_string_printf(g_str, "%.2f g", (double)max_g_z);
    canvas_draw_str_aligned(canvas, 123, 40, AlignRight, AlignTop, furi_string_get_cstr(g_str));
    
    furi_string_free(g_str);

    // Instructions
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Reset [<] Back");
}

// Main drawing function that switches screens
static void mpu6050_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    MPU6050App* app = static_cast<MPU6050App*>(context);

    switch (app->current_state) {
        case AppState_Main:
            draw_main_screen(canvas, app);
            break;
        case AppState_Settings:
            draw_settings_screen(canvas, app);
            break;
        case AppState_About:
            draw_about_screen(canvas, app);
            break;
        case AppState_MaxG:
            draw_max_g_screen(canvas, app);
            break;
    }
}

// Function to configure the MPU-6050 sensor
static bool init_mpu6050(MPU6050App* app) {
    // 1. Reset the device
    uint8_t reset_cmd[] = {MPU6050_REG_PWR_MGMT_1, MPU6050_RESET};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        reset_cmd,
        sizeof(reset_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (!success) return false;
    furi_delay_ms(100); // Wait for reset to complete

    // 2. Wake up and set clock source
    uint8_t pwr_mgmt_cmd[] = {MPU6050_REG_PWR_MGMT_1, MPU6050_CLOCK_SEL_PLL_XG};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        pwr_mgmt_cmd,
        sizeof(pwr_mgmt_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (!success) return false;

    // 3. Set Sample Rate Divider to 0 (1kHz sample rate)
    uint8_t smplrt_cmd[] = {MPU6050_REG_SMPLRT_DIV, 0x00};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        smplrt_cmd,
        sizeof(smplrt_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (!success) return false;

    // 4. Set Configuration (DLPF)
    uint8_t config_cmd[] = {MPU6050_REG_CONFIG, MPU6050_DLPF_CFG_20HZ};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        config_cmd,
        sizeof(config_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (!success) return false;

    // 5. Set Accelerometer Configuration (FSR)
    uint8_t accel_fsr = app->accel_fsr_index << 3; // Shift index to the correct bit position
    uint8_t accel_config_cmd[] = {MPU6050_REG_ACCEL_CONFIG, accel_fsr};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        accel_config_cmd,
        sizeof(accel_config_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (!success) return false;

    // 6. Set Gyroscope Configuration (FSR)
    uint8_t gyro_fsr = app->gyro_fsr_index << 3; // Shift index to the correct bit position
    uint8_t gyro_config_cmd[] = {MPU6050_REG_GYRO_CONFIG, gyro_fsr};
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        gyro_config_cmd,
        sizeof(gyro_config_cmd),
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        MPU6050_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return success;
}

// Function to calculate the accelerometer sensitivity scale factor (LSB/g)
static float get_accel_sensitivity(uint8_t accel_fsr_index) {
    // FSR values: 0=+/-2g, 1=+/-4g, 2=+/-8g, 3=+/-16g
    float sensitivity[] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};
    if (accel_fsr_index < sizeof(sensitivity) / sizeof(sensitivity[0])) {
        return sensitivity[accel_fsr_index];
    }
    return 8192.0f; // Default for +/- 4g (index 1)
}

// Function to read data from the sensor
static bool read_mpu6050(MPU6050App* app) {
    uint8_t raw_data[14]; // Accel X, Y, Z, Temp, Gyro X, Y, Z (2 bytes each)
    uint8_t reg_addr = MPU6050_REG_ACCEL_XOUT_H;

    // Acquire I2C bus access
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Send register address to start reading from
    bool success_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        &reg_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        MPU6050_I2C_TIMEOUT);

    // Read 14 bytes (Accel, Temp, Gyro)
    bool success_rx = false;
    if (success_tx) {
        success_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            app->i2c_address << 1,
            false,
            raw_data,
            14,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            MPU6050_I2C_TIMEOUT);
    }

    // Release I2C bus
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if (success_tx && success_rx) {
        // Data is received as MSB first
        int16_t raw_acc_x = (static_cast<int16_t>(raw_data[0]) << 8) | raw_data[1];
        int16_t raw_acc_y = (static_cast<int16_t>(raw_data[2]) << 8) | raw_data[3];
        int16_t raw_acc_z = (static_cast<int16_t>(raw_data[4]) << 8) | raw_data[5]; 
        
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->sensor_data.acc_x = raw_acc_x;
        app->sensor_data.acc_y = raw_acc_y;
        app->sensor_data.acc_z = raw_acc_z;
        
        // Calculate G values
        float sensitivity = get_accel_sensitivity(app->accel_fsr_index);
        app->sensor_data.acc_g_x = (float)raw_acc_x / sensitivity;
        app->sensor_data.acc_g_y = (float)raw_acc_y / sensitivity;
        app->sensor_data.acc_g_z = (float)raw_acc_z / sensitivity;

        // Update max G values for each axis (using absolute values)
        if (fabsf(app->sensor_data.acc_g_x) > app->max_g_x) {
            app->max_g_x = fabsf(app->sensor_data.acc_g_x);
        }
        if (fabsf(app->sensor_data.acc_g_y) > app->max_g_y) {
            app->max_g_y = fabsf(app->sensor_data.acc_g_y);
        }
        if (fabsf(app->sensor_data.acc_g_z) > app->max_g_z) {
            app->max_g_z = fabsf(app->sensor_data.acc_g_z);
        }

        app->is_sensor_initialized = true;
        furi_mutex_release(app->mutex);
        return true;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->is_sensor_initialized = false;
    furi_mutex_release(app->mutex);
    return false;
}

// Function to handle input events (keys)
static void mpu6050_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    MPU6050App* app = static_cast<MPU6050App*>(context);

    if (input_event->type == InputTypeShort) {
        switch (app->current_state) {
            case AppState_Main:
                if (input_event->key == InputKeyOk) {
                    app->current_state = AppState_MaxG; // Changed to Max G submenu
                } else if (input_event->key == InputKeyBack) {
                    app->running = false;
                } else if (input_event->key == InputKeyRight) {
                    app->current_state = AppState_About;
                } else if (input_event->key == InputKeyLeft) {
                    app->current_state = AppState_Settings;
                }
                break;
            case AppState_Settings:
                if (input_event->key == InputKeyUp) {
                    if (app->settings_cursor > 0) {
                        app->settings_cursor--;
                    }
                } else if (input_event->key == InputKeyDown) {
                    if (app->settings_cursor < SettingsItem_Count - 1) {
                        app->settings_cursor++;
                    }
                } else if (input_event->key == InputKeyLeft || input_event->key == InputKeyRight) {
                    if (app->settings_cursor == SettingsItem_Address) {
                        // Change I2C Address (typically 0x68 or 0x69)
                        if (input_event->key == InputKeyLeft) {
                            app->i2c_address = (app->i2c_address == 0x69) ? 0x68 : 0x69;
                        } else {
                            app->i2c_address = (app->i2c_address == 0x68) ? 0x69 : 0x68;
                        }
                    } else if (app->settings_cursor == SettingsItem_AccelFS) {
                        // Change Accel FSR (0-3)
                        if (input_event->key == InputKeyLeft) {
                            app->accel_fsr_index = (app->accel_fsr_index == 0) ? 3 : app->accel_fsr_index - 1;
                        } else {
                            app->accel_fsr_index = (app->accel_fsr_index == 3) ? 0 : app->accel_fsr_index + 1;
                        }
                    } else if (app->settings_cursor == SettingsItem_GyroFS) {
                        // Change Gyro FSR (0-3)
                        if (input_event->key == InputKeyLeft) {
                            app->gyro_fsr_index = (app->gyro_fsr_index == 0) ? 3 : app->gyro_fsr_index - 1;
                        } else {
                            app->gyro_fsr_index = (app->gyro_fsr_index == 3) ? 0 : app->gyro_fsr_index + 1;
                        }
                    }
                    // Apply new settings after changing a value
                    init_mpu6050(app);
                } else if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
            case AppState_About:
                if (input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
            case AppState_MaxG:
                if (input_event->key == InputKeyOk) {
                    // Reset max G values
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->max_g_x = 0.0f;
                    app->max_g_y = 0.0f;
                    app->max_g_z = 0.0f;
                    furi_mutex_release(app->mutex);
                } else if (input_event->key == InputKeyBack) {
                    app->current_state = AppState_Main;
                }
                break;
        }
    }
}

// Application allocation and initialization
static MPU6050App* mpu6050_app_alloc() {
    MPU6050App* app = static_cast<MPU6050App*>(malloc(sizeof(MPU6050App)));
    furi_assert(app);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, mpu6050_draw_callback, app);
    view_port_input_callback_set(app->view_port, mpu6050_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    app->running = true;
    app->current_state = AppState_Main;
    app->is_sensor_initialized = false;
    
    // Initialize sensor data
    app->sensor_data = (Mpu6050Data){0, 0, 0, 0.0f, 0.0f, 0.0f};
    app->max_g_x = 0.0f; // Initialize max G values
    app->max_g_y = 0.0f;
    app->max_g_z = 0.0f;

    // Settings initialization
    app->settings_cursor = SettingsItem_Address;
    app->i2c_address = MPU6050_I2C_ADDR;
    app->accel_fsr_index = 1; // Default +/- 4g, index 1
    app->gyro_fsr_index = 1;  // Default +/- 500 deg/s, index 1
    
    return app;
}

// Free resources
static void mpu6050_app_free(MPU6050App* app) {
    furi_assert(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);
    free(app);
}

// Application entry point
extern "C" int32_t mpu6050_reader_app(void* p) { 
    UNUSED(p);
    MPU6050App* app = mpu6050_app_alloc();

    // Funkcja furi_hal_i2c_init() została usunięta, ponieważ powodowała błąd linkera.
    // Działanie I2C polega na tym, że bus jest inicjalizowany przy pierwszym furi_hal_i2c_acquire.
    
    while (app->running) {
        if(!app->is_sensor_initialized) {
            app->is_sensor_initialized = init_mpu6050(app);
        }

        if(app->is_sensor_initialized) {
            read_mpu6050(app);
        }

        view_port_update(app->view_port);
        furi_delay_ms(100); 
    }
    
    mpu6050_app_free(app);
    return 0;
}
