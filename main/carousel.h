#pragma once

#include <stdint.h>

#define CAROUSEL_DEFAULT_INTERVAL_MS 5000

typedef enum
{
    CAROUSEL_ITEM_DATE = 0,
    CAROUSEL_ITEM_VLT,
    CAROUSEL_ITEM_WEATHER,
    CAROUSEL_ITEM_AIRPLANES,
    CAROUSEL_ITEM_COUNT,
} carousel_item_t;

void carousel_task(void *params);
carousel_item_t carousel_get_item(void);
uint32_t carousel_get_generation(void);
