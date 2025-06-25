#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_ops.h"
#include "esp_random.h"

// ILI9341 Display Configuration
#define DISPLAY_HOST            SPI2_HOST
#define DISPLAY_SCLK_GPIO       6
#define DISPLAY_MOSI_GPIO       7
#define DISPLAY_MISO_GPIO       -1  // Not used
#define DISPLAY_CS_GPIO         20
#define DISPLAY_DC_GPIO         21
#define DISPLAY_RST_GPIO        3
#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240

static const char *TAG = "monoscope-tv";

// Monoscope TV pattern colors (RGB565 format)
#define RGB565(r, g, b) (((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F))
#define COLOR_WHITE     RGB565(31, 63, 31)  // Pure white
#define COLOR_BLACK     RGB565(0, 0, 0)     // Pure black
#define COLOR_GRAY      RGB565(15, 31, 15)  // Gray
#define COLOR_RED       RGB565(31, 0, 0)    // Pure red
#define COLOR_GREEN     RGB565(0, 63, 0)    // Pure green
#define COLOR_BLUE      RGB565(0, 0, 31)    // Pure blue
#define COLOR_YELLOW    RGB565(31, 63, 0)   // Yellow
#define COLOR_CYAN      RGB565(0, 63, 31)   // Cyan
#define COLOR_MAGENTA   RGB565(31, 0, 31)   // Magenta

static esp_lcd_panel_handle_t panel_handle = NULL;

static void init_display(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = DISPLAY_SCLK_GPIO,
        .mosi_io_num = DISPLAY_MOSI_GPIO,
        .miso_io_num = DISPLAY_MISO_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = DISPLAY_DC_GPIO,
        .cs_gpio_num = DISPLAY_CS_GPIO,
        .pclk_hz = 10 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)DISPLAY_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ILI9341 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = DISPLAY_RST_GPIO,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

    ESP_LOGI(TAG, "Initialize LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

static void draw_circle(uint16_t *buffer, int center_x, int center_y, int radius, uint16_t color)
{
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = center_x + x;
                int py = center_y + y;
                if (px >= 0 && px < DISPLAY_WIDTH && py >= 0 && py < DISPLAY_HEIGHT) {
                    buffer[py * DISPLAY_WIDTH + px] = color;
                }
            }
        }
    }
}

static void draw_line(uint16_t *buffer, int x1, int y1, int x2, int y2, uint16_t color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x1 >= 0 && x1 < DISPLAY_WIDTH && y1 >= 0 && y1 < DISPLAY_HEIGHT) {
            buffer[y1 * DISPLAY_WIDTH + x1] = color;
        }

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void draw_monoscope_pattern(void)
{
    ESP_LOGI(TAG, "Drawing monoscope TV test pattern");

    // Allocate full screen buffer
    uint16_t *screen_buf = malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
    if (!screen_buf) {
        ESP_LOGE(TAG, "Failed to allocate screen buffer");
        return;
    }

    // Fill with black background
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
        screen_buf[i] = COLOR_BLACK;
    }

    int center_x = DISPLAY_WIDTH / 2;
    int center_y = DISPLAY_HEIGHT / 2;

    // Draw color bars at the top
    int bar_width = DISPLAY_WIDTH / 8;
    uint16_t colors[] = {COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
                        COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK};

    for (int i = 0; i < 8; i++) {
        for (int y = 0; y < 40; y++) {
            for (int x = i * bar_width; x < (i + 1) * bar_width && x < DISPLAY_WIDTH; x++) {
                screen_buf[y * DISPLAY_WIDTH + x] = colors[i];
            }
        }
    }

    // Draw crosshairs
    // Horizontal line
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        screen_buf[center_y * DISPLAY_WIDTH + x] = COLOR_WHITE;
    }
    // Vertical line
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        screen_buf[y * DISPLAY_WIDTH + center_x] = COLOR_WHITE;
    }

    // Draw concentric circles
    draw_circle(screen_buf, center_x, center_y, 80, COLOR_WHITE);
    draw_circle(screen_buf, center_x, center_y, 60, COLOR_WHITE);
    draw_circle(screen_buf, center_x, center_y, 40, COLOR_WHITE);
    draw_circle(screen_buf, center_x, center_y, 20, COLOR_WHITE);

    // Draw corner marks
    int corner_size = 20;
    // Top-left
    draw_line(screen_buf, 0, 50, corner_size, 50, COLOR_WHITE);
    draw_line(screen_buf, corner_size, 50, corner_size, 50 + corner_size, COLOR_WHITE);

    // Top-right
    draw_line(screen_buf, DISPLAY_WIDTH - 1, 50, DISPLAY_WIDTH - 1 - corner_size, 50, COLOR_WHITE);
    draw_line(screen_buf, DISPLAY_WIDTH - 1 - corner_size, 50, DISPLAY_WIDTH - 1 - corner_size, 50 + corner_size, COLOR_WHITE);

    // Bottom-left
    draw_line(screen_buf, 0, DISPLAY_HEIGHT - 50, corner_size, DISPLAY_HEIGHT - 50, COLOR_WHITE);
    draw_line(screen_buf, corner_size, DISPLAY_HEIGHT - 50, corner_size, DISPLAY_HEIGHT - 50 - corner_size, COLOR_WHITE);

    // Bottom-right
    draw_line(screen_buf, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 50, DISPLAY_WIDTH - 1 - corner_size, DISPLAY_HEIGHT - 50, COLOR_WHITE);
    draw_line(screen_buf, DISPLAY_WIDTH - 1 - corner_size, DISPLAY_HEIGHT - 50, DISPLAY_WIDTH - 1 - corner_size, DISPLAY_HEIGHT - 50 - corner_size, COLOR_WHITE);

    // Draw diagonal lines for resolution test
    draw_line(screen_buf, center_x - 50, center_y - 50, center_x + 50, center_y + 50, COLOR_RED);
    draw_line(screen_buf, center_x + 50, center_y - 50, center_x - 50, center_y + 50, COLOR_RED);

    // Draw gray scale bars at the bottom
    int gray_bar_width = DISPLAY_WIDTH / 10;
    for (int i = 0; i < 10; i++) {
        uint16_t gray_level = RGB565(i * 3, i * 6, i * 3);
        for (int y = DISPLAY_HEIGHT - 30; y < DISPLAY_HEIGHT; y++) {
            for (int x = i * gray_bar_width; x < (i + 1) * gray_bar_width && x < DISPLAY_WIDTH; x++) {
                screen_buf[y * DISPLAY_WIDTH + x] = gray_level;
            }
        }
    }

    // Send the entire screen to display
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, screen_buf));

    free(screen_buf);
}

