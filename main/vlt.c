#include "vlt.h"
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "carousel.h"
#include "display/compositor.h"
#include "display/font_slot.h"
#include "display/gfx.h"

#define SLOT_DIGITS 3
#define VLT_FIRST_STOP_MS 900
#define VLT_STOP_STAGGER_MS 350

typedef struct
{
    bool spinning;
    int final_value;
    uint32_t stop_time;
} digit_state_t;

static const size_t vlt_sym_count = 6;
static digit_state_t digit_states[SLOT_DIGITS];

static void init_slot_spin(uint32_t start_ms)
{
    for (int i = 0; i < SLOT_DIGITS; i++)
    {
        digit_states[i].spinning = true;
        digit_states[i].final_value = rand() % (vlt_sym_count);
        digit_states[i].stop_time = start_ms + VLT_FIRST_STOP_MS + (i * VLT_STOP_STAGGER_MS);
    }
}

static int vlt_aligned_x(void)
{
    // Place VLT (carousel) in the left region
    return LEFT_REGION_X + 5 * 4;
}

static void draw_vlt_text(int col, int row)
{
    char buf[SLOT_DIGITS + 1];
    buf[SLOT_DIGITS] = '\0';
    uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

    for (int i = 0; i < SLOT_DIGITS; i++)
    {
        if (digit_states[i].spinning && current_time < digit_states[i].stop_time)
        {
            // Show random symbol while spinning (map to font_slot range)
            const uint8_t s = rand() % (vlt_sym_count);
            buf[i] = (char)(font_slot.first_char + (s % (font_slot.last_char - font_slot.first_char + 1)));
        }
        else
        {
            // Show final value when stopped (map to font_slot range)
            digit_states[i].spinning = false;
            buf[i] = (char)(font_slot.first_char + (digit_states[i].final_value % (font_slot.last_char - font_slot.first_char + 1)));
        }
    }

    gfx_draw_text(gfx_handle, &font_slot, col, row, (char *)buf, LEFT_REGION_WIDTH);
}

static bool all_digits_stopped(void)
{
    for (int i = 0; i < SLOT_DIGITS; i++)
    {
        if (digit_states[i].spinning)
        {
            return false;
        }
    }
    return true;
}

// Task che anima il testo stile slot
void vlt_task(void *param)
{
    uint32_t last_generation_seen = UINT32_MAX;
    uint32_t current_time;
    int x_pos = vlt_aligned_x();

    for (;;)
    {
        if (carousel_get_item() == CAROUSEL_ITEM_VLT)
        {
            uint32_t generation = carousel_get_generation();
            if (generation != last_generation_seen)
            {
                current_time = pdTICKS_TO_MS(xTaskGetTickCount());
                init_slot_spin(current_time);
                last_generation_seen = generation;
            }

            compositor_clear_left_region();
            draw_vlt_text(x_pos, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Update display frequently
    }
}
