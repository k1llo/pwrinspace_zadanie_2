#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "esp_log.h"

static const char *TAG = "SENDER";

#define TX_PIN 26
#define RX_PIN 25

#define ID_CRITICAL_TEMP 0x001
#define ID_NORMAL_TEMP   0x010
#define ID_ENGINE_PRESSURE      0x020   
#define ID_GPS_DATA      0x050
#define ID_SYS_STATUS    0x100

typedef union {
    float f_val;
    uint8_t bytes[4];
} float_payload_t;

void app_main(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();

    ESP_LOGI(TAG, "Sender started. Generating CAN traffic");

    float engine_temp = 20.0f;
    float gps_lat = 51.107883f; // Wroclaw coords
    float gps_lon = 17.038538f;
    //float engine_pressure = engine_temp * 1.5f; 
    uint8_t status_code = 0;

    while (1) {
        twai_message_t msg;
        msg.extd = 0;
        msg.rtr = 0;
        float engine_pressure = engine_temp * 1.5f;

        // 1. Send System Status (DLC = 1 byte, Low Priority)
        msg.identifier = ID_SYS_STATUS;
        msg.data_length_code = 1;
        msg.data[0] = status_code;
        twai_transmit(&msg, pdMS_TO_TICKS(5));

        // 2. Send GPS (DLC = 8 bytes, Medium Priority)
        msg.identifier = ID_GPS_DATA;
        msg.data_length_code = 8;
        float_payload_t lat, lon;
        lat.f_val = gps_lat;
        lon.f_val = gps_lon;
        for(int i=0; i<4; i++) {
            msg.data[i] = lat.bytes[i];
            msg.data[i+4] = lon.bytes[i];
        }
        twai_transmit(&msg, pdMS_TO_TICKS(5));

        msg.identifier = ID_ENGINE_PRESSURE;
        msg.data_length_code = 4;
        float_payload_t pressure;
        pressure.f_val = engine_pressure;
        for(int i=0; i<4; i++) {
            msg.data[i] = pressure.bytes[i];
        }
        twai_transmit(&msg, pdMS_TO_TICKS(5));

        // 3. Send Temperature (DLC = 4 bytes, Dynamic Priority)
        float_payload_t temp;
        temp.f_val = engine_temp;
        msg.data_length_code = 4;
        for(int i=0; i<4; i++) {
            msg.data[i] = temp.bytes[i];
        }

        // DYNAMIC PRIORITY LOGIC
        if (engine_temp >= 100.0f) {
            msg.identifier = ID_CRITICAL_TEMP; // HIGHEST PRIORITY
        } else {
            msg.identifier = ID_NORMAL_TEMP;   // HIGH PRIORITY
        }
        twai_transmit(&msg, pdMS_TO_TICKS(5));

        // Update simulation values
        engine_temp += 5.5f; // Heat up the engine
        gps_lat += 0.0001f;  // Move the rocket
        
        // Reset simulation if engine melts
        if (engine_temp > 130.0f) {
            engine_temp = 20.0f; 
            ESP_LOGW(TAG, "ENGINE RESTARTED");
            vTaskDelay(pdMS_TO_TICKS(2000)); // Pause to see it in logs
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}