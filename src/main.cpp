#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "ChipProgrammer.h"
#include "DumpCatalog.h"

#ifndef CHIP_SDA_PIN
#define CHIP_SDA_PIN 27
#endif

#ifndef CHIP_SCL_PIN
#define CHIP_SCL_PIN 22
#endif

#ifndef CHIP_POWER_PIN
#define CHIP_POWER_PIN 4
#endif

#ifndef CHIP_RANDOM_PIN
#define CHIP_RANDOM_PIN 34
#endif

#ifndef SD_CS_PIN
#define SD_CS_PIN 5
#endif

#ifndef SD_SCK_PIN
#define SD_SCK_PIN 18
#endif

#ifndef SD_MISO_PIN
#define SD_MISO_PIN 19
#endif

#ifndef SD_MOSI_PIN
#define SD_MOSI_PIN 23
#endif

#ifndef XPT2046_IRQ
#define XPT2046_IRQ 36
#endif

#ifndef XPT2046_MOSI
#define XPT2046_MOSI 32
#endif

#ifndef XPT2046_MISO
#define XPT2046_MISO 39
#endif

#ifndef XPT2046_CLK
#define XPT2046_CLK 25
#endif

#ifndef XPT2046_CS
#define XPT2046_CS 33
#endif

#define BG 0x0000
#define PANEL 0x18C3
#define PANEL2 0x0C1B
#define BORDER 0x2945
#define BLUE 0x05FF
#define GREEN 0x07E0
#define ORANGE 0xFD20
#define PURPLE 0xA81F
#define RED 0xF800
#define TEXT 0xFFFF
#define DIM 0x7BEF

#define W 320
#define H 240
#define HEADER_H 28
#define SIDEBAR_W 70

TFT_eSPI tft;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);
ChipProgrammer programmer;
DumpCatalog catalog;

uint8_t selectedDump = 0;
uint8_t chipAddress = 0;
bool chipFound = false;
bool sdReady = false;
String statusText = "Booting";
String serialText = "";
uint8_t preview[64];
uint16_t previewSize = 0;
uint32_t lastTouchMs = 0;

struct Button
{
  int x;
  int y;
  int w;
  int h;
  const char *label;
  uint16_t color;
};

Button buttons[] = {
    {75, 195, 55, 35, "READ", BLUE},
    {135, 195, 55, 35, "WRITE", GREEN},
    {195, 195, 55, 35, "VER", PURPLE},
    {255, 195, 55, 35, "SAVE", ORANGE},
};

String hex2(uint8_t value)
{
  char out[3];
  snprintf(out, sizeof(out), "%02X", value);
  return String(out);
}

String addressText()
{
  if (!chipFound)
  {
    return "--";
  }

  char out[5];
  snprintf(out, sizeof(out), "0x%02X", chipAddress);
  return String(out);
}

const DumpInfo &currentDump()
{
  return catalog.get(selectedDump);
}

void setStatus(const String &text)
{
  statusText = text;
  Serial.println(text);
}

void drawHeader()
{
  tft.fillRect(0, 0, W, HEADER_H, PANEL);
  tft.setTextColor(TEXT, PANEL);
  tft.drawString("CARTRIDGE PROGRAMMER", 6, 7, 2);

  tft.setTextColor(sdReady ? GREEN : RED, PANEL);
  tft.drawString(sdReady ? "SD" : "NO SD", 228, 7, 2);
  tft.setTextColor(chipFound ? GREEN : DIM, PANEL);
  tft.drawString(addressText(), 272, 7, 2);
}

void drawSidebar()
{
  tft.fillRoundRect(0, HEADER_H, SIDEBAR_W, H - HEADER_H, 6, PANEL);

  tft.setTextColor(TEXT, PANEL);
  tft.drawString("MENU", 10, 35, 2);

  tft.setTextColor(BLUE, PANEL);
  tft.drawString("PREV", 10, 72, 2);

  tft.setTextColor(GREEN, PANEL);
  tft.drawString("NEXT", 10, 107, 2);

  tft.setTextColor(PURPLE, PANEL);
  tft.drawString("DUMPS", 10, 145, 2);

  tft.setTextColor(DIM, PANEL);
  String countText = String(catalog.count());
  tft.drawString(countText, 10, 170, 2);
}

