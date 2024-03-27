#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "inttypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/rmt.h"
#include "esp_code_scanner.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "esp_camera.h"
#include "esp_http_client.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lcd.h"
#include "board.h"
#include "nvs_flash.h"
#include "cJSON.h" 
#include "app_peripherals.h"
#include "fonts.h"
#include "led_strip.h"
#include "hx711.h"

#define EXAMPLE_ESP_WIFI_SSID      "VM6193248_2.4GHz"
#define EXAMPLE_ESP_WIFI_PASS      "bpvoj9gvuuTyfmzv"
//#define EXAMPLE_ESP_WIFI_SSID      "OnePlus7T"
//#define EXAMPLE_ESP_WIFI_PASS      "rahul122"

#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define CONFIG_EXAMPLE_RMT_TX_GPIO 45
#define CONFIG_EXAMPLE_STRIP_LED_NUMBER 1

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define IMAGE_MAX_SIZE (100 * 1024)/**< The maximum size of a single picture in the boot animation */
#define IMAGE_WIDTH    320 /*!< width of jpeg file */
#define IMAGE_HEIGHT    240 /*!< height of jpeg file */
#define portTICK_RATE_MS 10

#define MAX_NAME_LENGTH 64
#define MAX_PRICE_LENGTH 5

#define DEFAULT_VREF    1100                            /*!< Use adc2_vref_to_gpio() to obtain a better estimate */
#define NO_OF_SAMPLES   64
#define SAMPLE_TIME     200
#define DEVIATION 0.1

#define AVG_SAMPLES   10
#define GPIO_DATA   GPIO_NUM_19
#define GPIO_SCLK   GPIO_NUM_20


static const char *TAG = "SMART_TROLLEY";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static const adc_channel_t channel = ADC_CHANNEL_5;     /*!< PIO7 if ADC1, GPIO17 if ADC2 */
static const adc_bits_width_t width = ADC_BITWIDTH_13;

static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;
static QueueHandle_t adc_queue = NULL;

typedef struct {
    int product_id;
    char name[MAX_NAME_LENGTH];
    char price[MAX_PRICE_LENGTH];
    unsigned long weight;
} ProductInfo;

typedef struct {
    char *productName;
    double price;
    int product_id;
    int quantity;
} TableRow;

led_strip_t *strip;

/**
 * @brief rgb -> rgb565
 *
 * @param r red   (0~31)
 * @param g green (0~63)
 * @param b red   (0~31)
 *
 * @return data about color565
 */
uint16_t color565(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t color = ((r  << 11) | (g  << 6) | b);
    return (color << 8) | (color >> 8);
}

void esp_photo_display(void)
{
    ESP_LOGI(TAG, "LCD Working ........");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };

    /*!< Use settings defined above to initialize and mount SPIFFS filesystem. */
    /*!< Note: esp_vfs_spiffs_register is an all-in-one convenience function. */
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
    size_t total = 0, used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(NULL, &total, &used));

    uint8_t *rgb565 = malloc(IMAGE_WIDTH * IMAGE_HEIGHT * 2);
    if (NULL == rgb565) {
        ESP_LOGE(TAG, "can't alloc memory for rgb565 buffer");
        return;
    }
    uint8_t *buf = malloc(IMAGE_MAX_SIZE);
    if (NULL == buf) {
        free(rgb565);
        ESP_LOGE(TAG, "can't alloc memory for jpeg file buffer");
        return;
    }
    int read_bytes = 0;

    FILE *fd = fopen("/spiffs/image.jpg", "r");

    read_bytes = fread(buf, 1, IMAGE_MAX_SIZE, fd);
    ESP_LOGI(TAG, "spiffs:read_bytes:%d  fd: %p", read_bytes, fd);
    fclose(fd);

    jpg2rgb565(buf, read_bytes, rgb565, JPG_SCALE_NONE);
    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data(rgb565, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    free(buf);
    free(rgb565);
    vTaskDelay(2000 / portTICK_RATE_MS);
}

