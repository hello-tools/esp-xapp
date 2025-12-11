idf_component_register(
    SRCS "xapp.c" "xlist.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_timer" "nvs_flash" "esp_event" "freertos")
