#include "airplanes.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "carousel.h"
#include "display/compositor.h"
#include "display/font5x7.h"
#include "display/gfx.h"

#include "location.h"

static const char *TAG = "AIRPLANES";

#define AIRPLANES_LAT LAT
#define AIRPLANES_LON LON
#define AIRPLANES_REFRESH_MS (20 * 1000)
#define AIRPLANES_REFRESH_FAST_MS (5 * 1000)
#define AIRPLANES_CLOSEST_RADIUS_NM 7

typedef struct
{
    char callsign[16];
    char type[16];
    char dep[8];
    char dst[8];
    int distance_km;
    double lat;
    double lon;
    uint8_t has_position;
    uint8_t has_route;
    uint8_t valid;
} airplane_data_t;

static airplane_data_t s_airplane = {0};
static gfx_scrolling_text_id_t s_airplane_text_id = GFX_SCROLLING_TEXT_ID_INVALID;

bool airplanes_has_nearby(void)
{
    return s_airplane.valid != 0;
}

typedef struct
{
    char location[256];
    bool saw_location;
} http_response_capture_t;

static void trim_copy(char *dst, size_t dst_size, const char *src)
{
    size_t start;
    size_t end;
    size_t out_len;

    if (!dst || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (!src)
    {
        return;
    }

    start = 0;
    while (src[start] != '\0' && isspace((unsigned char)src[start]))
    {
        start++;
    }

    end = strlen(src);
    while (end > start && isspace((unsigned char)src[end - 1]))
    {
        end--;
    }

    out_len = end - start;
    if (out_len >= dst_size)
    {
        out_len = dst_size - 1;
    }

    memcpy(dst, src + start, out_len);
    dst[out_len] = '\0';
}

static void sanitize_airport_code(char *dst, size_t dst_size, const char *src)
{
    size_t i;
    size_t j;

    if (!dst || dst_size == 0)
    {
        return;
    }

    dst[0] = '\0';
    if (!src)
    {
        return;
    }

    j = 0;
    for (i = 0; src[i] != '\0' && j + 1 < dst_size; i++)
    {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c))
        {
            dst[j++] = (char)toupper(c);
        }
    }
    dst[j] = '\0';
}

static void parse_route_pair(char *dst_a, size_t dst_a_size, char *dst_b, size_t dst_b_size, const char *value)
{
    const char *separator;
    char first[32];
    size_t first_len;

    if (!dst_a || !dst_b || dst_a_size == 0 || dst_b_size == 0)
    {
        return;
    }

    dst_a[0] = '\0';
    dst_b[0] = '\0';

    if (!value)
    {
        return;
    }

    separator = strchr(value, '-');
    if (!separator)
    {
        sanitize_airport_code(dst_a, dst_a_size, value);
        return;
    }

    first_len = (size_t)(separator - value);
    if (first_len >= sizeof(first))
    {
        first_len = sizeof(first) - 1;
    }

    memcpy(first, value, first_len);
    first[first_len] = '\0';

    sanitize_airport_code(dst_a, dst_a_size, first);
    sanitize_airport_code(dst_b, dst_b_size, separator + 1);

    if (dst_a[0] == '\0' || dst_b[0] == '\0')
    {
        trim_copy(dst_a, dst_a_size, first);
        trim_copy(dst_b, dst_b_size, separator + 1);
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_capture_t *capture;

    if (!evt)
    {
        return ESP_OK;
    }

    if (evt->event_id == HTTP_EVENT_ON_HEADER && evt->header_key && evt->header_value)
    {
        capture = (http_response_capture_t *)evt->user_data;
        if (capture && strcasecmp(evt->header_key, "location") == 0)
        {
            trim_copy(capture->location, sizeof(capture->location), evt->header_value);
            capture->saw_location = capture->location[0] != '\0';
        }
    }

    return ESP_OK;
}

static bool http_get_json(const char *url, char *buf, size_t buf_size)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client;
    int status;
    int total_read;
    esp_err_t err;

    if (!url || !buf || buf_size < 2)
    {
        return false;
    }

    client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGW(TAG, "Failed to init HTTP client");
        return false;
    }

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }

    esp_http_client_fetch_headers(client);
    status = esp_http_client_get_status_code(client);
    if (status != 200)
    {
        ESP_LOGW(TAG, "HTTP status not OK: %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return false;
    }

    total_read = esp_http_client_read(client, buf, (int)buf_size - 1);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (total_read <= 0)
    {
        ESP_LOGW(TAG, "HTTP read failed: %d", total_read);
        return false;
    }

    buf[total_read] = '\0';
    return true;
}