void esp_color_display_green(void)
{
    ESP_LOGI(TAG, "LCD Initiated");
    uint16_t *data_buf = (uint16_t *)heap_caps_calloc(IMAGE_WIDTH * IMAGE_HEIGHT, sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return; // Exit the function if allocation fails
    }
    
    uint16_t color = color565(0, 0, 0);

    for (int b = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(0, 0, b++);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    heap_caps_free(data_buf);
}

void esp_color_display_blue(void)
{
    ESP_LOGI(TAG, "LCD Initiated");
    uint16_t *data_buf = (uint16_t *)heap_caps_calloc(IMAGE_WIDTH * IMAGE_HEIGHT, sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return; // Exit the function if allocation fails
    }
    
    uint16_t color = color565(0, 0, 0);

    for (int r = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(r++, 0, 0);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    heap_caps_free(data_buf);
}

void esp_color_display_red(void)
{
    ESP_LOGI(TAG, "LCD Initiated");
    uint16_t *data_buf = (uint16_t *)heap_caps_calloc(IMAGE_WIDTH * IMAGE_HEIGHT, sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return; // Exit the function if allocation fails
    }
    
    uint16_t color = color565(0, 0, 0);

    for (int g = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(0, g++, 0);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    heap_caps_free(data_buf);
}


void esp_color_display(void)
{
    ESP_LOGI(TAG, "LCD Initiated");
    uint16_t *data_buf = (uint16_t *)heap_caps_calloc(IMAGE_WIDTH * IMAGE_HEIGHT, sizeof(uint16_t), MALLOC_CAP_SPIRAM);

    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return; // Exit the function if allocation fails
    }
    
    uint16_t color = color565(0, 0, 0);

    for (int r = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(r++, 0, 0);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    for (int g = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(0, g++, 0);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    for (int b = 0,  j = 0; j < IMAGE_HEIGHT; j++) {
        if (j % 8 == 0) {
            color = color565(0, 0, b++);
        }

        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color;
        }
    }

    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    vTaskDelay(2000 / portTICK_RATE_MS);

    heap_caps_free(data_buf);
}

void adc_init(void)
{
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

}

double adc_voltage_conversion(uint32_t adc_reading)
{
    double voltage = 0;

    voltage = (2.60 * adc_reading) / 8191;

    return voltage;
}

void button_task(void *arg)
{
    /*!<Continuously sample ADC1*/
    while (1) {
        uint32_t adc_reading = 0;
        double voltage = 0;

        /*!< Multisampling */
        for (int i = 0; i < NO_OF_SAMPLES; i++) {
            if (unit == ADC_UNIT_1) {
                adc_reading += adc1_get_raw((adc1_channel_t)channel);
            } else {
                int raw;
                adc2_get_raw((adc2_channel_t)channel, width, &raw);
                adc_reading += raw;
            }
        }

        adc_reading /= NO_OF_SAMPLES;

        voltage = adc_voltage_conversion(adc_reading);
        ESP_LOGD(TAG, "ADC%d CH%d Raw: %lu   ; Voltage: %0.2lfV", unit, channel, adc_reading, voltage);

        xQueueSend(adc_queue, (double *)&voltage, 0);
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_TIME));
    }

    vTaskDelete(NULL);
}

void led_task(void *arg)
{
    double voltage = 0;

    while (1) {
        xQueueReceive(adc_queue, &voltage, portMAX_DELAY);

        if (voltage > 2.6) {
            continue;
        } else if (voltage > 2.41 - DEVIATION  && voltage <= 2.41 + DEVIATION) {
            ESP_LOGI(TAG, "rec(K1) -> red");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 0, 0));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        } else if (voltage > 1.98 - DEVIATION && voltage <= 1.98 + DEVIATION) {
            ESP_LOGI(TAG, "mode(K2) -> green");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 0, 255, 0));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        } else if (voltage > 1.65 - DEVIATION && voltage <= 1.65 + DEVIATION) {
            ESP_LOGI(TAG, "play(K3) -> blue");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 0, 0, 255));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        } else if (voltage > 1.11 - DEVIATION && voltage <= 1.11 + DEVIATION) {
            ESP_LOGI(TAG, "set(K4) -> yellow");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 0));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        } else if (voltage > 0.82 - DEVIATION && voltage <= 0.82 + DEVIATION) {
            ESP_LOGI(TAG, "vol(K5) -> purple");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 0, 255));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        } else if (voltage > 0.38 - DEVIATION && voltage <= 0.38 + DEVIATION) {
            ESP_LOGI(TAG, "vol+(K6) -> white");
            ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 255));
            ESP_ERROR_CHECK(strip->refresh(strip, 0));
        }

    }

    vTaskDelete(NULL);

}