void drawChipInfo()
{
  int x = SIDEBAR_W + 5;
  int y = HEADER_H + 5;
  const DumpInfo &dump = currentDump();

  tft.fillRoundRect(x, y, 240, 55, 6, PANEL);
  tft.setTextColor(TEXT, PANEL);
  tft.drawString("MODEL:", x + 6, y + 6, 2);
  tft.drawString("TYPE:", x + 6, y + 24, 2);
  tft.drawString("ID:", x + 6, y + 42, 2);

  tft.setTextColor(BLUE, PANEL);
  if (dump.valid)
  {
    String note = dump.note;
    if (note.length() > 18)
    {
      note = note.substring(0, 18);
    }

    tft.drawString(note, x + 70, y + 6, 2);
    tft.drawString(dump.brand + " " + dump.page, x + 70, y + 24, 2);
    tft.drawString(String(dump.size) + "B C" + String(dump.crum), x + 70, y + 42, 2);
  }
  else
  {
    tft.drawString("No BIN selected", x + 70, y + 6, 2);
    tft.drawString("SD /dumps", x + 70, y + 24, 2);
    tft.drawString("-", x + 70, y + 42, 2);
  }
}

void drawHex()
{
  int x = SIDEBAR_W + 5;
  int y = HEADER_H + 65;

  tft.fillRoundRect(x, y, 240, 90, 6, PANEL);
  tft.setTextColor(BLUE, PANEL);

  if (previewSize == 0)
  {
    tft.setTextColor(DIM, PANEL);
    tft.drawString("No data preview", x + 6, y + 10, 1);
    return;
  }

  for (int row = 0; row < 4; row++)
  {
    uint16_t offset = row * 8;
    if (offset >= previewSize)
    {
      break;
    }

    char line[42];
    snprintf(line, sizeof(line), "%04X  ", offset);
    String text = line;
    for (int i = 0; i < 8 && offset + i < previewSize; i++)
    {
      text += hex2(preview[offset + i]);
      text += ' ';
    }
    tft.drawString(text, x + 6, y + 10 + row * 15, 1);
  }

  tft.setTextColor(DIM, PANEL);
  tft.drawString(statusText.substring(0, 32), x + 6, y + 72, 1);
}

void drawButtons()
{
  for (Button &button : buttons)
  {
    tft.fillRoundRect(button.x, button.y, button.w, button.h, 5, button.color);
    tft.setTextColor(BG, button.color);
    int tx = button.x + 6;
    if (strcmp(button.label, "VER") == 0)
    {
      tx = button.x + 15;
    }
    tft.drawString(button.label, tx, button.y + 10, 2);
  }
}

void drawStatusFooter()
{
  tft.fillRect(SIDEBAR_W + 5, 160, 240, 28, BG);
  tft.setTextColor(serialText.length() > 0 ? GREEN : DIM, BG);
  String text = serialText.length() > 0 ? "SN: " + serialText : statusText;
  if (text.length() > 32)
  {
    text = text.substring(0, 32);
  }
  tft.drawString(text, SIDEBAR_W + 10, 168, 1);
}

void drawUI()
{
  tft.fillScreen(BG);
  drawHeader();
  drawSidebar();
  drawChipInfo();
  drawHex();
  drawStatusFooter();
  drawButtons();
}

bool loadDumpPreview()
{
  previewSize = 0;
  if (!sdReady || catalog.count() == 0)
  {
    return false;
  }

  File file = catalog.openDump(selectedDump);
  if (!file)
  {
    setStatus("Cannot open dump");
    return false;
  }

  previewSize = file.read(preview, sizeof(preview));
  file.close();
  return previewSize > 0;
}

bool ensureChip()
{
  programmer.powerOn();
  chipFound = programmer.findAddress(chipAddress);
  if (!chipFound)
  {
    setStatus("No I2C chip found");
    programmer.powerOff();
  }
  return chipFound;
}

void readChip()
{
  const DumpInfo &dump = currentDump();
  uint16_t size = dump.valid ? dump.size : 256;

  setStatus("Reading chip");
  if (!ensureChip())
  {
    drawUI();
    return;
  }

  previewSize = min<uint16_t>(sizeof(preview), size);
  ChipStatus result = programmer.readToBuffer(chipAddress, previewSize, preview, sizeof(preview));
  programmer.powerOff();
  setStatus(result.ok ? "Chip preview read" : result.message);
  drawUI();
}

