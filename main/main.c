#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/ledc_types.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <inttypes.h>
#include <portmacro.h>
#include <stdio.h>
#include <string.h>

#include "display/font5x7.h"
#include "display/font_slot.h"
#include "display/gfx.h"
#include "display/matrix.h"

#include "airplanes/airplanes.h"
#include "carousel.h"
#include "datetime.h"
#include "vlt.h"
#include "weather/weather.h"

#define TAG "MAIN"
#define WIFI_SSID "eagleTRT"
#define WIFI_PASS "eaglepiTRT"

gfx_handle_t *gfx_handle;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
}

static void initialize_wifi(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    gfx_handle = gfx_init(COL_NUM, ROW_NUM);

    initialize_wifi();
    xTaskCreate(matrix_task, "matrix", 16384, NULL, 20, NULL);
    xTaskCreate(carousel_task, "carousel", 4096, (void *)CAROUSEL_DEFAULT_INTERVAL_MS, 4, NULL);
    xTaskCreate(datetime_task, "time_sync", 4096, NULL, 2, NULL);
    xTaskCreate(vlt_task, "vlt", 4096, (void *)68, 5, NULL);
    xTaskCreate(weather_task, "weather", 8192, NULL, 3, NULL);
    xTaskCreate(airplanes_task, "airplanes", 16384, NULL, 3, NULL);
}
