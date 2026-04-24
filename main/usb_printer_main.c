#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "driver/gpio.h"

static const char *TAG = "USB_PRINTER";

#define PRINTER_VID 0x1504
#define PRINTER_PID 0x006e
#define PRINTER_IFACE 0

static cdc_acm_dev_hdl_t cdc_dev = NULL;
static QueueHandle_t print_queue = NULL;

typedef struct {
    uint8_t *data;
    size_t len;
} print_job_t;

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

    ESP_LOGI(TAG, "Print job sent: %s", text);
}

static void printer_task(void *arg)
{
    ESP_LOGI(TAG, "Printer task started, polling for printer connection...");

    cdc_acm_host_device_config_t dev_config = {
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = NULL,
        .user_arg = NULL,
    };

    int poll_count = 0;

    while (1) {
        if (!cdc_dev) {
            poll_count++;
            esp_err_t ret = cdc_acm_host_open(PRINTER_VID, PRINTER_PID, PRINTER_IFACE, &dev_config, &cdc_dev);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✓ Printer CONNECTED! VID=0x%04x PID=0x%04x", PRINTER_VID, PRINTER_PID);
                print_text("Hello ESP32!");
                print_text("USB Printer Test");
            } else if (ret == ESP_ERR_NOT_FOUND && poll_count % 10 == 0) {
                ESP_LOGW(TAG, "Printer not found (poll #%d)", poll_count);
            } else if (ret != ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Error opening printer: %s", esp_err_to_name(ret));
            }
        }

        print_job_t job;
        if (xQueueReceive(print_queue, &job, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (cdc_dev) {
                cdc_acm_host_data_tx_blocking(cdc_dev, job.data, job.len, 1000);
                ESP_LOGI(TAG, "Print job sent: %u bytes", job.len);
            }
            free(job.data);
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
    ESP_LOGI(TAG, "=== Starting USB Printer Example ===");
    ESP_LOGI(TAG, "Looking for printer VID=0x%04x PID=0x%04x", PRINTER_VID, PRINTER_PID);

    print_queue = xQueueCreate(10, sizeof(print_job_t));

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
