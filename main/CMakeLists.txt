idf_component_register(SRCS "main.c" "display/font5x7.c" "display/font_slot.c" "display/gfx.c" "display/matrix.c" "vlt.c" "datetime.c"
                    REQUIRES driver esp_wifi nvs_flash esp_netif
                    INCLUDE_DIRS ".")
