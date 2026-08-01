#ifndef CYD_DISPLAY_H
#define CYD_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Screen size in landscape mode
#define CYD_SCREEN_WIDTH  320
#define CYD_SCREEN_HEIGHT 240

// Colors in 565 format (16-bit)
#define CYD_COLOR_BLACK   0x0000
#define CYD_COLOR_WHITE   0xFFFF
#define CYD_COLOR_BLUE    0x001F
#define CYD_COLOR_RED     0xF800
#define CYD_COLOR_GREEN   0x07E0
#define CYD_COLOR_CYAN    0x07FF
#define CYD_COLOR_MAGENTA 0xF81F
#define CYD_COLOR_YELLOW  0xFFE0
#define CYD_COLOR_GRAY    0x7BEF
#define CYD_COLOR_DARK_GRAY 0x18C3
#define CYD_COLOR_GALAXY_BG 0x0802 // Premium dark background
#define CYD_COLOR_GALAXY_ACCENT 0x3A5F // Premium blue accent

// Initialize the display and touch system
esp_err_t cyd_display_init(void);

// Screen backlight and wake controls
void cyd_display_wake(void);
void cyd_display_sleep(void);
bool cyd_display_is_awake(void);

// Drawing primitives
void cyd_display_clear(uint16_t color);
void cyd_display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void cyd_display_draw_pixel(int16_t x, int16_t y, uint16_t color);
void cyd_display_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);
void cyd_display_draw_string_centered(int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

// Custom hotspot dashboard drawings
void cyd_display_draw_signal_bars(int16_t x, int16_t y, int rssi);
void cyd_display_draw_battery(int16_t x, int16_t y, int percentage);

// Handle the dashboard updates
void cyd_display_update_dashboard(void);

#endif // CYD_DISPLAY_H