bool get_adc_remove() {
    double voltage = 0;
    xQueueReceive(adc_queue, &voltage, portMAX_DELAY);
    if (voltage > 2.41 - DEVIATION  && voltage <= 2.41 + DEVIATION) {
        ESP_LOGI(TAG, "LED(K1) -> red : Removing Item from cart!");
        ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 0, 0));
        ESP_ERROR_CHECK(strip->refresh(strip, 0));
        return true;
    } else {
        ESP_LOGI(TAG, "LED(K2) -> green : Adding Item to cart!");
        ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 0, 255, 0));
        ESP_ERROR_CHECK(strip->refresh(strip, 0));
    }
    return false;
}

esp_err_t example_rmt_init(uint8_t gpio_num, int led_number, uint8_t rmt_channel)
{
    ESP_LOGI(TAG, "Initializing RMT ...");
    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(gpio_num, rmt_channel);

    /*!< set counter clock to 40MHz */
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(led_number, (led_strip_dev_t)config.channel);
    strip = led_strip_new_rmt_ws2812(&strip_config);

    if (!strip) {
        ESP_LOGE(TAG, "install driver failed");
        return ESP_FAIL;
    }

    /*!< Clear LED strip (turn off all LEDs) */
    ESP_ERROR_CHECK(strip->clear(strip, 100));
    /*!< Show simple rainbow chasing pattern */

    return ESP_OK;
}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void interpret_json_response(char *response, ProductInfo *productInfo) {
    ESP_LOGI(TAG, "Received JSON response: %s", response);
    cJSON *root = cJSON_Parse(response);
    if (root) {
        cJSON *product_id = cJSON_GetObjectItem(root, "product_id");
        cJSON *name = cJSON_GetObjectItem(root, "name");
        cJSON *price = cJSON_GetObjectItem(root, "price");
        cJSON *weight = cJSON_GetObjectItem(root, "weight");

        if (product_id && product_id->type == cJSON_Number &&
            name && name->type == cJSON_String &&
            price && price->type == cJSON_String && 
            weight && weight->type == cJSON_Number) {

            productInfo->product_id = product_id->valueint;
            strncpy(productInfo->name, name->valuestring, MAX_NAME_LENGTH - 1);
            productInfo->name[MAX_NAME_LENGTH - 1] = '\0';

            strncpy(productInfo->price, price->valuestring, MAX_PRICE_LENGTH - 1);
            productInfo->price[MAX_PRICE_LENGTH - 1] = '\0';

            productInfo->weight = (unsigned long)weight->valuedouble;

            ESP_LOGI(TAG, "Product ID: %d", productInfo->product_id);
            ESP_LOGI(TAG, "Name: %s", productInfo->name);
            ESP_LOGI(TAG, "Price: %s", productInfo->price);
            ESP_LOGI(TAG, "Weight: %ld", productInfo->weight);
        } else {
            ESP_LOGE(TAG, "Failed to extract product information from JSON");
        }

        cJSON_Delete(root);
    } else {
        ESP_LOGE(TAG, "Failed to parse JSON response!");
    }
}


esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP GET EVENT DATA: %s", (char *)evt->data);
        break;
    
    default:
        break;
    }
    return ESP_OK;
}

