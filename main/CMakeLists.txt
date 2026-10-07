file(GLOB BCM_SOURCES "*.c")

idf_component_register(
    SRCS ${BCM_SOURCES}
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_twai
        esp_driver_uart
        esp_driver_ledc
        esp_adc
        esp_timer
        nvs_flash
)

# O IDF 6.x marca o driver TWAI "clássico" como deprecated com #warning.
# Como o projeto trata warning como erro (-Werror=cpp), rebaixamos para warning.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=cpp)