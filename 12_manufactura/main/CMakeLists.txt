idf_component_register(
    SRCS 
        "main.c"
        "led_driver/led_driver.c"
    INCLUDE_DIRS 
        "."
        "led_driver"
)

nvs_create_partition_image(mfg_nvs ../mfg_data.csv FLASH_IN_PROJECT)