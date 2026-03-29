#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "esp_log.h"

static const char *TAG = "RECEIVER";

#define TX_PIN 26
#define RX_PIN 25

// CAN IDs matching the Sender
#define ID_CRITICAL_TEMP 0x001
#define ID_NORMAL_TEMP   0x010
#define ID_ENGINE_PRESSURE 0x020
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

    ESP_LOGI(TAG, "Receiver started. Waiting for telemetry");

    bool crisis_mode = false; 

    while (1) {
        twai_message_t msg;
        
        if (twai_receive(&msg, portMAX_DELAY) == ESP_OK) {
            
            // 1. ALWAYS check for critical messages first
            if (msg.identifier == ID_CRITICAL_TEMP && msg.data_length_code == 4) {
                crisis_mode = true; // ENABLE ALARM MODE
                
                float_payload_t temp;
                for(int i=0; i<4; i++) { temp.bytes[i] = msg.data[i]; }
                ESP_LOGE(TAG, "[ALARM] CRITICAL TEMP: %.1f C! IGNORING MINOR TASKS!", temp.f_val);
            }
            // 2. Return to normal operation
            else if (msg.identifier == ID_NORMAL_TEMP && msg.data_length_code == 4) {
                crisis_mode = false; // DISABLE ALARM MODE
                
                float_payload_t temp;
                for(int i=0; i<4; i++) { temp.bytes[i] = msg.data[i]; }
                ESP_LOGI(TAG, "[TEMP] Engine: %.1f C", temp.f_val);
            }
            
            // 3. Process minor tasks ONLY if there is no crisis
            if (crisis_mode == false) {
                
                if (msg.identifier == ID_GPS_DATA && msg.data_length_code == 8) {
                    float_payload_t lat, lon;
                    for(int i=0; i<4; i++) {
                        lat.bytes[i] = msg.data[i];
                        lon.bytes[i] = msg.data[i+4];
                    }
                    ESP_LOGI(TAG, "[GPS] Lat: %.4f | Lon: %.4f", lat.f_val, lon.f_val);
                }

                else if (msg.identifier == ID_ENGINE_PRESSURE && msg.data_length_code == 4) {
                    float_payload_t press;
                    for(int i=0; i<4; i++) { press.bytes[i] = msg.data[i]; }
                    ESP_LOGI(TAG, "[PRESS] Engine Pressure: %.1f atm", press.f_val);
                }
                
                else if (msg.identifier == ID_SYS_STATUS && msg.data_length_code == 1) {
                    //ESP_LOGI(TAG, "[STATUS] System Status: 0x%02X", msg.data[0]);
                }
                
            } else {
                // If we are here, crisis_mode == true and we received GPS or Status the packet is intentionally ignored to prioritize critical tasks
                
            }
        }
    }
}