void writeDump()
{
  const DumpInfo &dump = currentDump();
  if (!dump.valid)
  {
    setStatus("Select BIN first");
    drawUI();
    return;
  }

  setStatus("Writing dump");
  if (!ensureChip())
  {
    drawUI();
    return;
  }

  File file = catalog.openDump(selectedDump);
  if (!file)
  {
    setStatus("Cannot open dump");
    programmer.powerOff();
    drawUI();
    return;
  }

  ChipStatus result = programmer.writeFile(chipAddress, dump.size, file);
  file.close();

  if (result.ok && dump.crum > 0)
  {
    programmer.applyCrum(chipAddress, dump.crum, dump.size, serialText);
  }

  programmer.powerOff();
  setStatus(result.message);
  loadDumpPreview();
  drawUI();
}

void verifyDump()
{
  const DumpInfo &dump = currentDump();
  if (!dump.valid)
  {
    setStatus("Select BIN first");
    drawUI();
    return;
  }

  setStatus("Verifying");
  if (!ensureChip())
  {
    drawUI();
    return;
  }

  File file = catalog.openDump(selectedDump);
  if (!file)
  {
    setStatus("Cannot open dump");
    programmer.powerOff();
    drawUI();
    return;
  }

  ChipStatus result = programmer.verifyFile(chipAddress, dump.size, file);
  file.close();
  programmer.powerOff();

  setStatus(result.ok ? "Verify OK" : String("Errors: ") + result.errors);
  drawUI();
}

void saveChip()
{
  const DumpInfo &dump = currentDump();
  uint16_t size = dump.valid ? dump.size : 256;

  setStatus("Saving chip");
  if (!ensureChip())
  {
    drawUI();
    return;
  }

  String path = "/reads/read_" + String(millis()) + "_" + String(size) + ".bin";
  File dir = catalog.openPath("/reads", FILE_READ);
  if (!dir)
  {
    SD.mkdir("/reads");
  }
  else
  {
    dir.close();
  }

  File file = catalog.openPath(path.c_str(), FILE_WRITE);
  if (!file)
  {
    setStatus("Cannot create file");
    programmer.powerOff();
    drawUI();
    return;
  }

  ChipStatus result = programmer.saveToFile(chipAddress, size, file);
  file.close();
  programmer.powerOff();

  setStatus(result.ok ? "Saved " + path : result.message);
  drawUI();
}

void selectDump(int direction)
{
  if (catalog.count() == 0)
  {
    setStatus("No dumps on SD");
    drawUI();
    return;
  }

  selectedDump = (selectedDump + catalog.count() + direction) % catalog.count();
  serialText = "";
  loadDumpPreview();
  setStatus("Selected " + String(selectedDump + 1) + "/" + String(catalog.count()));
  drawUI();
}

bool touchedPoint(int &x, int &y)
{
  if (!touch.touched())
  {
    return false;
  }

  TS_Point p = touch.getPoint();
  x = map(p.x, 200, 3900, 0, W);
  y = map(p.y, 200, 3900, 0, H);
  x = constrain(x, 0, W - 1);
  y = constrain(y, 0, H - 1);
  return true;
}

bool inside(const Button &button, int x, int y)
{
  return x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h;
}

void handleTouch()
{
  int x = 0;
  int y = 0;
  if (!touchedPoint(x, y))
  {
    return;
  }

  if (millis() - lastTouchMs < 250)
  {
    return;
  }
  lastTouchMs = millis();

  if (x < SIDEBAR_W)
  {
    if (y < 100)
    {
      selectDump(-1);
    }
    else
    {
      selectDump(1);
    }
    return;
  }

  for (uint8_t i = 0; i < 4; i++)
  {
    if (inside(buttons[i], x, y))
    {
      switch (i)
      {
      case 0:
        readChip();
        break;
      case 1:
        writeDump();
        break;
      case 2:
        verifyDump();
        break;
      case 3:
        saveChip();
        break;
      }
      return;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  programmer.begin(CHIP_SDA_PIN, CHIP_SCL_PIN, CHIP_POWER_PIN, CHIP_RANDOM_PIN);
  sdReady = catalog.begin(SD_CS_PIN, SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  if (sdReady)
  {
    catalog.scan("/dumps");
    loadDumpPreview();
    setStatus(catalog.count() > 0 ? "SD ready" : catalog.lastError());
  }
  else
  {
    setStatus(catalog.lastError());
  }

  drawUI();
}

void loop()
{
  handleTouch();
}
