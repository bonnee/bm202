/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_event.h"
// #include "esp_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/ledc_types.h"
#include "nvs_flash.h"
// #include "protocol_examples_common.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <inttypes.h>
#include <portmacro.h>
#include <stdio.h>
#include <string.h>

#include "display/font5x7.h"
#include "display/gfx.h"
#include "display/matrix.h"

#define TAG "MAIN"
#define WIFI_SSID "eagleTRT"
#define WIFI_PASS "eaglepiTRT"
#define NTP_SERVER "time.inrim.it"

void hello_task(void *params)
{
    for (;;)
    {
        // why does it crash when logging?
        // ESP_LOGI(TAG, "Hello world!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

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

static void initialize_time(void)
{
    // Set timezone to Rome (UTC+1 with daylight savings)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_init();
}

#define ROW_NUM 7
#define COL_NUM 95
void time_sync_task(void *params)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    for (;;)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%a %d/%m %H:%M", &timeinfo); // Removed +2 since TZ handles it
        gfx_draw_text(gfx_handle, &font5x7, 0, 0, strftime_buf, COL_NUM);

        // ESP_LOGI(TAG, "Current time: %s", strftime_buf);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    gfx_handle = gfx_init(COL_NUM, ROW_NUM);
    initialize_wifi();
    initialize_time(); // Replace initialize_sntp() with this
    // gfx_draw_text(gfx_handle, &font5x7, 0, 0, "Sborra nei pantaloni croccanti", COL_NUM);
    //   vTaskDelay(pdMS_TO_TICKS(1000));
    xTaskCreate(matrix_task, "matrix", 16384, NULL, 20, NULL);
    xTaskCreate(hello_task, "hello", 2048, NULL, 2, NULL);
    xTaskCreate(time_sync_task, "time_sync", 4096, NULL, 2, NULL);
}
