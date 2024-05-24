//=====[Libraries]=============================================================

#include <stdio.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//=====[Defines]===============================================================

//=====[Declaration of public data types]======================================

//=====[Declaration and initialization of public global constants]=============

static const char *TAG = "main";

//=====[Declaration and initialization of public global variables]=============

//=====[Declarations (prototypes) of public functions]=========================

//=====[Main function, the program entry point after power on or reset]========

void app_main(void)
{
    uint32_t i = 0;
    while (1)
    {
        ESP_LOGI(TAG, "[%" PRIu32 "] Hola Mundo!", i);
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

//=====[Implementations of public functions]===================================