static bool https_native_request(const char *qor_id, ProductInfo *product_info) {
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    int content_length = 0;

    extern const unsigned char client_cert_start[] asm("_binary_client_cert_pem_start");
    extern const unsigned char client_cert_end[] asm("_binary_client_cert_pem_end");
    size_t  client_cert_len = client_cert_end - client_cert_start;
    extern const unsigned char client_key_start[] asm("_binary_client_key_pem_start");
    extern const unsigned char client_key_end[] asm("_binary_client_key_pem_end");
    size_t  client_key_len = client_cert_end - client_cert_start;

    esp_tls_cfg_t tls_cfg = {
        .cacert_pem_buf = (const unsigned char *) client_cert_start,
        .cacert_bytes = client_cert_len,
    };

    esp_tls_t *tls = esp_tls_init();

    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
    }

    if (esp_tls_conn_http_new_sync("https://iot.api.pastav.com/item/details", &tls_cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
    }

    esp_http_client_config_t config = {
        .url = "https://iot.api.pastav.com/item/details",
        .method = HTTP_METHOD_POST,
        .event_handler = client_event_get_handler,
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        //.cert_pem = (char *)client_cert_start,
        //.cert_len = client_cert_len,
        //.client_cert_pem = (char *)client_cert_start,
        //.client_cert_len =  client_cert_len,
        //.client_key_pem = (char *)client_key_start,
        //.client_key_len = client_key_len,
        .skip_cert_common_name_check = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    char *post_data = (char *)malloc(MAX_HTTP_OUTPUT_BUFFER);
    if (post_data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for post_data");
        return false;
    }
    snprintf(post_data, MAX_HTTP_OUTPUT_BUFFER, "{\"col\": \"qr_identifier\", \"detail\": \"%s\"}", qor_id);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(post_data);
        esp_http_client_cleanup(client);
        return false;
    }

    int wlen = esp_http_client_write(client, post_data, strlen(post_data) + 1);
    if (wlen < 0) {
        ESP_LOGE(TAG, "Write failed");
        free(post_data);
        esp_http_client_cleanup(client);
        return false;
    }

    content_length = esp_http_client_fetch_headers(client);
    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP client fetch headers failed");
        free(post_data);
        esp_http_client_cleanup(client);
        return false;
    }

    int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
    if (data_read >= 0) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);
        interpret_json_response(output_buffer, product_info);
    } else {
        ESP_LOGE(TAG, "Failed to read response");
    }

    free(post_data);
    esp_http_client_cleanup(client);
    return true;
}

void lcd_initialize() {
    lcd_config_t lcd_config = {
#ifdef CONFIG_LCD_ST7789
        .clk_fre         = 80 * 1000 * 1000, /*!< ILI9341 Stable frequency configuration */
#endif
#ifdef CONFIG_LCD_ILI9341
        .clk_fre         = 40 * 1000 * 1000, /*!< ILI9341 Stable frequency configuration */
#endif
        .pin_clk         = LCD_CLK,
        .pin_mosi        = LCD_MOSI,
        .pin_dc          = LCD_DC,
        .pin_cs          = LCD_CS,
        .pin_rst         = LCD_RST,
        .pin_bk          = LCD_BK,
        .max_buffer_size = 2 * 1024,
        .horizontal      = 2, /*!< 2: UP, 3: DOWN */
        .swap_data       = 1,
    };

    lcd_init(&lcd_config);
}

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define FONT_WIDTH 8
#define CHAR_SPACING 4

void drawChar(uint16_t *data_buf, int x, int y, char character, uint16_t color) {
    // Convert to uppercase
    character = toupper(character);

    // Check for valid character range
    if (character >= 'A' && character <= 'Z') {
        int charIndex = character - 'A';
        for (int col = 0; col < CHAR_WIDTH; col++) {
            for (int row = 0; row < CHAR_HEIGHT; row++) {
                if ((fontData[charIndex][col] >> row) & 0x01) {
                    data_buf[(x + col) + IMAGE_WIDTH * (y + row)] = color;
                }
            }
        }
    }
}

