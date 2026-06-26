#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

// ================= COLORS =================
#define BG          0x0000
#define PANEL       0x18C3
#define PANEL2      0x0C1B
#define BORDER      0x2945

#define BLUE        0x05FF
#define GREEN       0x07E0
#define ORANGE      0xFD20
#define PURPLE      0xA81F

#define TEXT        0xFFFF
#define DIM         0x7BEF

// ================= UI LAYOUT =================
#define W 320
#define H 240

#define HEADER_H 28
#define SIDEBAR_W 70
#define FOOTER_H 45

// ================= DRAW HELPERS =================
void drawHeader()
{
    tft.fillRect(0, 0, W, HEADER_H, PANEL);

    tft.setTextColor(TEXT, PANEL);
    tft.drawString("CARTRIDGE PROGRAMMER", 6, 7, 2);

    tft.setTextColor(DIM, PANEL);
    tft.drawString("USB", 240, 7, 2);
    tft.drawString("3.3V", 280, 7, 2);
}

void drawSidebar()
{
    tft.fillRoundRect(0, HEADER_H, SIDEBAR_W, H - HEADER_H, 6, PANEL);

    tft.setTextColor(TEXT, PANEL);
    tft.drawString("MENU", 10, 35, 2);

    tft.setTextColor(BLUE, PANEL);
    tft.drawString("READ", 10, 70, 2);

    tft.setTextColor(GREEN, PANEL);
    tft.drawString("WRITE", 10, 100, 2);

    tft.setTextColor(PURPLE, PANEL);
    tft.drawString("VERIFY", 10, 130, 2);

    tft.setTextColor(ORANGE, PANEL);
    tft.drawString("SAVE", 10, 160, 2);
}

void drawChipInfo()
{
    int x = SIDEBAR_W + 5;
    int y = HEADER_H + 5;

    tft.fillRoundRect(x, y, 240, 55, 6, PANEL);

    tft.setTextColor(TEXT, PANEL);
    tft.drawString("MODEL:", x + 6, y + 6, 2);
    tft.drawString("ID:", x + 6, y + 42, 2);
    tft.drawString("TYPE:", x + 6, y + 24, 2);
    tft.setTextColor(BLUE, PANEL);
    tft.drawString("HP 85A", x + 70, y + 6, 2);
    tft.drawString("HP Auto", x + 70, y + 24, 2);
    tft.drawString("0x1FC7", x + 70, y + 42, 2);
}

void drawHex()
{
    int x = SIDEBAR_W + 5;
    int y = HEADER_H + 65;

    tft.fillRoundRect(x, y, 240, 90, 6, PANEL);

    tft.setTextColor(BLUE, PANEL);

    tft.drawString("0000  7F 1C 85 A0 00 00 03 E8", x + 6, y + 10, 1);
    tft.drawString("0010  00 00 64 00 00 00 00 00", x + 6, y + 25, 1);
    tft.drawString("0020  00 00 00 00 00 00 00 00", x + 6, y + 40, 1);
    tft.drawString("0030  FF FF FF FF FF FF FF FF", x + 6, y + 55, 1);
}

void drawButtons()
{
    int y = 195;

    tft.fillRoundRect(75, y, 55, 35, 5, BLUE);
    tft.fillRoundRect(135, y, 55, 35, 5, GREEN);
    tft.fillRoundRect(195, y, 55, 35, 5, PURPLE);
    tft.fillRoundRect(255, y, 55, 35, 5, ORANGE);

    tft.setTextColor(BG);

    tft.drawString("READ", 82, y + 10, 2);
    tft.drawString("WRITE", 140, y + 10, 2);
    tft.drawString("VER", 210, y + 10, 2);
    tft.drawString("SAVE", 265, y + 10, 2);
}

// ================= MAIN SCREEN =================
void drawUI()
{
    tft.fillScreen(BG);

    drawHeader();
    drawSidebar();
    drawChipInfo();
    drawHex();
    drawButtons();
}

// ================= SETUP =================
void setup()
{
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(BG);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    drawUI();
}

// ================= LOOP =================
void loop()
{
    // пока ничего — только статический UI
}