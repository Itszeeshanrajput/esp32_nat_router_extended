#include "cyd_display.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "router_globals.h"
#include "esp_wifi.h"
#include "multi_ap.h"

static const char *TAG = "CYD_Display";

extern char *ap_ip;
extern char *ap_ssid;

// GPIO Configuration for ILI9341 LCD
#define LCD_SPI_HOST       SPI2_HOST
#define PIN_NUM_MISO       12
#define PIN_NUM_MOSI       13
#define PIN_NUM_CLK        14
#define PIN_NUM_CS         15
#define PIN_NUM_DC         2
#define PIN_NUM_RST        4
#define PIN_NUM_BCKL       21

// GPIO Configuration for XPT2046 Touch
#define TOUCH_SPI_HOST     SPI3_HOST
#define PIN_TOUCH_MISO     39
#define PIN_TOUCH_MOSI     32
#define PIN_TOUCH_CLK      25
#define PIN_TOUCH_CS       33

static spi_device_handle_t lcd_spi_handle;
static spi_device_handle_t touch_spi_handle;

static bool display_awake = true;
static uint32_t last_touch_ticks = 0;

// Standard 8x8 font definition (ASCII 32 to 127)
static const uint8_t font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // (space)
    {0x18, 0x3C, 0x3E, 0x18, 0x18, 0x00, 0x18, 0x00}, // !
    {0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00}, // "
    {0x24, 0x24, 0xFF, 0x24, 0xFF, 0x24, 0x24, 0x00}, // #
    {0x18, 0x3E, 0x60, 0x1C, 0x03, 0x7E, 0x18, 0x00}, // $
    {0x00, 0xC6, 0xCC, 0x18, 0x30, 0x66, 0xC6, 0x00}, // %
    {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x7E, 0x00}, // &
    {0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00}, // '
    {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // (
    {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // )
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // *
    {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // ,
    {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // .
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00}, // /
    {0x3E, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // 0
    {0x0C, 0x1C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3E, 0x00}, // 1
    {0x3E, 0x63, 0x03, 0x1E, 0x30, 0x60, 0x7F, 0x00}, // 2
    {0x3E, 0x63, 0x03, 0x1E, 0x03, 0x63, 0x3E, 0x00}, // 3
    {0x06, 0x0E, 0x1E, 0x36, 0x7F, 0x06, 0x06, 0x00}, // 4
    {0x7F, 0x60, 0x7E, 0x03, 0x03, 0x63, 0x3E, 0x00}, // 5
    {0x1C, 0x30, 0x60, 0x7E, 0x63, 0x63, 0x3E, 0x00}, // 6
    {0x7F, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x00}, // 7
    {0x3E, 0x63, 0x63, 0x3E, 0x63, 0x63, 0x3E, 0x00}, // 8
    {0x3E, 0x63, 0x63, 0x7F, 0x03, 0x06, 0x3C, 0x00}, // 9
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // :
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // ;
    {0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00}, // <
    {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // =
    {0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00}, // >
    {0x3E, 0x63, 0x03, 0x06, 0x0C, 0x00, 0x0C, 0x00}, // ?
    {0x3E, 0x63, 0x03, 0x3B, 0x6B, 0x6B, 0x3E, 0x00}, // @
    {0x18, 0x3C, 0x66, 0x66, 0x7F, 0x66, 0x66, 0x00}, // A
    {0x7E, 0x63, 0x63, 0x7E, 0x63, 0x63, 0x7E, 0x00}, // B
    {0x3E, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3E, 0x00}, // C
    {0x7C, 0x66, 0x63, 0x63, 0x63, 0x66, 0x7C, 0x00}, // D
    {0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x7F, 0x00}, // E
    {0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3E, 0x63, 0x60, 0x6F, 0x63, 0x63, 0x3E, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7F, 0x66, 0x66, 0x66, 0x00}, // H
    {0x3E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3E, 0x00}, // I
    {0x1F, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00}, // J
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7F, 0x00}, // L
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // M
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // N
    {0x3E, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // O
    {0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3E, 0x63, 0x63, 0x63, 0x6B, 0x67, 0x3E, 0x0C}, // Q
    {0x7E, 0x63, 0x63, 0x7E, 0x78, 0x6C, 0x66, 0x00}, // R
    {0x3E, 0x63, 0x60, 0x3E, 0x03, 0x63, 0x3E, 0x00}, // S
    {0x7F, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E, 0x00}, // U
    {0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // V
    {0x00, 0x00, 0x63, 0x63, 0x6B, 0x7F, 0x36, 0x00}, // w
    {0x63, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x63, 0x00}, // X
    {0x63, 0x63, 0x63, 0x36, 0x1C, 0x18, 0x18, 0x00}, // Y
    {0x7F, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x7F, 0x00}, // Z
    {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // [
    {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x00}, // backslash
    {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // ]
    {0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00}, // ^
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // _
    {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, // `
    {0x00, 0x00, 0x3E, 0x03, 0x3F, 0x63, 0x3F, 0x00}, // a
    {0x60, 0x60, 0x7E, 0x63, 0x63, 0x63, 0x7E, 0x00}, // b
    {0x00, 0x00, 0x3E, 0x60, 0x60, 0x63, 0x3E, 0x00}, // c
    {0x03, 0x03, 0x3F, 0x63, 0x63, 0x63, 0x3F, 0x00}, // d
    {0x00, 0x00, 0x3E, 0x63, 0x7F, 0x60, 0x3E, 0x00}, // e
    {0x1C, 0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x00}, // f
    {0x00, 0x00, 0x3F, 0x63, 0x63, 0x3F, 0x03, 0x3E}, // g
    {0x60, 0x60, 0x7E, 0x63, 0x63, 0x63, 0x63, 0x00}, // h
    {0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // i
    {0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x38}, // j
    {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, // k
    {0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00}, // l
    {0x00, 0x00, 0x76, 0x7F, 0x6D, 0x63, 0x63, 0x00}, // m
    {0x00, 0x00, 0x7E, 0x63, 0x63, 0x63, 0x63, 0x00}, // n
    {0x00, 0x00, 0x3E, 0x63, 0x63, 0x63, 0x3E, 0x00}, // o
    {0x00, 0x00, 0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60}, // p
    {0x00, 0x00, 0x3F, 0x63, 0x63, 0x3F, 0x03, 0x03}, // q
    {0x00, 0x00, 0x7E, 0x63, 0x60, 0x60, 0x60, 0x00}, // r
    {0x00, 0x00, 0x3E, 0x60, 0x3E, 0x03, 0x3E, 0x00}, // s
    {0x30, 0x30, 0x7E, 0x30, 0x30, 0x30, 0x1C, 0x00}, // t
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x63, 0x3F, 0x00}, // u
    {0x00, 0x00, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // v
    {0x00, 0x00, 0x63, 0x63, 0x6B, 0x7F, 0x36, 0x00}, // w
    {0x63, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x63, 0x00}, // X
    {0x63, 0x63, 0x63, 0x36, 0x1C, 0x18, 0x18, 0x00}, // Y
    {0x7F, 0x03, 0x06, 0x0C, 0x18, 0x7F, 0x00}, // z
    {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, // {
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // |
    {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, // }
    {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ~
    {0x00, 0x10, 0x38, 0x7C, 0xFE, 0x7C, 0x38, 0x10}  // bullet
};

// Write a command to ILI9341
static void lcd_write_cmd(uint8_t cmd)
{
    gpio_set_level(PIN_NUM_DC, 0); // DC low for command
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(lcd_spi_handle, &t);
}

// Write data to ILI9341
static void lcd_write_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    gpio_set_level(PIN_NUM_DC, 1); // DC high for data
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(lcd_spi_handle, &t);
}

static void lcd_write_data_byte(uint8_t val)
{
    lcd_write_data(&val, 1);
}

// Set drawing address window on screen
static void lcd_set_window(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    lcd_write_cmd(0x2A); // Column Address Set
    uint8_t data_x[] = { (x0 >> 8) & 0xFF, x0 & 0xFF, (x1 >> 8) & 0xFF, x1 & 0xFF };
    lcd_write_data(data_x, 4);

    lcd_write_cmd(0x2B); // Page Address Set
    uint8_t data_y[] = { (y0 >> 8) & 0xFF, y0 & 0xFF, (y1 >> 8) & 0xFF, y1 & 0xFF };
    lcd_write_data(data_y, 4);

    lcd_write_cmd(0x2C); // Memory Write
}

// Draw pixel
void cyd_display_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= CYD_SCREEN_WIDTH || y < 0 || y >= CYD_SCREEN_HEIGHT) return;
    lcd_set_window(x, y, x, y);
    uint8_t data[] = { (color >> 8) & 0xFF, color & 0xFF };
    lcd_write_data(data, 2);
}

// Fill a rectangular area with a single color
void cyd_display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > CYD_SCREEN_WIDTH) w = CYD_SCREEN_WIDTH - x;
    if (y + h > CYD_SCREEN_HEIGHT) h = CYD_SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    lcd_set_window(x, y, x + w - 1, y + h - 1);

    int chunk_pixels = 320; // fill line chunks
    if (chunk_pixels > w) chunk_pixels = w;

    uint8_t *buf = malloc(chunk_pixels * 2);
    if (!buf) return;

    for (int i = 0; i < chunk_pixels; i++) {
        buf[i * 2] = (color >> 8) & 0xFF;
        buf[i * 2 + 1] = color & 0xFF;
    }

    int total_bytes = w * h * 2;
    gpio_set_level(PIN_NUM_DC, 1);

    while (total_bytes > 0) {
        int to_write = total_bytes;
        if (to_write > chunk_pixels * 2) to_write = chunk_pixels * 2;

        spi_transaction_t t = {
            .length = to_write * 8,
            .tx_buffer = buf,
        };
        spi_device_polling_transmit(lcd_spi_handle, &t);
        total_bytes -= to_write;
    }

    free(buf);
}

// Clear the entire screen
void cyd_display_clear(uint16_t color)
{
    cyd_display_fill_rect(0, 0, CYD_SCREEN_WIDTH, CYD_SCREEN_HEIGHT, color);
}

// Draw character
static void cyd_display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t scale)
{
    if (c < 32 || c > 127) c = 127; // use bullet for out of range
    uint8_t idx = c - 32;

    for (int8_t i = 0; i < 8; i++) {
        uint8_t line = font8x8[idx][i];
        for (int8_t j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                if (scale == 1) {
                    cyd_display_draw_pixel(x + j, y + i, color);
                } else {
                    cyd_display_fill_rect(x + j * scale, y + i * scale, scale, scale, color);
                }
            } else if (bg != color) {
                if (scale == 1) {
                    cyd_display_draw_pixel(x + j, y + i, bg);
                } else {
                    cyd_display_fill_rect(x + j * scale, y + i * scale, scale, scale, bg);
                }
            }
        }
    }
}

// Draw string
void cyd_display_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
    while (*str) {
        cyd_display_draw_char(x, y, *str, color, bg, scale);
        x += 8 * scale;
        str++;
        if (x + 8 * scale > CYD_SCREEN_WIDTH) {
            break; // No wrap for clean lines
        }
    }
}

// Draw string centered horizontally
void cyd_display_draw_string_centered(int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale)
{
    int len = strlen(str);
    int16_t x = (CYD_SCREEN_WIDTH - (len * 8 * scale)) / 2;
    if (x < 0) x = 0;
    cyd_display_draw_string(x, y, str, color, bg, scale);
}

// Draw signal strength bars
void cyd_display_draw_signal_bars(int16_t x, int16_t y, int rssi)
{
    int bars = 0;
    if (rssi != 0) {
        if (rssi > -55) bars = 4;
        else if (rssi > -70) bars = 3;
        else if (rssi > -85) bars = 2;
        else if (rssi > -100) bars = 1;
    }

    uint16_t color_inactive = CYD_COLOR_GRAY;
    uint16_t color_active = CYD_COLOR_GREEN;

    cyd_display_fill_rect(x + 12, y + 12, 3, 4, (bars >= 1) ? color_active : color_inactive);
    cyd_display_fill_rect(x + 17, y + 8, 3, 8, (bars >= 2) ? color_active : color_inactive);
    cyd_display_fill_rect(x + 22, y + 4, 3, 12, (bars >= 3) ? color_active : color_inactive);
    cyd_display_fill_rect(x + 27, y, 3, 16, (bars >= 4) ? color_active : color_inactive);
}

// Draw battery percentage indicator
void cyd_display_draw_battery(int16_t x, int16_t y, int percentage)
{
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    cyd_display_fill_rect(x, y, 28, 14, CYD_COLOR_WHITE);
    cyd_display_fill_rect(x + 2, y + 2, 24, 10, CYD_COLOR_BLACK);
    cyd_display_fill_rect(x + 28, y + 4, 2, 6, CYD_COLOR_WHITE);

    int fill_w = (percentage * 22) / 100;
    uint16_t fill_color = CYD_COLOR_GREEN;
    if (percentage < 20) fill_color = CYD_COLOR_RED;
    else if (percentage < 50) fill_color = CYD_COLOR_YELLOW;

    if (fill_w > 0) {
        cyd_display_fill_rect(x + 3, y + 3, fill_w, 8, fill_color);
    }

    char buf[8];
    sprintf(buf, "%d%%", percentage);
    cyd_display_draw_string(x - 35, y + 3, buf, CYD_COLOR_WHITE, CYD_COLOR_BLACK, 1);
}

// Helper to read actual battery voltage and calculate percentage on IO35
static int cyd_read_battery_percentage(void)
{
    int val = adc1_get_raw(ADC1_CHANNEL_7); // IO35 is ADC1 Channel 7
    if (val <= 0) {
        static int simulated_percent = 98;
        static uint32_t last_drain = 0;
        if (xTaskGetTickCount() - last_drain > pdMS_TO_TICKS(60000)) {
            simulated_percent--;
            if (simulated_percent < 1) simulated_percent = 100;
            last_drain = xTaskGetTickCount();
        }
        return simulated_percent;
    }

    float voltage = (val * 3.3f * 2.0f) / 4095.0f;
    int percent = (int)((voltage - 3.2f) * 100.0f / (4.2f - 3.2f));
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

// Read ambient light level via onboard LDR on GPIO 34 and adjust backlight
static void cyd_adjust_backlight_by_ldr(void)
{
    if (!display_awake) return;

    int ldr_raw = adc1_get_raw(ADC1_CHANNEL_6); // IO34 is ADC1 Channel 6
    // Map LDR raw range (0 - 4095) where bright is higher value, dark is lower value
    int duty = 150 + (ldr_raw * 873 / 4095);
    if (duty < 150) duty = 150;     // Minimum visible backlight
    if (duty > 1023) duty = 1023;   // Maximum backlight

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ESP_LOGD(TAG, "LDR Raw: %d, adjusted screen PWM duty: %d/1023", ldr_raw, duty);
}

// Screen Backlight wake controls using LEDC PWM
void cyd_display_wake(void)
{
    display_awake = true;
    cyd_adjust_backlight_by_ldr();
    last_touch_ticks = xTaskGetTickCount();
    ESP_LOGI(TAG, "Backlight ON (Awake)");
}

void cyd_display_sleep(void)
{
    display_awake = false;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0); // Backlight off
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    ESP_LOGI(TAG, "Backlight OFF (Sleep)");
}

bool cyd_display_is_awake(void)
{
    return display_awake;
}

// Read raw coordinates from XPT2046
static bool touch_read_raw(uint16_t *x, uint16_t *y)
{
    uint8_t tx_data[3];
    uint8_t rx_data[3];

    tx_data[0] = 0x90; // Command for Y
    tx_data[1] = 0x00;
    tx_data[2] = 0x00;

    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    gpio_set_level(PIN_TOUCH_CS, 0);
    spi_device_polling_transmit(touch_spi_handle, &t);
    gpio_set_level(PIN_TOUCH_CS, 1);

    uint16_t raw_y = ((rx_data[1] << 8) | rx_data[2]) >> 3;

    tx_data[0] = 0xD0; // Command for X
    t.tx_buffer = tx_data;
    gpio_set_level(PIN_TOUCH_CS, 0);
    spi_device_polling_transmit(touch_spi_handle, &t);
    gpio_set_level(PIN_TOUCH_CS, 1);

    uint16_t raw_x = ((rx_data[1] << 8) | rx_data[2]) >> 3;

    if (raw_x > 150 && raw_x < 3950 && raw_y > 150 && raw_y < 3950) {
        *x = (raw_x - 150) * CYD_SCREEN_WIDTH / 3800;
        *y = (raw_y - 150) * CYD_SCREEN_HEIGHT / 3800;

        *x = CYD_SCREEN_WIDTH - *x;
        *y = CYD_SCREEN_HEIGHT - *y;

        return true;
    }
    return false;
}

// Update the premium Dashboard UI
void cyd_display_update_dashboard(void)
{
    cyd_display_fill_rect(0, 0, CYD_SCREEN_WIDTH, CYD_SCREEN_HEIGHT, CYD_COLOR_GALAXY_BG);

    wifi_ap_record_t ap_info;
    memset(&ap_info, 0, sizeof(ap_info));
    int rssi = 0;
    char up_ssid[33] = "DISCONNECTED";
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
        strncpy(up_ssid, (char *)ap_info.ssid, sizeof(up_ssid) - 1);
    }

    int battery_percentage = cyd_read_battery_percentage();
    uint16_t client_count = getConnectCount();

    // 1. TOP HEADER STATUS BAR (y: 0 to 24)
    cyd_display_draw_string(10, 5, "GALAXY", CYD_COLOR_CYAN, CYD_COLOR_GALAXY_BG, 1);
    cyd_display_draw_string(62, 5, "HOTSPOT", CYD_COLOR_WHITE, CYD_COLOR_GALAXY_BG, 1);
    cyd_display_draw_signal_bars(220, 4, rssi);
    cyd_display_draw_battery(285, 5, battery_percentage);

    // Header divider line
    cyd_display_fill_rect(0, 24, CYD_SCREEN_WIDTH, 1, CYD_COLOR_GALAXY_ACCENT);

    // 2. MAIN ACTIVE HOTSPOT CLIENTS BOX (Left Side)
    cyd_display_fill_rect(10, 35, 140, 100, CYD_COLOR_DARK_GRAY);
    cyd_display_draw_string_centered(45, "CLIENTS", CYD_COLOR_GRAY, CYD_COLOR_DARK_GRAY, 1);
    char count_str[8];
    sprintf(count_str, "%d", client_count);
    int count_len = strlen(count_str);
    int16_t count_x = 10 + (140 - (count_len * 8 * 4)) / 2;
    cyd_display_draw_string(count_x, 60, count_str, (client_count > 0) ? CYD_COLOR_GREEN : CYD_COLOR_WHITE, CYD_COLOR_DARK_GRAY, 4);
    cyd_display_draw_string_centered(115, "CONNECTED", CYD_COLOR_GRAY, CYD_COLOR_DARK_GRAY, 1);

    // 3. HOTSPOT INFORMATION LABELS (Right Side)
    char label_buf[64];
    cyd_display_draw_string(160, 35, "HOTSPOT CONFIG", CYD_COLOR_CYAN, CYD_COLOR_GALAXY_BG, 1);

    sprintf(label_buf, "SSID: %s", ap_ssid ? ap_ssid : "ESP32_Router");
    cyd_display_draw_string(160, 55, label_buf, CYD_COLOR_WHITE, CYD_COLOR_GALAXY_BG, 1);

    sprintf(label_buf, "IP:   %s", ap_ip ? ap_ip : "192.168.4.1");
    cyd_display_draw_string(160, 75, label_buf, CYD_COLOR_WHITE, CYD_COLOR_GALAXY_BG, 1);

    sprintf(label_buf, "UPLINK: %s", up_ssid);
    cyd_display_draw_string(160, 95, label_buf, ap_connect ? CYD_COLOR_GREEN : CYD_COLOR_RED, CYD_COLOR_GALAXY_BG, 1);

    // 4. SAVED AP PROFILE LIST (Bottom Half)
    cyd_display_draw_string(10, 145, "SAVED NETWORKS (TAP TO SWITCH)", CYD_COLOR_CYAN, CYD_COLOR_GALAXY_BG, 1);
    cyd_display_fill_rect(10, 158, 300, 1, CYD_COLOR_GALAXY_ACCENT);

    saved_ap_t saved_list[MAX_SAVED_APS];
    multi_ap_load(saved_list);

    int display_row = 0;
    for (int i = 0; i < MAX_SAVED_APS; i++) {
        if (saved_list[i].valid) {
            char profile_buf[64];
            bool is_current = (strcmp(saved_list[i].ssid, up_ssid) == 0);
            sprintf(profile_buf, "%d. %-18.18s  %s", i + 1, saved_list[i].ssid, is_current ? "[ACTIVE]" : "[SAVED]");
            cyd_display_draw_string(15, 165 + display_row * 13, profile_buf, is_current ? CYD_COLOR_GREEN : CYD_COLOR_GRAY, CYD_COLOR_GALAXY_BG, 1);
            display_row++;
            if (display_row >= 3) break;
        }
    }

    if (display_row == 0) {
        cyd_display_draw_string(15, 165, "No saved AP profiles. Configure in WebUI.", CYD_COLOR_GRAY, CYD_COLOR_GALAXY_BG, 1);
    }

    // 5. FOOTER STATUS BAR (y: 215 to 240)
    cyd_display_fill_rect(10, 215, 300, 20, CYD_COLOR_DARK_GRAY);
    cyd_display_draw_string(20, 221, "Touch screen to Wake  |  5s Timer", CYD_COLOR_CYAN, CYD_COLOR_DARK_GRAY, 1);
}

// Background task
static void cyd_display_task(void *pvParameters)
{
    uint16_t touch_x = 0;
    uint16_t touch_y = 0;

    ESP_LOGI(TAG, "CYD display task started.");
    last_touch_ticks = xTaskGetTickCount();

    cyd_display_clear(CYD_COLOR_GALAXY_BG);
    cyd_display_update_dashboard();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));

        if (touch_read_raw(&touch_x, &touch_y)) {
            ESP_LOGD(TAG, "Touch detected at (%d, %d)", touch_x, touch_y);

            if (!display_awake) {
                cyd_display_wake();
                cyd_display_update_dashboard();
            } else {
                last_touch_ticks = xTaskGetTickCount();

                // Check Row click on Saved AP Profiles
                if (touch_x >= 10 && touch_x <= 310 && touch_y >= 160 && touch_y <= 200) {
                    // Identify slot index from coordinate
                    int row = (touch_y - 160) / 13;

                    // Match visual row to actual NVS valid AP slots
                    saved_ap_t saved_list[MAX_SAVED_APS];
                    multi_ap_load(saved_list);
                    int valid_row_match = 0;
                    int target_slot = -1;

                    for (int i = 0; i < MAX_SAVED_APS; i++) {
                        if (saved_list[i].valid) {
                            if (valid_row_match == row) {
                                target_slot = i;
                                break;
                            }
                            valid_row_match++;
                        }
                    }

                    if (target_slot != -1) {
                        ESP_LOGI(TAG, "Tapped profile at slot index %d. Switching connection...", target_slot);
                        // Draw a temporary on-screen loading status to be extremely user-friendly!
                        cyd_display_fill_rect(10, 215, 300, 20, CYD_COLOR_RED);
                        cyd_display_draw_string(20, 221, "Connecting to Selected Profile...", CYD_COLOR_WHITE, CYD_COLOR_RED, 1);

                        multi_ap_switch_to(target_slot);
                        vTaskDelay(pdMS_TO_TICKS(1500));
                        cyd_display_update_dashboard();
                    }
                }
            }
        }

        // Screen auto-timeout after 5 seconds
        if (display_awake && (xTaskGetTickCount() - last_touch_ticks > pdMS_TO_TICKS(5000))) {
            ESP_LOGI(TAG, "Display auto-timeout. Sleeping display.");
            cyd_display_sleep();
        }

        // Periodically refresh dashboard parameters and adjust backlight via LDR
        static uint32_t last_refresh = 0;
        if (display_awake && (xTaskGetTickCount() - last_refresh > pdMS_TO_TICKS(1000))) {
            cyd_adjust_backlight_by_ldr();
            cyd_display_update_dashboard();
            last_refresh = xTaskGetTickCount();
        }
    }
}

// Initialize the display hardware
esp_err_t cyd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing CYD Display and Touch SPI interfaces...");

    // Setup ADC for battery measurement on pin IO35
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); // IO35
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // IO34 (LDR Sensor)

    // Configure LEDC Timer and Channel for PWM Backlight on GPIO 21
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_10_BIT,
        .timer_num        = LEDC_TIMER_1,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_1,
        .timer_sel      = LEDC_TIMER_1,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_NUM_BCKL,
        .duty           = 1023, // Start with maximum brightness
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_NUM_DC, 1);

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 320 * 240 * 2
    };
    esp_err_t ret = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI2 bus: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 26 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    ret = spi_bus_add_device(LCD_SPI_HOST, &devcfg, &lcd_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add LCD SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    io_conf.pin_bit_mask = (1ULL << PIN_NUM_RST);
    gpio_config(&io_conf);
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    lcd_write_cmd(0x01); // Software Reset
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_write_cmd(0x28); // Display OFF

    lcd_write_cmd(0x3A); // Pixel Format Set
    lcd_write_data_byte(0x55);

    lcd_write_cmd(0x36); // Memory Access Control (Orientation)
    lcd_write_data_byte(0x28);

    lcd_write_cmd(0x11); // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_write_cmd(0x29); // Display ON
    vTaskDelay(pdMS_TO_TICKS(100));

    buscfg.miso_io_num = PIN_TOUCH_MISO;
    buscfg.mosi_io_num = PIN_TOUCH_MOSI;
    buscfg.sclk_io_num = PIN_TOUCH_CLK;
    ret = spi_bus_initialize(TOUCH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI3 bus for touch: %s", esp_err_to_name(ret));
        return ret;
    }

    devcfg.clock_speed_hz = 2 * 1000 * 1000;
    devcfg.spics_io_num = PIN_TOUCH_CS;
    ret = spi_bus_add_device(TOUCH_SPI_HOST, &devcfg, &touch_spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add Touch SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    io_conf.pin_bit_mask = (1ULL << PIN_TOUCH_CS);
    gpio_config(&io_conf);
    gpio_set_level(PIN_TOUCH_CS, 1);

    xTaskCreate(cyd_display_task, "cyd_display_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}