void drawString(uint16_t *data_buf, int x, int y, const char *message, uint16_t color) {
    while (*message != '\0') {
        drawChar(data_buf, x, y, *message, color);
        x += CHAR_WIDTH + CHAR_SPACING; // Adjust spacing based on font and desired character separation
        message++;
    }
}

void display_table(TableRow **messageArray, int numRows, int numCols)
{
    ESP_LOGI(TAG, "LCD Initiated");
    int tableRows = 11;
    int tableCols = 2;
    int cellHeight = IMAGE_WIDTH / tableRows;

    int cellWidthRight  = IMAGE_HEIGHT / 3;
    int cellWidthLeft = IMAGE_HEIGHT - cellWidthRight; 

    // Allocate memory for color buffer (assuming 16-bit color)
    uint16_t *data_buf = (uint16_t *)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));

    if (data_buf == NULL) {
        // Handle memory allocation failure
        ESP_LOGE(TAG, "Failed to allocate memory for data_buf");
        return; // Or take appropriate action   
    }


    // Clear the buffer with white background
    for (int j = 0; j < IMAGE_HEIGHT; j++) {
        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color565(255, 255, 255);
        }
    }

    for (int j = 0; j < IMAGE_HEIGHT; j++) {
        for (int i = 0; i < IMAGE_WIDTH / tableRows; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color565(0, 0, 50);
        }
    }

    for (int j = 0; j < IMAGE_HEIGHT; j++) {
        for (int i = cellHeight * 10; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color565(0, 50, 0);
        }
    }

    // Draw table outline with cell padding and spacing
    for (int row = 0; row < tableRows; row++) {
        int x = row * cellHeight;
        for (int i = 0; i < IMAGE_WIDTH; i++) {
            if (row > 0) {
                // Draw horizontal lines for top and bottom of cells (except top row)
                data_buf[x + IMAGE_WIDTH * i] = color565(0, 0, 0);
            }
        }
    }

    for (int col = 0; col < tableCols; col++) {
        int y = col * cellWidthRight;
        for (int j = 0; j < IMAGE_WIDTH; j++) {
            if (col > 0) {
                // Draw vertical lines for left and right of cells (except leftmost column)
                data_buf[j + IMAGE_WIDTH * y] = color565(0, 0, 0);
            }
        }
    }

    for (int i = 0; i < IMAGE_WIDTH; i++) {
        data_buf[i] = color565(0, 0, 0);  // Top boundary
        data_buf[i + IMAGE_WIDTH * (IMAGE_HEIGHT - 1)] = color565(0, 0, 0);  // Bottom boundary
    }

    for (int j = 0; j < IMAGE_HEIGHT; j++) {
        data_buf[j * IMAGE_WIDTH] = color565(0, 0, 0);  // Left boundary
        data_buf[IMAGE_WIDTH - 1 + IMAGE_WIDTH * j] = color565(0, 0, 0);  // Right boundary
    }

    int x = 14.5;
    int y = 230;

    // Display messages in each cell in black
    for (int row = 0; row < numRows; row++) {
        const char *originalProductName = messageArray[row]->productName;
        double originalPrice = messageArray[row]->price;

        char quantityStr[5];
        snprintf(quantityStr, sizeof(quantityStr), "%d", messageArray[row]->quantity);
        
        char productPrice[20];
        if (originalPrice != 0.01) {
            if(messageArray[row]->product_id != 11) {
                snprintf(productPrice, sizeof(productPrice), "EUR %sX%.2f", quantityStr, originalPrice);
            } else {
                snprintf(productPrice, sizeof(productPrice), "EUR %.2f", originalPrice);
            }
        } else {
            snprintf(productPrice, sizeof(productPrice), "%s", "PRICE");
        }

        char productName[50];
        if (originalPrice != 0.01 && messageArray[row]->product_id != 11) {
            snprintf(productName, sizeof(productName), "%s Qty %s", originalProductName, quantityStr);
        } else {
            snprintf(productName, sizeof(productName), "%s", originalProductName);
        }

        for (size_t i = 0; i < strlen(productName); ++i) {
            char c = productName[i];
            if (c != ' ' && c != '\0') {
                const uint8_t* charData = getCharData(c);
                for (int chcol = 0; chcol < 5; chcol++) {
                    for (int chrow = 0; chrow < 7; chrow++) {
                        if ((charData[chcol] >> chrow) & 0x01) {
                            if((row == numRows-1)) { x = 300; }
                            data_buf[(x + chcol) + IMAGE_WIDTH * (y + chrow)] = color565(0, 0, 0); // Black color
                        }
                    }
                }
                y -= 6;
            } else {
                if(c == ' ') {
                    y-=6;
                }
            }
        }

        y=70;
        for (size_t i = 0; i < strlen(productPrice); ++i) {
            char c = productPrice[i];
            if (c != ' ' && c != '\0') {
                const uint8_t* charData = getCharData(c);
                for (int chcol = 0; chcol < 5; chcol++) {
                    for (int chrow = 0; chrow < 7; chrow++) {
                        if ((charData[chcol] >> chrow) & 0x01) {
                            if((row == numRows-1)) { x = 300; }
                            data_buf[(x + chcol) + IMAGE_WIDTH * (y + chrow)] = color565(0, 0, 0); // Black color
                        }
                    }
                }
                y -= 6;
            } else {
                if(c == ' ') {
                    y-=6;
                }
            }
        }
        x+=cellHeight;
        y=230;
    }

    // Display the color table
    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));
    heap_caps_free(data_buf);
    vTaskDelay(2000 / portTICK_RATE_MS);
}


