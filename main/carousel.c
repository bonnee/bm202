#include "carousel.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "airplanes/airplanes.h"
#include "display/compositor.h"
#include "weather/weather.h"
static volatile carousel_item_t s_current_item = 0;
static volatile uint32_t s_generation = 0;

static bool carousel_item_enabled(carousel_item_t item)
{
    if (item == CAROUSEL_ITEM_AIRPLANES)
    {
        return airplanes_has_nearby();
    }
    if (item == CAROUSEL_ITEM_WEATHER)
    {
        return weather_has_data();
    }

    return true;
}

static carousel_item_t carousel_next_enabled_item(carousel_item_t current)
{
    uint8_t count = (uint8_t)CAROUSEL_ITEM_COUNT;
    uint8_t start = (uint8_t)current;

    for (uint8_t i = 1; i <= count; i++)
    {
        carousel_item_t candidate = (carousel_item_t)((start + i) % count);
        if (carousel_item_enabled(candidate))
        {
            return candidate;
        }
    }

    return current;
}

carousel_item_t carousel_get_item(void)
{
    return s_current_item;
}

uint32_t carousel_get_generation(void)
{
    return s_generation;
}

void carousel_task(void *params)
{
    uint32_t interval_ms = (uint32_t)params;
    if (interval_ms == 0)
    {
        interval_ms = CAROUSEL_DEFAULT_INTERVAL_MS;
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
        s_current_item = carousel_next_enabled_item(s_current_item);
        s_generation++;
        compositor_clear_left_region();
    }
}