static void add_tv_noise(void)
{
    // Create random noise overlay
    uint16_t *noise_buf = malloc(DISPLAY_WIDTH * sizeof(uint16_t));
    if (!noise_buf) {
        ESP_LOGE(TAG, "Failed to allocate noise buffer");
        return;
    }

    // Add random scan lines and static
    for (int i = 0; i < 20; i++) {  // Add 20 random noise lines
        int random_y = esp_random() % DISPLAY_HEIGHT;

        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            // Generate random noise pixel
            uint32_t noise = esp_random();
            if (noise % 10 < 3) {  // 30% chance of noise
                uint16_t noise_color;
                if (noise % 3 == 0) {
                    noise_color = COLOR_WHITE;
                } else if (noise % 3 == 1) {
                    noise_color = COLOR_GRAY;
                } else {
                    noise_color = COLOR_BLACK;
                }
                noise_buf[x] = noise_color;
            } else {
                // Keep some transparency by using a darker version
                noise_buf[x] = RGB565(noise % 8, noise % 16, noise % 8);
            }
        }

        // Apply noise line to display
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, random_y, DISPLAY_WIDTH, random_y + 1, noise_buf));
    }

    free(noise_buf);
}

static void simulate_tv_flicker(void)
{
    // Simulate old TV brightness flicker by adjusting the entire display
    static int brightness_cycle = 0;
    brightness_cycle = (brightness_cycle + 1) % 100;

    // Every so often, create a brightness flicker effect
    if (brightness_cycle % 30 == 0) {
        // Brief screen flash/dim
        uint16_t *flicker_buf = malloc(DISPLAY_WIDTH * sizeof(uint16_t));
        if (flicker_buf) {
            uint16_t flicker_color = (esp_random() % 2) ? RGB565(2, 4, 2) : RGB565(8, 16, 8);

            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                flicker_buf[x] = flicker_color;
            }

            // Apply flicker to a few random lines
            for (int i = 0; i < 5; i++) {
                int y = esp_random() % DISPLAY_HEIGHT;
                ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, DISPLAY_WIDTH, y + 1, flicker_buf));
            }

            free(flicker_buf);
        }
    }
}

void app_main(void)
{
    // Initialize display
    init_display();

    // Draw monoscope TV test pattern
    draw_monoscope_pattern();

    ESP_LOGI(TAG, "Monoscope TV test pattern displayed. Starting old TV simulation...");

    // Main loop with random TV effects
    while (1) {
        // Random wait between 100ms and 2 seconds
        int random_delay = 100 + (esp_random() % 1900);
        vTaskDelay(pdMS_TO_TICKS(random_delay));

        // Randomly choose an effect
        uint32_t effect = esp_random() % 10;

        if (effect < 6) {
            // 60% chance - Add TV noise/static
            add_tv_noise();
        } else if (effect < 8) {
            // 20% chance - TV flicker
            simulate_tv_flicker();
        } else {
            // 20% chance - Redraw clean pattern (like signal stabilizing)
            draw_monoscope_pattern();
        }

        // Brief pause between effects
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}