void display_hello2() {
    // Allocate buffer for LCD data
    uint16_t *data_buf = (uint16_t *)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));

    // Check if memory allocation was successful
    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return; // Exit the function if allocation fails
    }

    // Clear buffer with background color
    for (int j = 0; j < IMAGE_HEIGHT; j++) {
        for (int i = 0; i < IMAGE_WIDTH; i++) {
            data_buf[i + IMAGE_WIDTH * j] = color565(255, 255, 255); // White background
        }
    }

    // Display "hello" using fontDataMap
    int x = 10;
    int y = 230;

    const char hello[] = "hello";
    for (size_t i = 0; i < sizeof(hello) - 1; ++i) {
        char c = hello[i];
        const uint8_t* charData = getCharData(c);
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if ((charData[col] >> row) & 0x01) {
                    data_buf[(x + col) + IMAGE_WIDTH * (y + row)] = color565(0, 0, 0); // Black color
                }
            }
        }
        y -= 6;
    }

    // Display the data on the LCD
    lcd_set_index(0, 0, IMAGE_WIDTH - 1, IMAGE_HEIGHT - 1);
    lcd_write_data((uint8_t *)data_buf, IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(uint16_t));

    // Free the allocated memory
    free(data_buf);
    vTaskDelay(2000 / portTICK_RATE_MS);
}

