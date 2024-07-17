//=====[Libraries]=============================================================

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "mqtt_client.h"
#include "esp_random.h"
#include "esp_https_ota.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "qrcode.h"
#include "iot_button.h"

#include "led_driver.h"

//=====[Defines]===============================================================

#define SERVICE_NAME_PREFIX "PROV_"
#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

#define BOOT_BUTTON_NUM GPIO_NUM_0
#define BUTTON_ACTIVE_LEVEL 0

//=====[Declaration of public data types]======================================

//=====[Declaration and initialization of public global constants]=============

static const char *TAG = "main";

static const EventBits_t WIFI_CONNECTED_EVENT = BIT0;

static const char *button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
};

extern const uint8_t ca_crt_start[] asm("_binary_ca_crt_start");
extern const uint8_t ca_crt_end[] asm("_binary_ca_crt_end");
extern const uint8_t client_crt_start[] asm("_binary_client_crt_start");
extern const uint8_t client_crt_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_end[] asm("_binary_client_key_end");

static const EventBits_t OTA_START_BIT = BIT0;

//=====[Declaration and initialization of public global variables]=============

static esp_mqtt_client_handle_t client;

static EventGroupHandle_t wifi_event_group;

static char *serial_no;

static led_config_t inbuilt_led;

static EventGroupHandle_t s_ota_event_group;

//=====[Declarations (prototypes) of public functions]=========================

void init_nvs(void);
void wifi_init_sta(void);
void wifi_init_prov(void);
void get_device_service_name(char *service_name, size_t max);
void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void wifi_prov_print_qr(const char *name, const char *pop, const char *transport);
void button_init(gpio_num_t gpio_num);
void button_event_cb(void *arg, void *data);
void mqtt_app_start(void);
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void init_mfg_nvs(void);
esp_err_t calloc_and_read_from_nvs(nvs_handle handle, const char *key, char **value);
void ota_task(void *pvParameter);
esp_err_t http_event_handler(esp_http_client_event_t *evt);

//=====[Main function, the program entry point after power on or reset]========

void app_main(void)
{
    button_init(BOOT_BUTTON_NUM);
    led_driver_init(&inbuilt_led, INBUILT_LED, LED_OFF);
    init_nvs();
    init_mfg_nvs();
    wifi_init_sta();
    wifi_init_prov();
    s_ota_event_group = xEventGroupCreate();
    configASSERT(s_ota_event_group != NULL);
    BaseType_t ret;
    ret = xTaskCreatePinnedToCore(
        ota_task,
        "Tarea OTA",
        8192,
        NULL,
        (tskIDLE_PRIORITY + 5U),
        NULL,
        PRO_CPU_NUM);
    configASSERT(ret == pdPASS);
    mqtt_app_start();
    uint32_t temp;
    char buff[10];
    int msg_id;
    while (1)
    {
        temp = (esp_random() % 9) + 18;
        snprintf(buff, sizeof(buff), "%" PRIu32, temp);
        msg_id = esp_mqtt_client_enqueue(client, "/temperature", buff, 0, 1, 0, true);
        ESP_LOGI(TAG, "Enqueued msg_id=%d", msg_id);
        vTaskDelay(15000 / portTICK_PERIOD_MS);
    }
}

//=====[Implementations of public functions]===================================

void init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();
    configASSERT(wifi_event_group != NULL);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

void wifi_init_prov(void)
{
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    if (!provisioned)
    {
        ESP_LOGI(TAG, "Starting provisioning");
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
        const char *pop = "abcd1234";
        wifi_prov_security1_params_t *sec_params = pop;
        uint8_t custom_service_uuid[] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02};
        ESP_ERROR_CHECK(wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid));
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, NULL));
        wifi_prov_print_qr(service_name, pop, PROV_TRANSPORT_BLE);
    }
    else
    {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, pdTRUE, pdTRUE, portMAX_DELAY);
}

void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, eth_mac));
    snprintf(service_name, max, "%s%02X%02X%02X", SERVICE_NAME_PREFIX, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT)
    {
        switch (event_id)
        {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;
        case WIFI_PROV_CRED_RECV:
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received Wi-Fi credentials\n\tSSID     : %s\n\tPassword : %s",
                     (const char *)wifi_sta_cfg->ssid, (const char *)wifi_sta_cfg->password);
            break;
        case WIFI_PROV_CRED_FAIL:
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG, "Provisioning failed!\n\tReason : %s\n\tPlease reset to factory and retry provisioning",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            break;
        case WIFI_PROV_END:
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Station started");
        esp_wifi_connect(); // no usar la macro ESP_ERROR_CHECK
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect(); // no usar la macro ESP_ERROR_CHECK
    }
}

void wifi_prov_print_qr(const char *name, const char *pop, const char *transport)
{
    if (!name || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = {0};
    if (pop)
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}", PROV_QR_VERSION, name, pop, transport);
    }
    else
    {
        snprintf(payload, sizeof(payload), "{\"ver\":\"%s\",\"name\":\"%s\",\"transport\":\"%s\"}", PROV_QR_VERSION, name, transport);
    }
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_qrcode_generate(&cfg, payload));
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL, payload);
}

