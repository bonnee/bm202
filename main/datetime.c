#include "datetime.h"
#include "esp_sntp.h"
// #include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display/font5x7.h"
#include "display/gfx.h"

static void init()
{
    // Set timezone to Rome (UTC+1 with daylight savings)
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_init();
}

void datetime_task(void *params)
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    init();

    srand((unsigned)time(NULL));

    for (;;)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%a %H:%M", &timeinfo); // Removed +2 since TZ handles it
        gfx_draw_text(gfx_handle, &font5x7, 0, 0, strftime_buf, COL_NUM - 27);

        // ESP_LOGI(TAG, "Current time: %s", strftime_buf);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}