static bool http_get_json_logged(const char *url, char *buf, size_t buf_size, int *status_out)
{
    char current_url[256];
    int redirect_hops = 0;

    if (!url || !buf || buf_size < 2)
    {
        return false;
    }

    strncpy(current_url, url, sizeof(current_url) - 1);
    current_url[sizeof(current_url) - 1] = '\0';

    for (;;)
    {
        http_response_capture_t capture = {0};
        esp_http_client_config_t config = {
            .url = current_url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 8000,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .disable_auto_redirect = true,
            .max_redirection_count = 0,
            .event_handler = http_event_handler,
            .user_data = &capture,
        };
        esp_http_client_handle_t client;
        int status;
        int total_read;
        esp_err_t err;

        client = esp_http_client_init(&config);
        if (!client)
        {
            ESP_LOGW(TAG, "Failed to init HTTP client for GET %s", current_url);
            return false;
        }

        ESP_LOGI(TAG, "GET %s", current_url);

        err = esp_http_client_open(client, 0);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "HTTP GET open failed for %s: %s", current_url, esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return false;
        }

        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (status_out)
        {
            *status_out = status;
        }

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308)
        {
            if (!capture.saw_location)
            {
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return false;
            }

            strncpy(current_url, capture.location, sizeof(current_url) - 1);
            current_url[sizeof(current_url) - 1] = '\0';
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            redirect_hops++;
            if (redirect_hops > 5)
            {
                return false;
            }
            continue;
        }

        if (status < 200 || status >= 300)
        {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return false;
        }

        total_read = esp_http_client_read(client, buf, (int)buf_size - 1);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        if (total_read <= 0)
        {
            return false;
        }

        buf[total_read] = '\0';
        return true;
    }
}

static double deg2rad(double deg)
{
    return deg * 0.017453292519943295;
}

static int haversine_km(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);
    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    double dist = 6371.0 * c;

    if (dist < 0.0)
    {
        return 0;
    }

    return (int)(dist + 0.5);
}

static bool try_get_string(cJSON *obj, const char *key, char *dst, size_t dst_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring)
    {
        trim_copy(dst, dst_size, item->valuestring);
        return dst[0] != '\0';
    }
    return false;
}

