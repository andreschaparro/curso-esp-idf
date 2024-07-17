//=====[#include guards - begin]===============================================

#pragma once

//=====[Libraries]=============================================================
#include "driver/gpio.h"

//=====[Declaration of public defines]=========================================

#define INBUILT_LED GPIO_NUM_2

//=====[Declaration of public data types]======================================

typedef enum
{
    LED_OFF = 0,
    LED_ON = 1,
} led_status_t;

typedef struct led_config
{
    gpio_num_t gpio_num;
    led_status_t status;
} led_config_t;

//=====[Declarations (prototypes) of public functions]=========================

void led_driver_init(led_config_t *led_config, gpio_num_t gpio_num, led_status_t value);

void led_driver_update(led_config_t *led_config, led_status_t value);

//=====[#include guards - end]=================================================