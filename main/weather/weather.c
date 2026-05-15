#include "weather.h"

#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "carousel.h"
#include "display/compositor.h"
#include "display/font5x7.h"
#include "display/gfx.h"
#include "display/sprite.h"
#include "weather/icons.h"

#include "location.h"

static const char *TAG = "WEATHER";

// Defaults near Milan, can be externalized later.
#define WEATHER_LAT LAT
#define WEATHER_LON LON
#define WEATHER_REFRESH_MS (10 * 60 * 1000)
#define WEATHER_RETRY_MS (30 * 1000)
#define WEATHER_ANIMATION_FRAME_MS 500

typedef struct
{
    int temp_c;
    int temp_max_c;
    int temp_min_c;
    int weather_code;
    uint8_t valid;
} weather_data_t;

// ============================================================================
// Sprite Bitmaps (defined once, reused by animations)
// ============================================================================

static weather_data_t s_weather = {
    .temp_c = 0,
    .temp_max_c = 0,
    .temp_min_c = 0,
    .weather_code = 0,
    .valid = 0,
};

static gfx_scrolling_text_id_t s_weather_text_id = GFX_SCROLLING_TEXT_ID_INVALID;

static bool is_night_now(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    return (timeinfo.tm_hour < 6) || (timeinfo.tm_hour >= 18);
}

bool weather_has_data(void)
{
    return s_weather.valid != 0;
}

// ============================================================================
// Animation Sequences
// ============================================================================
// Bitmaps stored once and referenced efficiently by animation sequences.

/**
 * @brief Get current icon bitmap for a weather code with animation support
 *
 * For partly cloudy (code 2), animates between sun and cloud every 250ms.
 * For other codes, uses static icons or pulsing variants.
 *
 * @param code     Weather code from Open-Meteo API
 * @param time_ms  Current time in milliseconds
 * @return         Pointer to sprite bitmap
 */
static const uint8_t *get_weather_icon_bitmap(int code, uint32_t time_ms)
{
    const bool night = is_night_now();

    // Clear sky (0-1): static sun
    if (code == 0 || code == 1)
    {
        return night ? sprite_moon : sprite_sun;
    }

    // Partly cloudy (2): animate between sun and cloud
    if (code == 2)
    {
        uint32_t cycle = (time_ms / WEATHER_ANIMATION_FRAME_MS) % 2;
        return (cycle == 0) ? sprite_cloud : (night ? sprite_moon_small : sprite_sun_small);
    }

    // Fog (45, 48): animate between two fog variants
    if (code == 45 || code == 48)
    {
        uint32_t cycle = (time_ms / WEATHER_ANIMATION_FRAME_MS) % 2;
        return (cycle == 0) ? sprite_fog : sprite_fog2;
    }
    // Overcast (3): static cloud
    if (code == 3)
    {
        return sprite_cloud_empty;
    }

    // Drizzle/rain (51-67, 80-82): pulsing rain
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82))
    {
        uint32_t cycle = (time_ms / WEATHER_ANIMATION_FRAME_MS) % 2;
        return (cycle == 0) ? sprite_rain : sprite_rain2;
    }

    // Snow (71-77, 85-86): sparkling snow
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86))
    {
        uint32_t cycle = (time_ms / WEATHER_ANIMATION_FRAME_MS) % 2;
        return (cycle == 0) ? sprite_snow : sprite_snow2;
    }

    // Thunderstorm (95+): flashing storm
    if (code >= 95)
    {
        uint32_t cycle = (time_ms / WEATHER_ANIMATION_FRAME_MS) % 2;
        return (cycle == 0) ? sprite_rain : sprite_storm_flash;
    }

    return sprite_cloud; // fallback
}

static uint16_t weather_group_width(int text_width_px)
{
    return ICON_SIZE + ICON_TEXT_GAP + text_width_px;
}