static bool parse_route_response(const char *json, airplane_data_t *airplane)
{
    cJSON *root;
    cJSON *airport_codes;
    cJSON *airport_codes_iata;
    cJSON *airports;

    if (!json || !airplane)
    {
        return false;
    }

    root = cJSON_Parse(json);
    if (!root)
    {
        return false;
    }

    airport_codes = cJSON_GetObjectItemCaseSensitive(root, "airport_codes");
    airport_codes_iata = cJSON_GetObjectItemCaseSensitive(root, "_airport_codes_iata");
    airports = cJSON_GetObjectItemCaseSensitive(root, "_airports");

    if (cJSON_IsString(airport_codes_iata) && airport_codes_iata->valuestring && airport_codes_iata->valuestring[0] != '\0')
    {
        parse_route_pair(airplane->dep, sizeof(airplane->dep), airplane->dst, sizeof(airplane->dst), airport_codes_iata->valuestring);
        airplane->has_route = airplane->dep[0] != '\0' && airplane->dst[0] != '\0';
    }

    if ((!airplane->has_route || airplane->dep[0] == '\0' || airplane->dst[0] == '\0') && airport_codes && cJSON_IsString(airport_codes) && airport_codes->valuestring && airport_codes->valuestring[0] != '\0')
    {
        parse_route_pair(airplane->dep, sizeof(airplane->dep), airplane->dst, sizeof(airplane->dst), airport_codes->valuestring);
        airplane->has_route = airplane->dep[0] != '\0' && airplane->dst[0] != '\0';
    }

    if ((!airplane->has_route || airplane->dep[0] == '\0' || airplane->dst[0] == '\0') && cJSON_IsArray(airports) && cJSON_GetArraySize(airports) >= 2)
    {
        cJSON *from = cJSON_GetArrayItem(airports, 0);
        cJSON *to = cJSON_GetArrayItem(airports, 1);
        cJSON *from_iata = from ? cJSON_GetObjectItemCaseSensitive(from, "iata") : NULL;
        cJSON *to_iata = to ? cJSON_GetObjectItemCaseSensitive(to, "iata") : NULL;
        cJSON *from_icao = from ? cJSON_GetObjectItemCaseSensitive(from, "icao") : NULL;
        cJSON *to_icao = to ? cJSON_GetObjectItemCaseSensitive(to, "icao") : NULL;
        const char *from_code = (cJSON_IsString(from_iata) && from_iata->valuestring && from_iata->valuestring[0] != '\0') ? from_iata->valuestring : (cJSON_IsString(from_icao) ? from_icao->valuestring : NULL);
        const char *to_code = (cJSON_IsString(to_iata) && to_iata->valuestring && to_iata->valuestring[0] != '\0') ? to_iata->valuestring : (cJSON_IsString(to_icao) ? to_icao->valuestring : NULL);

        if (from_code && to_code)
        {
            trim_copy(airplane->dep, sizeof(airplane->dep), from_code);
            trim_copy(airplane->dst, sizeof(airplane->dst), to_code);
            airplane->has_route = airplane->dep[0] != '\0' && airplane->dst[0] != '\0';
        }
    }

    cJSON_Delete(root);
    return airplane->has_route;
}

static bool parse_closest_aircraft(const char *json, airplane_data_t *out)
{
    cJSON *root;
    cJSON *ac;
    cJSON *plane = NULL;
    cJSON *item = NULL;
    char callsign[16] = {0};
    char type[16] = {0};
    cJSON *lat;
    cJSON *lon;

    if (!json || !out)
    {
        return false;
    }

    root = cJSON_Parse(json);
    if (!root)
    {
        ESP_LOGW(TAG, "Failed to parse closest-aircraft JSON");
        return false;
    }

    ac = cJSON_GetObjectItemCaseSensitive(root, "ac");
    if (!cJSON_IsArray(ac))
    {
        cJSON_Delete(root);
        return false;
    }

    cJSON_ArrayForEach(item, ac)
    {
        lat = cJSON_GetObjectItemCaseSensitive(item, "lat");
        lon = cJSON_GetObjectItemCaseSensitive(item, "lon");
        if (cJSON_IsNumber(lat) && cJSON_IsNumber(lon))
        {
            plane = item;
            break;
        }
    }

    if (!plane)
    {
        cJSON_Delete(root);
        return false;
    }

    try_get_string(plane, "flight", callsign, sizeof(callsign));
    if (callsign[0] == '\0')
    {
        try_get_string(plane, "hex", callsign, sizeof(callsign));
    }

    try_get_string(plane, "t", type, sizeof(type));
    if (type[0] == '\0')
    {
        try_get_string(plane, "type", type, sizeof(type));
    }

    lat = cJSON_GetObjectItemCaseSensitive(plane, "lat");
    lon = cJSON_GetObjectItemCaseSensitive(plane, "lon");
    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon))
    {
        cJSON_Delete(root);
        return false;
    }

    memset(out, 0, sizeof(*out));
    trim_copy(out->callsign, sizeof(out->callsign), callsign[0] ? callsign : "N/A");
    trim_copy(out->type, sizeof(out->type), type[0] ? type : "ACFT");
    out->lat = lat->valuedouble;
    out->lon = lon->valuedouble;
    out->has_position = 1;
    out->distance_km = haversine_km(AIRPLANES_LAT, AIRPLANES_LON, out->lat, out->lon);
    out->valid = 1;

    cJSON_Delete(root);
    return true;
}

