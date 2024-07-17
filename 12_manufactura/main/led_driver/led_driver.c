//=====[Libraries]=============================================================

#include <stdio.h>

#include "esp_log.h"
#include "driver/gpio.h"

#include "led_driver.h"

//=====[Declaration of private defines]========================================

//=====[Declaration of private data types]=====================================

//=====[Declaration and initialization of public global constants]=============

//=====[Declaration and initialization of private global constants]============

static const char *TAG = "led_driver";

//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

//=====[Declaration and initialization of private global variables]============

//=====[Declarations (prototypes) of private functions]========================

//=====[Implementations of public functions]===================================

void led_driver_init(led_config_t *led_config, gpio_num_t gpio_num, led_status_t value)
{
    led_config->gpio_num = gpio_num;
    led_config->status = value;
    gpio_set_direction(led_config->gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(led_config->gpio_num, led_config->status);
    ESP_LOGI(TAG, "Led on GPIO %d started", led_config->gpio_num);
}

void led_driver_update(led_config_t *led_config, led_status_t value)
{
    if (led_config->status != value)
    {
        led_config->status = value;
        gpio_set_level(led_config->gpio_num, led_config->status);
        ESP_LOGI(TAG, "Led on GPIO %d %s", led_config->gpio_num, led_config->status ? "On" : "Off");
    }
}

//=====[Implementations of private functions]==================================