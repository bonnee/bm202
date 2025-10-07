#include "vlt.h"
#include <stdbool.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "display/font_slot.h"
#include "display/gfx.h"

#define SLOT_DIGITS 3
#define MIN_SPIN_TIME 500
#define MAX_SPIN_TIME 2000
#define REVEAL_DELAY 300  // ms between each digit reveal
#define REPEAT_DELAY 4000 // ms before starting new animation

typedef struct
{
    bool spinning;
    int final_value;
    uint32_t stop_time;
} digit_state_t;

static const size_t vlt_sym_count = 6;
static digit_state_t digit_states[SLOT_DIGITS];

static void init_slot_spin(void)
{
    for (int i = 0; i < SLOT_DIGITS; i++)
    {
        digit_states[i].spinning = true;
        digit_states[i].final_value = rand() % (vlt_sym_count);
        // Stagger the stop times
        digit_states[i].stop_time = pdTICKS_TO_MS(xTaskGetTickCount()) +
                                    MIN_SPIN_TIME +
                                    (rand() % (MAX_SPIN_TIME - MIN_SPIN_TIME)) +
                                    (i * REVEAL_DELAY);
    }
}

static void draw_vlt_text(int col, int row)
{
    uint8_t buf[SLOT_DIGITS + 1];
    buf[SLOT_DIGITS] = '\0';
    uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

    for (int i = 0; i < SLOT_DIGITS; i++)
    {
        if (i > 0)
        {
            // strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
        }

        if (digit_states[i].spinning && current_time < digit_states[i].stop_time)
        {
            // Show random symbol while spinning
            const uint8_t s = rand() % (vlt_sym_count);
            // strncat(buf, s, sizeof(buf) - strlen(buf) - 1);
            buf[i] = s; // s;
        }
        else
        {
            // Show final value when stopped
            digit_states[i].spinning = false;
            buf[i] = digit_states[i].final_value;
            // strncat(buf, digit_states[i].final_value,
            //        sizeof(buf) - strlen(buf) - 1);
        }
    }

    gfx_draw_text(gfx_handle, &font_slot, col, row, (char *)buf, COL_NUM - col);
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
void vlt_task(void *x)
{
    uint32_t last_spin_time = 0;

    for (;;)
    {
        uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());

        // Start new spin after delay
        if (all_digits_stopped() &&
            (current_time - last_spin_time) > REPEAT_DELAY)
        {
            init_slot_spin();
            last_spin_time = current_time;
        }

        draw_vlt_text((uint32_t)x, 0);
        vTaskDelay(pdMS_TO_TICKS(50)); // Update display frequently
    }
}
