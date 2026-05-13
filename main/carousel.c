#include "carousel.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display/compositor.h"

static volatile carousel_item_t s_current_item = 0;
static volatile uint32_t s_generation = 0;

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
        s_current_item = (carousel_item_t)(((uint8_t)s_current_item + 1) % (uint8_t)CAROUSEL_ITEM_COUNT);
        s_generation++;
        compositor_clear_left_region();
    }
}