void addRow(TableRow ***table, size_t *rowCount, double *total, const char *productName, const char *price, int product_id) {

    for (size_t i = 0; i < *rowCount; i++) {
        if ((*table)[i]->product_id == product_id && product_id != 11 && product_id != 0) {
            // Product with the same product_id already exists, increment quantity
            (*table)[i]->quantity++;
            double price_as_double = strtod(price, NULL);
            *total+=price_as_double;
            return;
        }
    }

    *table = realloc(*table, (*rowCount + 1) * sizeof(TableRow *));
    if (*table == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    (*table)[*rowCount] = malloc(sizeof(TableRow));
    if ((*table)[*rowCount] == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    (*table)[*rowCount]->productName = strdup(productName);
    if ((*table)[*rowCount]->productName == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    (*table)[*rowCount]->price = strtod(price, NULL);

    if ((*table)[*rowCount]->price == 0.0 && errno == ERANGE) {
        perror("Error converting price to double");
        exit(EXIT_FAILURE);
    }

    if(product_id != 0 && product_id != 11){
        *total+=(*table)[*rowCount]->price;
    }

    (*table)[*rowCount]->product_id = product_id;
    (*table)[*rowCount]->quantity = 1;
    
    (*rowCount)++;
}

void removeRow(TableRow ***table, size_t *rowCount, double *total, int product_id) {
    for (size_t i = 0; i < *rowCount; i++) {
        if ((*table)[i]->product_id == product_id) {
            if((*table)[i]->quantity > 1 && product_id != 11 && product_id != 0){
                ((*table)[i]->quantity)--;
                *total -= ((*table)[i]->price);
                return;
            } else {
                
                if(product_id != 11 && product_id != 0){
                    *total -= ((*table)[i]->price);
                }

                free((*table)[i]->productName);
                free((*table)[i]);

                for (size_t j = i; j < (*rowCount) - 1; j++) {
                    (*table)[j] = (*table)[j + 1];
                }

                (*rowCount)--;

                *table = realloc(*table, *rowCount * sizeof(TableRow *));
                if (*rowCount > 0 && *table == NULL) {
                    perror("Memory allocation failed");
                    exit(EXIT_FAILURE);
                }

                return;
            }
        }
    }

    fprintf(stderr, "Product with product_id %d not found in the table\n", product_id);
    ESP_LOGI(TAG, "Item is not available in the cart!");
}

void payments_task(TableRow **table, int rowCount, double total)
{
    double voltage = 0;
    xQueueReceive(adc_queue, &voltage, portMAX_DELAY);
    if (voltage > 0.38 - DEVIATION && voltage <= 0.38 + DEVIATION) {
            
        ESP_LOGI(TAG, "LED(K6) -> yellow : Payments Task Initiating .....");
        ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 0));
        ESP_ERROR_CHECK(strip->refresh(strip, 0));
            
        esp_color_display_blue();

        ESP_LOGI(TAG, "Total payment to be done : %.2f", total);
        ESP_LOGI(TAG, "Payments yet to be implemented");
        display_table(table, rowCount, 2);
    }
}

static unsigned long weight_reading_task(void)
{
    unsigned long weight = 0;
    weight = hx711_get_units(AVG_SAMPLES);
    ESP_LOGI(TAG, "Weight Reading: %ld\n ", weight);
    return weight;
    //vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static void initialise_weight_sensor(void)
{
    ESP_LOGI(TAG, "Weight Sensor Initiated");
    hx711_init(GPIO_DATA,GPIO_SCLK,eGAIN_128);
    hx711_tare();
}

bool is_item_added_removed(unsigned long current_weight, unsigned long initial_weight, unsigned long product_weight, bool is_removed) {
    long weight;
    if (is_removed) {
        weight = (long)(initial_weight - product_weight) - (long)current_weight;
    } else {
        weight = (long)(initial_weight + product_weight) - (long)current_weight;
    }
    
    if (weight >= -10 && weight < 10) {
        return true;
    } else {
        if (abs(initial_weight - current_weight) > 10) {
            while (1) {
                double voltage = 0;
                xQueueReceive(adc_queue, &voltage, portMAX_DELAY);

                if (voltage > 1.65 - DEVIATION && voltage <= 1.65 + DEVIATION) {
                    ESP_LOGI(TAG, "LED(K3) -> Yellow: Progressing to further scan...");
                    ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 100));
                    ESP_ERROR_CHECK(strip->refresh(strip, 0));
                    return true;
                }

                ESP_LOGI(TAG, "Call Customer Assistance!!");
            }
        }
    }
    return false;
}


