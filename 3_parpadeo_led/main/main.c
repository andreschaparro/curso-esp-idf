//=====[Libraries]=============================================================

#include <stdio.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_indicator.h"

//=====[Defines]===============================================================

#define INBUILT_LED_NUM GPIO_NUM_2
#define LED_ACTIVE_LEVEL true

//=====[Declaration of public data types]======================================

enum
{
    BLINK_FAST = 0,
    BLINK_MAX,
};

//=====[Declaration and initialization of public global constants]=============

static const char *TAG = "main";

static const blink_step_t fast_blink[] = {
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_LOOP, 0, 0},
};

//=====[Declaration and initialization of public global variables]=============

blink_step_t const *led_mode[] = {
    [BLINK_FAST] = fast_blink,
    [BLINK_MAX] = NULL,
};

static led_indicator_handle_t inbuilt_led_handle = NULL;

//=====[Declarations (prototypes) of public functions]=========================

void led_init(gpio_num_t gpio_num, led_indicator_handle_t *led_indicator_handle);

//=====[Main function, the program entry point after power on or reset]========

void app_main(void)
{
    led_init(INBUILT_LED_NUM, &inbuilt_led_handle);
    while (1)
    {
        for (uint8_t i = 0; i < BLINK_MAX; i++)
        {
            led_indicator_start(inbuilt_led_handle, i);
            ESP_LOGI(TAG, "start blink");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            led_indicator_stop(inbuilt_led_handle, i);
            ESP_LOGI(TAG, "stop blink");
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }
}

//=====[Implementations of public functions]===================================

void led_init(gpio_num_t gpio_num, led_indicator_handle_t *led_indicator_handle)
{
    led_indicator_gpio_config_t gpio_cfg = {
        .is_active_level_high = LED_ACTIVE_LEVEL,
        .gpio_num = (int32_t)gpio_num,
    };
    const led_indicator_config_t led_cfg = {
        .mode = LED_GPIO_MODE,
        .led_indicator_gpio_config = &gpio_cfg,
        .blink_lists = led_mode,
        .blink_list_num = BLINK_MAX,
    };
    *led_indicator_handle = led_indicator_create(&led_cfg);
    assert(*led_indicator_handle != NULL);
}