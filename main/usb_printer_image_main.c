#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "driver/gpio.h"

static const char *TAG = "USB_PRINTER_IMG";

#define PRINTER_VID 0x1504
#define PRINTER_PID 0x006e
#define PRINTER_IFACE 0

extern const uint8_t logo_pbm_start[] asm("_binary_logo_pbm_start");
extern const uint8_t logo_pbm_end[] asm("_binary_logo_pbm_end");

static cdc_acm_dev_hdl_t cdc_dev = NULL;

static void usb_lib_task(void *arg)
{
    while (1) {
        usb_host_lib_handle_events(100, NULL);
    }
    vTaskDelete(NULL);
}

static void print_text(const char *text)
{
    if (!cdc_dev) {
        ESP_LOGE(TAG, "Printer not connected");
        return;
    }

    uint8_t init[] = {0x1B, 0x40};
    uint8_t center[] = {0x1B, 0x61, 0x01};
    uint8_t newline[] = {0x0A};
    uint8_t cut[] = {0x1D, 0x56, 0x41, 0x00};

    cdc_acm_host_data_tx_blocking(cdc_dev, init, sizeof(init), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, center, sizeof(center), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, (uint8_t *)text, strlen(text), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, newline, sizeof(newline), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, cut, sizeof(cut), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Text printed: %s", text);
}

static void parse_pbm_and_print(void)
{
    if (!cdc_dev) {
        ESP_LOGE(TAG, "Printer not connected");
        return;
    }

    const uint8_t *pbm_data = logo_pbm_start;
    size_t pbm_size = logo_pbm_end - logo_pbm_start;

    ESP_LOGI(TAG, "PBM size: %zu bytes", pbm_size);

    uint8_t *p = (uint8_t *)pbm_data;
    uint8_t *end = (uint8_t *)pbm_data + pbm_size;

    if (p[0] != 'P' || p[1] != '4') {
        ESP_LOGE(TAG, "Invalid PBM header");
        return;
    }
    p += 2;

    uint32_t width = 0, height = 0;
    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;

    while (p < end && *p >= '0' && *p <= '9') {
        width = width * 10 + (*p - '0');
        p++;
    }

    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;

    while (p < end && *p >= '0' && *p <= '9') {
        height = height * 10 + (*p - '0');
        p++;
    }

    while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;

    ESP_LOGI(TAG, "PBM: %" PRIu32 " x %" PRIu32 " pixels", width, height);

    uint32_t bytes_per_row = (width + 7) / 8;
    uint32_t pixel_data_size = bytes_per_row * height;

    ESP_LOGI(TAG, "Pixel data: %" PRIu32 " bytes (row=%" PRIu32 " bytes)", pixel_data_size, bytes_per_row);

    uint8_t *pixel_data = (uint8_t *)p;

    uint8_t init[] = {0x1B, 0x40};
    uint8_t center[] = {0x1B, 0x61, 0x01};

    cdc_acm_host_data_tx_blocking(cdc_dev, init, sizeof(init), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, center, sizeof(center), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint16_t xL = bytes_per_row & 0xFF;
    uint16_t xH = (bytes_per_row >> 8) & 0xFF;
    uint16_t yL = height & 0xFF;
    uint16_t yH = (height >> 8) & 0xFF;

    uint8_t img_cmd[] = {0x1D, 0x76, 0x30, 0x00, xL, xH, yL, yH};

    cdc_acm_host_data_tx_blocking(cdc_dev, img_cmd, sizeof(img_cmd), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    cdc_acm_host_data_tx_blocking(cdc_dev, pixel_data, pixel_data_size, 5000);
    vTaskDelay(pdMS_TO_TICKS(200));

    uint8_t feed[] = {0x1B, 0x64, 0x03};
    cdc_acm_host_data_tx_blocking(cdc_dev, feed, sizeof(feed), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t cut[] = {0x1D, 0x56, 0x41, 0x00};
    cdc_acm_host_data_tx_blocking(cdc_dev, cut, sizeof(cut), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "✓ Image printed successfully!");
}

static void printer_task(void *arg)
{
    ESP_LOGI(TAG, "Printer task started, polling for printer...");

    cdc_acm_host_device_config_t dev_config = {
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = NULL,
        .user_arg = NULL,
    };

    int poll_count = 0;
    bool image_printed = false;

    while (1) {
        if (!cdc_dev) {
            poll_count++;
            esp_err_t ret = cdc_acm_host_open(PRINTER_VID, PRINTER_PID, PRINTER_IFACE, &dev_config, &cdc_dev);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✓ Printer CONNECTED! VID=0x%04x PID=0x%04x", PRINTER_VID, PRINTER_PID);
                image_printed = false;
            }
        }

        if (cdc_dev && !image_printed) {
            print_text("Image Printing Test");
            vTaskDelay(pdMS_TO_TICKS(500));
            parse_pbm_and_print();
            image_printed = true;
        }

        if (cdc_dev) {
            esp_err_t ret = cdc_acm_host_close(cdc_dev);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Printer disconnected");
                cdc_dev = NULL;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB Printer Image Test ===");
    ESP_LOGI(TAG, "Looking for printer VID=0x%04x PID=0x%04x", PRINTER_VID, PRINTER_PID);

    usb_host_config_t host_config = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB host: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "USB Host installed");

    ret = cdc_acm_host_install(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install CDC ACM: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "CDC ACM host installed");

    xTaskCreate(usb_lib_task, "usb_lib_task", 4096, NULL, 20, NULL);
    xTaskCreate(printer_task, "printer_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "USB tasks created");
}