static void decode_task()
{   
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    initialise_weight_sensor();

    // Wait for the WiFi connection to be established
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    lcd_initialize();
    /*< Show a picture */
    // esp_photo_display();
    /*< RGB display */
    esp_color_display();

    if(ESP_OK != app_camera_init()) {
        vTaskDelete(NULL);
        return;
    }

    camera_fb_t *fb = NULL;
    int64_t time1, time2;
    
    TableRow **table = NULL;
    size_t rowCount = 0;
    double total = 0.00;
    unsigned long initial_weight = weight_reading_task();
    ESP_LOGI(TAG, "Initial Weight: %ld", initial_weight);
    
    ProductInfo product_info;

    addRow(&table, &rowCount, &total,"Products", "0.01", 0);
    addRow(&table, &rowCount, &total, "Total", "0.00", 11);
    display_table(table, rowCount, 2);

    ESP_ERROR_CHECK(example_rmt_init(CONFIG_EXAMPLE_RMT_TX_GPIO, CONFIG_EXAMPLE_STRIP_LED_NUMBER, RMT_CHANNEL_0));

    adc_queue = xQueueCreate(1, sizeof(double));
    adc_init();
    xTaskCreatePinnedToCore(&button_task, "button_task", 3 * 1024, NULL, 5, NULL, 0);
    
    ESP_LOGI(TAG, "LED -> white");
    ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 255));
    ESP_ERROR_CHECK(strip->refresh(strip, 0));

    while (1)
    {
        payments_task(table, rowCount, total);

        fb = esp_camera_fb_get();
        if(fb == NULL){
            ESP_LOGI(TAG, "Camera get failed\n");
            continue;
        }

        time1 = esp_timer_get_time();
        // Decode Progress
        esp_image_scanner_t *esp_scn = esp_code_scanner_create();
        esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, fb->width, fb->height};
        esp_code_scanner_set_config(esp_scn, config);
        int decoded_num = esp_code_scanner_scan_image(esp_scn, fb->buf);

        if(decoded_num){
            esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn);
            time2 = esp_timer_get_time();
            ESP_LOGI(TAG, "Decode time in %lld ms.", (time2 - time1) / 1000);
            ESP_LOGI(TAG, "Decoded %s symbol \"%s\"\n", result.type_name, result.data);
            
            bool success = https_native_request(result.data, &product_info);

            if (success) {
                const char *product_name = product_info.name;
                const char *product_price = product_info.price;
                int prodcut_id = product_info.product_id;
                unsigned long weight = product_info.weight;
                bool is_removed = false;

                while (1) {
                    if(get_adc_remove()){
                        ESP_LOGI(TAG, "Remove item from the cart ...");
                        is_removed = true;
                    } else {
                        ESP_LOGI(TAG, "Place item in the cart ...");
                    }
                    unsigned long current_weight = weight_reading_task();
                    if(is_item_added_removed(current_weight, initial_weight, weight, get_adc_remove())){
                        ESP_LOGI(TAG, "Item placed/removed in the cart!");
                        initial_weight = current_weight;
                        break;
                    }
                }

                if(is_removed) {
                    esp_color_display_red();
                    removeRow(&table, &rowCount, &total, 11);
                    removeRow(&table, &rowCount, &total, prodcut_id);
                } else {
                    esp_color_display_green();
                    removeRow(&table, &rowCount, &total, 11);
                    addRow(&table, &rowCount, &total, product_name, product_price, prodcut_id);
                }

                ESP_LOGI(TAG, "Displaying table...");
                char total_str[5];
                snprintf(total_str, sizeof(total_str), "%.2f", total);
                addRow(&table, &rowCount, &total, "Total", total_str, 11);
                display_table(table, rowCount, 2); 
            
                ESP_LOGI(TAG, "LED -> blue :  Details displayed on LCD!");
                ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 0, 0, 255));
                ESP_ERROR_CHECK(strip->refresh(strip, 0));

                vTaskDelay(1000 / portTICK_PERIOD_MS);
            
                ESP_LOGI(TAG, "LED -> white");
                ESP_ERROR_CHECK(strip->set_pixel(strip, 0, 255, 255, 255));
                ESP_ERROR_CHECK(strip->refresh(strip, 0));
            }
            
            esp_camera_return_all();
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            esp_camera_fb_return(fb);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_code_scanner_destroy(esp_scn);
        esp_camera_fb_return(fb);
    }
}


void app_main()
{
    xTaskCreatePinnedToCore(decode_task, TAG, 6 * 1024, NULL, 6, NULL, 0);
}
