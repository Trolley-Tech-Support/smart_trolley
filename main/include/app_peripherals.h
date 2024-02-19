#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_camera.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// camera pins
#define CAMERA_MODULE_NAME "ESP-S2-KALUGA"
#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 1
#define CAMERA_PIN_SIOD 8
#define CAMERA_PIN_SIOC 7

#define CAMERA_PIN_D7 38
#define CAMERA_PIN_D6 21
#define CAMERA_PIN_D5 40
#define CAMERA_PIN_D4 39
#define CAMERA_PIN_D3 42
#define CAMERA_PIN_D2 41
#define CAMERA_PIN_D1 37
#define CAMERA_PIN_D0 36
#define CAMERA_PIN_VSYNC 2
#define CAMERA_PIN_HREF 3
#define CAMERA_PIN_PCLK 33

#define XCLK_FREQ_HZ 10000000
#define CAMERA_PIXFORMAT PIXFORMAT_RGB565
#define CAMERA_FRAME_SIZE FRAMESIZE_240X240
#define CAMERA_FB_COUNT 2

#ifdef __cplusplus
extern "C"
{
#endif

    esp_err_t app_camera_init();
#ifdef __cplusplus
}
#endif