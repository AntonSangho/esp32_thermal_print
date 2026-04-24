#include <stdio.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_BLINK";

#ifndef BLINK_GPIO
#define BLINK_GPIO 20
#endif

#ifndef BLINK_PERIOD_MS
#define BLINK_PERIOD_MS 500
#endif

void blink_task(void *arg)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while (1) {
        ESP_LOGI(TAG, "LED ON");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));

        ESP_LOGI(TAG, "LED OFF");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LED blink example");
    ESP_LOGI(TAG, "GPIO: %d, Period: %d ms", BLINK_GPIO, BLINK_PERIOD_MS);

    xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);
}
