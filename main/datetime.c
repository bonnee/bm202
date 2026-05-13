#include "datetime.h"
#include "esp_sntp.h"
// #include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "carousel.h"
#include "display/compositor.h"
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
    char time_buf[16];
    char date_buf[32];

    init();

    srand((unsigned)time(NULL));

    for (;;)
    {
        time(&now);
        localtime_r(&now, &timeinfo);

        strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
        compositor_clear_right_region();
        gfx_draw_text(gfx_handle, &font5x7, RIGHT_REGION_X, 0, time_buf, RIGHT_REGION_WIDTH);

        if (carousel_get_item() == CAROUSEL_ITEM_DATE)
        {
            int text_width_px;
            int x;

            strftime(date_buf, sizeof(date_buf), "%d %b", &timeinfo);
            // Show date in left (carousel) region
            x = LEFT_REGION_X;
            compositor_clear_left_region();
            gfx_draw_text(gfx_handle, &font5x7, x, 0, date_buf, LEFT_REGION_WIDTH);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}