
#pragma once

#include "gfx.h"
#include "layer.h"
#include <freertos/FreeRTOS.h>

#define ROW_NUM 7
#define COL_NUM 95

/**
 * @brief Initialize the matrix display system
 */
void matrix_init(void);

/**
 * @brief Main matrix display task - continuously updates the LED matrix
 */
void matrix_task(void *param);

/**
 * @brief Set the active layer for display output
 * 
 * @param layer Layer to display (or NULL to disable)
 */
void matrix_set_active_layer(gfx_layer_t *layer);

/**
 * @brief Get the currently displayed layer
 * 
 * @return Active layer or NULL
 */
gfx_layer_t *matrix_get_active_layer(void);
