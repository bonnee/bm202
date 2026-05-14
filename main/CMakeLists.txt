idf_component_register(SRCS "main.c" 
                             "carousel.c"
                             "display/font5x7.c" 
                             "display/font_slot.c" 
                             "display/gfx.c" 
                             "display/compositor.c"
                             "display/matrix.c"
                             "display/render_queue.c"
                             "vlt.c" 
                             "datetime.c"
                             "weather/weather.c" "display/sprite.c"
                             "airplanes/airplanes.c"
                    REQUIRES driver esp_wifi nvs_flash esp_netif esp_http_client json mbedtls
                    INCLUDE_DIRS ".")