static bool fetch_route_info(airplane_data_t *airplane)
{
    char route_url[192];
    char response[2048] = {0};
    int route_status = 0;

    if (!airplane || !airplane->has_position)
    {
        return false;
    }

    snprintf(route_url,
             sizeof(route_url),
             "https://api.adsb.lol/api/0/route/%s",
             airplane->callsign);

    if (!http_get_json_logged(route_url, response, sizeof(response), &route_status))
    {
        ESP_LOGW(TAG, "Route lookup failed url=%s status=%d callsign=%s", route_url, route_status, airplane->callsign);
        return false;
    }

    if (!parse_route_response(response, airplane))
    {
        ESP_LOGW(TAG,
                 "No route found url=%s status=%d body_size=%u callsign=%s",
                 route_url,
                 route_status,
                 (unsigned int)strlen(response),
                 airplane->callsign);
        return false;
    }

    return true;
}

static void fetch_airplanes(void)
{
    char url[192];
    char response[4096] = {0};
    airplane_data_t tmp;

    snprintf(url,
             sizeof(url),
             "https://api.adsb.lol/v2/closest/%.6f/%.6f/%d",
             AIRPLANES_LAT,
             AIRPLANES_LON,
             AIRPLANES_CLOSEST_RADIUS_NM);

    if (!http_get_json(url, response, sizeof(response)))
    {
        s_airplane.valid = 0;
        ESP_LOGW(TAG, "Failed to fetch closest aircraft");
        return;
    }

    if (!parse_closest_aircraft(response, &tmp))
    {
        s_airplane.valid = 0;
        ESP_LOGW(TAG, "No valid aircraft in closest response");
        return;
    }

    fetch_route_info(&tmp);
    s_airplane = tmp;

    ESP_LOGI(TAG,
             "Nearest: callsign=%s type=%s dist=%dkm route=%s (%s->%s)",
             s_airplane.callsign,
             s_airplane.type,
             s_airplane.distance_km,
             s_airplane.has_route ? "yes" : "no",
             s_airplane.dep,
             s_airplane.dst);
}

static void format_airplane_text(const airplane_data_t *airplane, char *out, size_t out_size)
{
    if (!airplane || !out || out_size == 0)
    {
        return;
    }

    if (!airplane->valid)
    {
        snprintf(out, out_size, "NO AIRCRAFT");
        return;
    }

    if (airplane->has_route)
    {
        snprintf(out,
                 out_size,
                 "%s %s-%s %dkm",
                 airplane->type[0] ? airplane->type : "ACFT",
                 airplane->dep,
                 airplane->dst,
                 airplane->distance_km);
        return;
    }

    snprintf(out,
             out_size,
             "%s %dkm",
             airplane->callsign[0] ? airplane->callsign : "N/A",
             airplane->distance_km);
}

void airplanes_fetch_task(void *params)
{
    for (;;)
    {
        fetch_airplanes();
        vTaskDelay(pdMS_TO_TICKS(s_airplane.valid ? AIRPLANES_REFRESH_FAST_MS : AIRPLANES_REFRESH_MS));
    }
}

void airplanes_task(void *params)
{
    char display_text[96];

    xTaskCreate(airplanes_fetch_task, "airplanes_fetch", 16384, NULL, tskIDLE_PRIORITY + 1, NULL);

    ESP_LOGI(TAG, "started");
    for (;;)
    {
        if (carousel_get_item() == CAROUSEL_ITEM_AIRPLANES)
        {
            format_airplane_text(&s_airplane, display_text, sizeof(display_text));
            s_airplane_text_id = gfx_draw_scrolling_text(gfx_handle, &font5x7, LEFT_REGION_X, 0, display_text, LEFT_REGION_WIDTH, s_airplane_text_id);
        }
        else
        {
            if (s_airplane_text_id != GFX_SCROLLING_TEXT_ID_INVALID)
            {
                gfx_remove_scrolling_text_by_id(gfx_handle, s_airplane_text_id);
                s_airplane_text_id = GFX_SCROLLING_TEXT_ID_INVALID;
                compositor_clear_left_region();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
