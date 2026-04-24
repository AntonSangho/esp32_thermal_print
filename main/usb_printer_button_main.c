#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "driver/gpio.h"

static const char *TAG = "USB_PRINTER_BUTTON";

#define PRINTER_VID 0x1504
#define PRINTER_PID 0x006e
#define PRINTER_IFACE 0
#define BUTTON_PIN GPIO_NUM_3
#define BUTTON_ACTIVE_LOW 1

extern const uint8_t logo_pbm_start[] asm("_binary_logo_pbm_start");
extern const uint8_t logo_pbm_end[] asm("_binary_logo_pbm_end");

static cdc_acm_dev_hdl_t cdc_dev = NULL;
static uint32_t press_count = 0;

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

    ESP_LOGI(TAG, "Original PBM: %" PRIu32 " x %" PRIu32 " pixels", width, height);

    uint8_t *orig_pixel_data = (uint8_t *)p;
    uint32_t orig_bytes_per_row = (width + 7) / 8;

    uint32_t scaled_width = (width * 4) / 3;
    uint32_t scaled_height = (height * 4) / 3;
    uint32_t scaled_bytes_per_row = (scaled_width + 7) / 8;
    uint32_t scaled_pixel_data_size = scaled_bytes_per_row * scaled_height;

    uint8_t *scaled_pixel_data = (uint8_t *)malloc(scaled_pixel_data_size);
    if (!scaled_pixel_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for scaled image");
        return;
    }

    memset(scaled_pixel_data, 0, scaled_pixel_data_size);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t byte_idx = x / 8;
            uint32_t bit_idx = 7 - (x % 8);
            uint8_t pixel = (orig_pixel_data[y * orig_bytes_per_row + byte_idx] >> bit_idx) & 1;

            uint32_t scaled_x_start = (x * 4) / 3;
            uint32_t scaled_x_end = ((x + 1) * 4) / 3;
            uint32_t scaled_y_start = (y * 4) / 3;
            uint32_t scaled_y_end = ((y + 1) * 4) / 3;

            for (uint32_t scaled_y = scaled_y_start; scaled_y < scaled_y_end; scaled_y++) {
                for (uint32_t scaled_x = scaled_x_start; scaled_x < scaled_x_end; scaled_x++) {
                    uint32_t scaled_byte_idx = scaled_x / 8;
                    uint32_t scaled_bit_idx = 7 - (scaled_x % 8);

                    if (pixel) {
                        scaled_pixel_data[scaled_y * scaled_bytes_per_row + scaled_byte_idx] |= (1 << scaled_bit_idx);
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "Scaled PBM: %" PRIu32 " x %" PRIu32 " pixels (1.33x)", scaled_width, scaled_height);

    uint8_t init[] = {0x1B, 0x40};
    uint8_t center[] = {0x1B, 0x61, 0x01};

    cdc_acm_host_data_tx_blocking(cdc_dev, init, sizeof(init), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    cdc_acm_host_data_tx_blocking(cdc_dev, center, sizeof(center), 1000);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint16_t xL = scaled_bytes_per_row & 0xFF;
    uint16_t xH = (scaled_bytes_per_row >> 8) & 0xFF;
    uint16_t yL = scaled_height & 0xFF;
    uint16_t yH = (scaled_height >> 8) & 0xFF;

    uint8_t img_cmd[] = {0x1D, 0x76, 0x30, 0x00, xL, xH, yL, yH};

    cdc_acm_host_data_tx_blocking(cdc_dev, img_cmd, sizeof(img_cmd), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    cdc_acm_host_data_tx_blocking(cdc_dev, scaled_pixel_data, scaled_pixel_data_size, 5000);
    vTaskDelay(pdMS_TO_TICKS(200));

    uint8_t feed[] = {0x1B, 0x64, 0x03};
    cdc_acm_host_data_tx_blocking(cdc_dev, feed, sizeof(feed), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t cut[] = {0x1D, 0x56, 0x41, 0x00};
    cdc_acm_host_data_tx_blocking(cdc_dev, cut, sizeof(cut), 1000);
    vTaskDelay(pdMS_TO_TICKS(100));

    free(scaled_pixel_data);
    ESP_LOGI(TAG, "✓ Image printed successfully!");
}

static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started (GPIO %d)", BUTTON_PIN);

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    uint32_t prev_state = gpio_get_level(BUTTON_PIN);
    uint32_t stable_count = 0;
    const uint32_t debounce_threshold = 10;

    while (1) {
        uint32_t curr_state = gpio_get_level(BUTTON_PIN);

        if (curr_state == prev_state) {
            stable_count++;
        } else {
            stable_count = 0;
            prev_state = curr_state;
        }

        if (stable_count >= debounce_threshold && curr_state == 0) {
            if (cdc_dev) {
                press_count++;
                ESP_LOGI(TAG, "Button Press #%lu - Printing image...", press_count);
                parse_pbm_and_print();
            } else {
                ESP_LOGW(TAG, "Button pressed but printer not connected");
            }

            stable_count = 0;
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
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

    while (1) {
        if (!cdc_dev) {
            poll_count++;
            esp_err_t ret = cdc_acm_host_open(PRINTER_VID, PRINTER_PID, PRINTER_IFACE, &dev_config, &cdc_dev);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✓ Printer CONNECTED! VID=0x%04x PID=0x%04x", PRINTER_VID, PRINTER_PID);
            } else if (ret == ESP_ERR_NOT_FOUND && poll_count % 10 == 0) {
                ESP_LOGW(TAG, "Printer not found (poll #%d)", poll_count);
            } else if (ret != ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Error opening printer: %s", esp_err_to_name(ret));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB Printer + Button Control ===");
    ESP_LOGI(TAG, "Button PIN: GPIO %d | Printer: VID=0x%04x PID=0x%04x", BUTTON_PIN, PRINTER_VID, PRINTER_PID);

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
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "All tasks created");
}
