idf_component_register(SRCS "cmd_router.c" "multi_ap.c"
                    INCLUDE_DIRS .
                    REQUIRES console nvs_flash driver spi_flash esp_wifi)
