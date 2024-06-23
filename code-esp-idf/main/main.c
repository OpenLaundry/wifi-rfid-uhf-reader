#include <stdio.h>
#include <freertos/portmacro.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/uart.h"
#include "esp_log.h"

// const byte multi_read[] = {0xBB, 0x00, 0x27, 0x00, 0x03, 0x22, 0xFF, 0xFF, 0x4A, 0x7E};
const uint8_t single_read[] = {0x00, 0x22, 0x00, 0x00};
const uint8_t stop_read[] = {0x00, 0x22, 0x00, 0x00};

#define UART_NUM UART_NUM_2
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

#define CMD_HEADER 0xBB
#define CMD_END 0x7E

static const int RX_BUF_SIZE = 1024;

#define BLINK_GPIO 12
#define VM_EN_GPIO 14

static uint8_t s_led_state = 0;

void init_uart() {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendHexData(const char* logName, const uint8_t* data, size_t len) {
    const int txBytes = uart_write_bytes(UART_NUM, (const char*)data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

uint8_t calculate_checksum(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

static void tx_task(void* arg) {
    static const char* TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);

    while (1) {
        uint8_t checksum = calculate_checksum(single_read, sizeof(single_read));
        uint8_t cmd[20] = {CMD_HEADER};
        for (size_t i = 0; i < sizeof(single_read); ++i) {
            cmd[i+1] = single_read[i];
        }
        cmd[sizeof(cmd)+1] = checksum;
        cmd[sizeof(cmd)+1] = CMD_END;
        sendHexData(TX_TASK_TAG, cmd, sizeof(cmd));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


static void rx_task(void* arg) {
    static const char* RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}

static void configure_led(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_reset_pin(VM_EN_GPIO);

    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(VM_EN_GPIO, GPIO_MODE_OUTPUT);
}

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

void app_main(void)
{

    /* Configure the peripheral according to the LED type */
    configure_led();
    init_uart();
    gpio_set_level(VM_EN_GPIO, true);
    vTaskDelay(250 / portTICK_PERIOD_MS);
    xTaskCreate(tx_task, "uart_tx_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);

    while (1) {
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}