// Fetch from Open-Meteo API over HTTP.
static bool fetch_weather_stub(void)
{
    char url[256];
    snprintf(url,
             sizeof(url),
             "http://api.open-meteo.com/v1/forecast?latitude=%.2f&longitude=%.2f&daily=precipitation_probability_max,temperature_2m_max,temperature_2m_min,weather_code&current=temperature_2m,weather_code&timezone=auto&forecast_days=1",
             WEATHER_LAT,
             WEATHER_LON);

    ESP_LOGD(TAG, "Fetching weather from: %s", url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGW(TAG, "Failed to init HTTP client");
        return false;
    }

    ESP_LOGD(TAG, "Opening HTTP connection");

    // Open connection and fetch headers
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    // Read response status from headers
    int content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    ESP_LOGD(TAG, "HTTP status: %d, content_len: %d", status, content_len);

    if (status != 200)
    {
        ESP_LOGW(TAG, "Invalid response: status=%d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    char buf[4096] = {0};
    // Read all response body
    int total_read = esp_http_client_read(client, buf, sizeof(buf) - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read <= 0)
    {
        ESP_LOGW(TAG, "Failed to read response body, total_read=%d", total_read);
        return false;
    }

    buf[total_read] = '\0';
    ESP_LOGD(TAG, "Read %d bytes from weather response", total_read);

    cJSON *root = cJSON_Parse(buf);
    if (!root)
    {
        ESP_LOGW(TAG, "Failed to parse JSON response");
        return false;
    }

    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *temp = current ? cJSON_GetObjectItemCaseSensitive(current, "temperature_2m") : NULL;
    cJSON *code = current ? cJSON_GetObjectItemCaseSensitive(current, "weather_code") : NULL;

    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    cJSON *temp_max_arr = daily ? cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max") : NULL;
    cJSON *temp_max = temp_max_arr ? cJSON_GetArrayItem(temp_max_arr, 0) : NULL;
    cJSON *temp_min_arr = daily ? cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min") : NULL;
    cJSON *temp_min = temp_min_arr ? cJSON_GetArrayItem(temp_min_arr, 0) : NULL;

    if (cJSON_IsNumber(temp) && cJSON_IsNumber(code))
    {
        s_weather.temp_c = (int)(temp->valuedouble + 0.5);
        s_weather.temp_max_c = (int)(temp_max->valuedouble + 0.5);
        s_weather.temp_min_c = (int)(temp_min->valuedouble + 0.5);
        s_weather.weather_code = code->valueint;
        s_weather.valid = 1;
        ESP_LOGI(TAG, "Weather updated: %d°C (%d..%d) code=%d", s_weather.temp_c, s_weather.temp_min_c, s_weather.temp_max_c, s_weather.weather_code);
        cJSON_Delete(root);
        return true;
    }
    else
    {
        ESP_LOGW(TAG, "Missing temperature or weather_code in response");
    }

    cJSON_Delete(root);
    return false;
}

void weather_task(void *params)
{
    (void)params;
    TickType_t last_fetch;
    TickType_t fetch_interval = pdMS_TO_TICKS(WEATHER_RETRY_MS);
    char temp_text[12];

    // vTaskDelay(pdMS_TO_TICKS(5000)); // Stagger startup a bit to avoid contention

    ESP_LOGI(TAG, "started");
    last_fetch = xTaskGetTickCount() - fetch_interval;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();

        if ((now - last_fetch) >= fetch_interval)
        {
            if (fetch_weather_stub())
            {
                fetch_interval = pdMS_TO_TICKS(WEATHER_REFRESH_MS);
            }
            else
            {
                fetch_interval = pdMS_TO_TICKS(WEATHER_RETRY_MS);
            }
            last_fetch = now;
        }

        if (carousel_get_item() == CAROUSEL_ITEM_WEATHER)
        {
            int temp_width_px;
            int group_width_px;
            int icon_x;
            int text_x;
            const uint8_t *icon_bitmap;
            uint32_t time_ms;

            if (s_weather.valid)
            {
                ESP_LOGD(TAG, "Displaying weather: %d°C code=%d", s_weather.temp_c, s_weather.weather_code);
                snprintf(temp_text, sizeof(temp_text), "%d&%d~%d&", s_weather.temp_c, s_weather.temp_min_c, s_weather.temp_max_c);
            }
            else
            {
                snprintf(temp_text, sizeof(temp_text), "--&--~--&");
            }

            temp_width_px = (int)strlen(temp_text) * (font5x7.width + font5x7.spacing) - font5x7.spacing;
            group_width_px = weather_group_width(temp_width_px);
            // Place carousel (weather) in the left region
            icon_x = LEFT_REGION_X;
            text_x = icon_x + ICON_SIZE + ICON_TEXT_GAP;

            // Get current animation frame based on elapsed time
            time_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS);

            compositor_clear_rect(icon_x, 0, group_width_px, ROW_NUM);

            // For partly cloudy (code 2) draw a composed scrolling cloud over sun
            if (s_weather.weather_code == 2)
            {
                // composed animation: cloud scrolls horizontally over sun
                const uint32_t period_ms = 1500; // full left->right sweep
                uint32_t t = time_ms % period_ms;
                float progress = (float)t / (float)period_ms;     // 0..1
                int scroll_range = ICON_SIZE * 2 + ICON_SIZE / 2; // move from -ICON_SIZE..+ICON_SIZE
                int offset = (int)((-(ICON_SIZE + ICON_SIZE / 2)) + (progress * scroll_range));
                // Draw base sun (opaque) then cloud clipped to icon region (transparent)
                sprite_blit_opaque(is_night_now() ? sprite_moon_small : sprite_sun_small, ICON_SIZE, ICON_SIZE, icon_x, 0);
                sprite_blit_transparent_clipped(sprite_cloud, ICON_SIZE, ICON_SIZE, icon_x + offset, 0, icon_x, 0, ICON_SIZE, ROW_NUM);
            }
            else
            {
                icon_bitmap = get_weather_icon_bitmap(s_weather.weather_code, time_ms);
                // Draw icon opaque so it doesn't show through text gaps
                sprite_blit_opaque(icon_bitmap, ICON_SIZE, ICON_SIZE, icon_x, 0);
            }

            s_weather_text_id = gfx_draw_scrolling_text(gfx_handle, &font5x7, text_x, 0, temp_text, LEFT_REGION_WIDTH, s_weather_text_id);
        }
        else if (s_weather_text_id != GFX_SCROLLING_TEXT_ID_INVALID)
        {
            gfx_remove_scrolling_text_by_id(gfx_handle, s_weather_text_id);
            s_weather_text_id = GFX_SCROLLING_TEXT_ID_INVALID;
            compositor_clear_left_region();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
