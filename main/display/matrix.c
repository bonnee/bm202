#include "matrix.h"
#include "driver/spi_common.h"
#include "esp_err.h"
#include "hal/ledc_types.h"
#include "hal/spi_types.h"
#include "portmacro.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <string.h>

#include "font5x7.h"
#include "gfx.h"

#define TAG "MAT"

// Layer display control
static gfx_layer_t *active_layer = NULL;
static SemaphoreHandle_t layer_mutex = NULL;

#define IO_CLR GPIO_NUM_4
#define IO_CLK GPIO_NUM_3
#define IO_MOSI GPIO_NUM_1 // Dedicated SPI MOSI pin
#define SPI_FREQ (2 * 1000 * 1000)

// GPIOs for each row. Please change these to match your hardware.
static const gpio_num_t row_gpios[ROW_NUM] = {
    GPIO_NUM_0,
    GPIO_NUM_21,
    GPIO_NUM_10,
    GPIO_NUM_20,
    GPIO_NUM_7,
    GPIO_NUM_6,
    GPIO_NUM_5,
};

static spi_device_handle_t spi;

// ============================================================================
// Layer Management
// ============================================================================

void matrix_set_active_layer(gfx_layer_t *layer) {
    if (layer_mutex) {
        xSemaphoreTake(layer_mutex, portMAX_DELAY);
    }
    active_layer = layer;
    if (layer_mutex) {
        xSemaphoreGive(layer_mutex);
    }
}

gfx_layer_t *matrix_get_active_layer(void) {
    gfx_layer_t *layer = NULL;
    if (layer_mutex) {
        xSemaphoreTake(layer_mutex, portMAX_DELAY);
    }
    layer = active_layer;
    if (layer_mutex) {
        xSemaphoreGive(layer_mutex);
    }
    return layer;
}

static void gfx_update_callback(TimerHandle_t xTimer)
{
    if (gfx_handle)
    {
        gfx_update(gfx_handle);
    }
}

static void init()
{
    // Create layer mutex for thread-safe layer switching
    if (!layer_mutex) {
        layer_mutex = xSemaphoreCreateMutex();
    }

    // Configure SPI
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = IO_MOSI,
        .sclk_io_num = IO_CLK,
    }; // .max_transfer_sz = 96 / 8};

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_FREQ,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
        .pre_cb = NULL, // Logic is now in matrix_task
        .post_cb = NULL,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));

    // Configure GPIOs
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask =
        BIT64(IO_CLR);
    io_conf.pull_down_en = 1;
    io_conf.pull_up_en = 0;
    // configure GPIO with the given settings

    for (uint8_t i = 0; i < ROW_NUM; i++)
    {
        io_conf.pin_bit_mask |= BIT64(row_gpios[i]);
    }
    gpio_config(&io_conf);
    // set row gpios to low
    for (uint8_t i = 0; i < ROW_NUM; i++)
    {
        gpio_set_level(row_gpios[i], pdFALSE);
    }

    // Reset display
    gpio_set_level(IO_CLR, pdFALSE);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(IO_CLR, pdTRUE);
    gpio_set_level(IO_CLK, pdFALSE);

    // Create and start a timer for gfx_update
    TimerHandle_t gfx_update_timer = xTimerCreate(
        "gfx_update_timer", // Timer name
        pdMS_TO_TICKS(50),  // 50ms period
        pdTRUE,             // Auto-reload
        (void *)0,          // Timer ID
        gfx_update_callback // Callback function
    );

    if (gfx_update_timer)
    {
        xTimerStart(gfx_update_timer, 0);
    }
}

void matrix_init(void) {
    // Initialize but don't run the task yet
    // This allows setting up layers before matrix_task starts
    if (!layer_mutex) {
        layer_mutex = xSemaphoreCreateMutex();
    }
}

void matrix_task(void *params)
{
    init();

    spi_device_acquire_bus(spi, portMAX_DELAY);

    uint8_t row = 0;
    for (;;)
    {
        // Get current active layer or fallback to gfx_handle
        gfx_layer_t *layer = NULL;
        if (layer_mutex) {
            xSemaphoreTake(layer_mutex, pdMS_TO_TICKS(1));
        }
        layer = active_layer;
        if (layer_mutex) {
            xSemaphoreGive(layer_mutex);
        }

        uint8_t *row_data = NULL;
        uint16_t bytes_per_row = (COL_NUM + 7) / 8;

        // Prefer active layer; fallback to legacy gfx_handle
        if (layer && layer->fb) {
            row_data = layer->fb + (row * bytes_per_row);
        } else if (gfx_handle && gfx_handle->fb) {
            row_data = gfx_handle->fb + (row * bytes_per_row);
        }

        if (row_data) {
            spi_transaction_t trans = {
                .tx_buffer = row_data,
                .length = COL_NUM + 1,
            };
            ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_polling_transmit(spi, &trans));
        }

        gpio_set_level(row_gpios[row], pdTRUE);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(row_gpios[row], pdFALSE);
        row = (row + 1) % ROW_NUM;
    }
    spi_device_release_bus(spi);
}
