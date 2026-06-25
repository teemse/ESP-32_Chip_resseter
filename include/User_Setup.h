#ifndef USER_SETUP_H
#define USER_SETUP_H

// Драйвер дисплея
#define ILI9341_DRIVER

// Размеры дисплея
#define TFT_WIDTH  320
#define TFT_HEIGHT 240

// Пины для CYD (ESP32-2432S028)
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_MOSI 13
#define TFT_MISO 12
#define TFT_SCLK 14
#define TFT_BL   21

// Пины тачскрина (XPT2046 / H2112)
#define TOUCH_CS 33
// T_CLK = TFT_SCLK (14)
// T_DIN = TFT_MOSI (13)
// T_DOUT = TFT_MISO (12)
// T_IRQ = 36 (не используется в этом примере)

// Частоты SPI
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Шрифты
#define SMOOTH_FONT

#endif