void button_init(gpio_num_t gpio_num)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = (int32_t)gpio_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn != NULL);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb, (void *)BUTTON_PRESS_DOWN);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, (void *)BUTTON_PRESS_UP);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, (void *)BUTTON_PRESS_REPEAT);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, (void *)BUTTON_PRESS_REPEAT_DONE);
    err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK);
    err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, (void *)BUTTON_DOUBLE_CLICK);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_LONG_PRESS_START);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, (void *)BUTTON_LONG_PRESS_HOLD);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, (void *)BUTTON_LONG_PRESS_UP);
    ESP_ERROR_CHECK(err);
}

void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    switch ((button_event_t)data)
    {
    case BUTTON_PRESS_DOWN:
        break;
    case BUTTON_PRESS_UP:
        break;
    case BUTTON_PRESS_REPEAT:
        break;
    case BUTTON_PRESS_REPEAT_DONE:
        break;
    case BUTTON_SINGLE_CLICK:
        break;
    case BUTTON_DOUBLE_CLICK:
        break;
    case BUTTON_LONG_PRESS_START:
        break;
    case BUTTON_LONG_PRESS_HOLD:
        break;
    case BUTTON_LONG_PRESS_UP:
        ESP_LOGI(TAG, "Reset provisioned status of the device!");
        ESP_ERROR_CHECK(wifi_prov_mgr_reset_provisioning()); // Provisioned status is determined by the Wi-Fi STA configuration, saved on the NVS
        esp_restart();
        break;
    default:
        break;
    }
}

void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.hostname = "raspberrypi.local",
        .broker.address.transport = MQTT_TRANSPORT_OVER_SSL,
        .broker.address.port = 8883,
        .broker.verification.certificate = (const char *)ca_crt_start,
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_crt_start,
                .key = (const char *)client_key_start,
            },
        }};
    client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/led", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        msg_id = esp_mqtt_client_subscribe(client, "/update", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC = %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA = %.*s", event->data_len, event->data);
        if (strncmp(event->topic, "/led", event->topic_len) == 0)
        {
            if (strncmp(event->data, "on", event->data_len) == 0)
            {
                led_driver_update(&inbuilt_led, LED_ON);
            }
            else if (strncmp(event->data, "off", event->data_len) == 0)
            {
                led_driver_update(&inbuilt_led, LED_OFF);
            }
        }
        else if (strncmp(event->topic, "/update", event->topic_len) == 0)
        {
            xEventGroupSetBits(s_ota_event_group, OTA_START_BIT);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            if (event->error_handle->esp_tls_last_esp_err != 0)
            {
                ESP_LOGE(TAG, "Last error %s: 0x%x", "reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            }
            if (event->error_handle->esp_tls_stack_err != 0)
            {
                ESP_LOGE(TAG, "Last error %s: 0x%x", "reported from tls stack", event->error_handle->esp_tls_stack_err);
            }
            if (event->error_handle->esp_transport_sock_errno != 0)
            {
                ESP_LOGE(TAG, "Last error %s: 0x%x", "captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            }
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void init_mfg_nvs(void)
{
    ESP_ERROR_CHECK(nvs_flash_init_partition("mfg_nvs"));
    ESP_LOGI(TAG, "Opening mfg_nvs handle");
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open_from_partition("mfg_nvs", "mfg_data", NVS_READONLY, &my_handle));
    ESP_LOGI(TAG, "The mfg_nvs handle successfully opened");
    ESP_LOGI(TAG, "Reading values from mfg_nvs");
    ESP_ERROR_CHECK(calloc_and_read_from_nvs(my_handle, "serial_no", &serial_no));
}

esp_err_t calloc_and_read_from_nvs(nvs_handle handle, const char *key, char **value)
{
    size_t required_size = 0;
    ESP_ERROR_CHECK(nvs_get_str(handle, key, NULL, &required_size));
    *value = calloc(sizeof(char), required_size);
    if (*value != NULL)
    {
        ESP_ERROR_CHECK(nvs_get_str(handle, key, *value, &required_size));
        ESP_LOGI(TAG, "Read key: %s, value: %s", key, *value);
        return ESP_OK;
    }
    return ESP_FAIL;
}

void ota_task(void *pvParameter)
{
    for (;;)
    {
        EventBits_t bits = xEventGroupWaitBits(s_ota_event_group,
                                               OTA_START_BIT,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);
        if (bits & OTA_START_BIT)
        {
            ESP_LOGI(TAG, "Starting OTA");
            esp_http_client_config_t config = {
                .url = "https://raspberrypi.local:8443/updates/1_hola_mundo.bin",
                .cert_pem = (const char *)ca_crt_start,
                .client_cert_pem = (const char *)client_crt_start,
                .client_key_pem = (const char *)client_key_start,
                .event_handler = http_event_handler,
                .keep_alive_enable = true,
            };
            esp_https_ota_config_t ota_config = {
                .http_config = &config,
            };
            ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
            esp_err_t ret = esp_https_ota(&ota_config);
            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
                esp_restart();
            }
            else
            {
                ESP_LOGE(TAG, "Firmware upgrade failed");
            }
        }
        else
        {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
}

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}