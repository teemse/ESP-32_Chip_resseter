#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <SPI.h>

TFT_eSPI tft;

// ===== Параметры интерфейса =====
#define BGCOLOR    0x0000  // Черный
#define FGCOLOR    0x07E0  // Зеленый
#define ALCOLOR    0xF800  // Красный
#define ODD_COLOR  0x30c5  // Темно-бирюзовый
#define EVEN_COLOR 0x32e5  // Светло-бирюзовый

#define ICON_W      100
#define ICON_H      80
#define ICON_MARGIN 8
#define COLS        3
#define ROWS        2
#define ITEMS_PER_PAGE (COLS * ROWS)

// ===== Пины SD карты =====
#define SD_SCK  14
#define SD_MISO 12
#define SD_MOSI 13
#define SD_CS   15

// ===== Пины тачскрина =====
#define TOUCH_CS  33
#define TOUCH_IRQ 36

// ===== Структура для иконок (для определения зон нажатия) =====
struct IconZone {
    int x, y, w, h;
    int fileIndex;
};

// ===== Глобальные переменные =====
bool sdcardMounted = false;
std::vector<String> fileList;
std::vector<IconZone> iconZones;
int currentPage = 0;
int totalPages = 0;
int selectedIndex = -1;  // -1 = ничего не выбрано

// ===== Прототипы функций =====
void initSD();
void scanSD();
void drawMainMenu();
void drawIcon(int x, int y, int w, int h, const String& label, bool selected);
void launchApp(const String& filename);
void handleTouch();
void getTouchPoint(int &tx, int &ty);

// ====================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n\nCYD Touch Launcher Starting...");
    
    // Инициализация дисплея
    tft.init();
    tft.setRotation(1);  // Горизонтальная ориентация
    tft.fillScreen(BGCOLOR);
    
    // Настройка подсветки
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    
    // Настройка пинов тачскрина
    pinMode(TOUCH_CS, OUTPUT);
    digitalWrite(TOUCH_CS, HIGH);
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    
    // Приветственный экран
    tft.setTextColor(FGCOLOR, BGCOLOR);
    tft.setTextFont(4);
    tft.drawCentreString("CYD Launcher", tft.width()/2, 10, 1);
    tft.drawLine(0, 50, tft.width(), 50, FGCOLOR);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, BGCOLOR);
    tft.drawCentreString("Initializing...", tft.width()/2, tft.height()/2, 1);
    tft.drawCentreString("Touch Screen Ready", tft.width()/2, tft.height()/2 + 30, 1);
    
    // Инициализация SD карты
    initSD();
    
    // Сканирование файлов
    if (sdcardMounted) {
        scanSD();
    }
    
    // Рисуем главное меню
    drawMainMenu();
    
    Serial.println("Setup complete!");
}

// ====================================================================
void initSD() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS, SPI, 40000000)) {
        sdcardMounted = false;
        Serial.println("SD Card Mount Failed!");
        tft.fillScreen(BGCOLOR);
        tft.setTextColor(ALCOLOR, BGCOLOR);
        tft.drawCentreString("SD Card Error!", tft.width()/2, tft.height()/2, 2);
        return;
    }
    
    sdcardMounted = true;
    Serial.println("SD Card mounted successfully!");
    Serial.printf("Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
}

// ====================================================================
void scanSD() {
    fileList.clear();
    Serial.println("Scanning for .bin files...");
    
    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open root directory!");
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.startsWith("/")) name = name.substring(1);
            
            if (name.endsWith(".bin") || name.endsWith(".BIN")) {
                fileList.push_back(name);
                Serial.println("Found: " + name);
            }
        }
        file = root.openNextFile();
    }
    
    // Сортировка
    for (int i = 0; i < fileList.size(); i++) {
        for (int j = i + 1; j < fileList.size(); j++) {
            if (fileList[i] > fileList[j]) {
                String temp = fileList[i];
                fileList[i] = fileList[j];
                fileList[j] = temp;
            }
        }
    }
    
    totalPages = (fileList.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;
    
    Serial.printf("Found %d .bin files, %d pages\n", fileList.size(), totalPages);
}

// ====================================================================
void drawIcon(int x, int y, int w, int h, const String& label, bool selected) {
    uint16_t color = selected ? FGCOLOR : ((x + y) % 2 == 0 ? ODD_COLOR : EVEN_COLOR);
    
    tft.fillRoundRect(x, y, w, h, 8, BGCOLOR);
    
    if (selected) {
        tft.drawRoundRect(x, y, w, h, 8, color);
        tft.drawRoundRect(x+2, y+2, w-4, h-4, 8, color);
        // Подсветка фона для выбранного
        tft.fillRoundRect(x+4, y+4, w-8, h-8, 8, 0x1082);  // Темно-зеленый
    } else {
        tft.drawRoundRect(x, y, w, h, 8, color);
    }
    
    // Иконка
    tft.setTextColor(color, selected ? 0x1082 : BGCOLOR);
    tft.drawString("[SD]", x + 25, y + 15, 2);
    
    // Имя файла
    tft.setTextColor(selected ? FGCOLOR : TFT_WHITE, selected ? 0x1082 : BGCOLOR);
    tft.setTextFont(2);
    
    String shortName = label;
    if (shortName.endsWith(".bin") || shortName.endsWith(".BIN")) {
        shortName = shortName.substring(0, shortName.length() - 4);
    }
    if (shortName.length() > 10) {
        shortName = shortName.substring(0, 9) + "~";
    }
    
    tft.drawCentreString(shortName, x + w/2, y + h - 25, 1);
}

// ====================================================================
void drawMainMenu() {
    tft.fillScreen(BGCOLOR);
    iconZones.clear();
    
    // Заголовок
    tft.setTextColor(FGCOLOR, BGCOLOR);
    tft.setTextFont(4);
    tft.drawCentreString("SD Launcher", tft.width()/2, 5, 1);
    
    // Линия-разделитель
    tft.drawLine(0, 40, tft.width(), 40, FGCOLOR);
    
    // Информация
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, BGCOLOR);
    tft.drawString("Files: " + String(fileList.size()), 5, 45);
    
    String pageInfo = "Page " + String(currentPage + 1) + "/" + String(totalPages);
    tft.drawString(pageInfo, tft.width() - tft.textWidth(pageInfo) - 5, 45);
    
    if (!sdcardMounted) {
        tft.setTextColor(ALCOLOR, BGCOLOR);
        tft.drawCentreString("No SD Card!", tft.width()/2, tft.height()/2, 2);
        return;
    }
    
    if (fileList.size() == 0) {
        tft.setTextColor(TFT_YELLOW, BGCOLOR);
        tft.drawCentreString("No .bin files found!", tft.width()/2, tft.height()/2 - 20, 2);
        tft.drawCentreString("Put .bin files on SD card", tft.width()/2, tft.height()/2 + 20, 2);
        return;
    }
    
    // Кнопки навигации (вверх/вниз)
    int btnSize = 30;
    // Кнопка "Предыдущая страница"
    tft.fillTriangle(15, 60, 15 + btnSize, 45, 15 + btnSize, 75, 
                     currentPage > 0 ? FGCOLOR : TFT_DARKGREY);
    // Кнопка "Следующая страница"
    tft.fillTriangle(tft.width() - 15, 60, tft.width() - 15 - btnSize, 45, 
                     tft.width() - 15 - btnSize, 75, 
                     currentPage < totalPages - 1 ? FGCOLOR : TFT_DARKGREY);
    
    // Сетка иконок
    int totalWidth = COLS * ICON_W + (COLS - 1) * ICON_MARGIN;
    int startX = (tft.width() - totalWidth) / 2;
    int startY = 85;
    
    int startIdx = currentPage * ITEMS_PER_PAGE;
    int endIdx = min(startIdx + ITEMS_PER_PAGE, (int)fileList.size());
    
    for (int i = startIdx; i < endIdx; i++) {
        int pageIdx = i - startIdx;
        int col = pageIdx % COLS;
        int row = pageIdx / COLS;
        
        int x = startX + col * (ICON_W + ICON_MARGIN);
        int y = startY + row * (ICON_H + ICON_MARGIN + 15);
        
        bool selected = (i == selectedIndex);
        drawIcon(x, y, ICON_W, ICON_H, fileList[i], selected);
        
        // Сохраняем зону для тача
        iconZones.push_back({x, y, ICON_W, ICON_H, i});
    }
    
    // Подсказка
    tft.setTextFont(1);
    tft.setTextColor(TFT_DARKGREY, BGCOLOR);
    tft.drawCentreString("Touch icon to select, touch again to launch", tft.width()/2, 
                         tft.height() - 10, 1);
}

// ====================================================================
void getTouchPoint(int &tx, int &ty) {
    digitalWrite(TOUCH_CS, LOW);
    
    // Читаем X
    SPI.transfer(0x90);  // Команда чтения X
    uint16_t x = SPI.transfer16(0) >> 3;
    
    // Читаем Y
    SPI.transfer(0xD0);  // Команда чтения Y
    uint16_t y = SPI.transfer16(0) >> 3;
    
    digitalWrite(TOUCH_CS, HIGH);
    
    // Конвертация координат (зависит от ориентации)
    // Для rotation=1 на CYD:
    tx = map(x, 300, 3800, 0, tft.width());
    ty = map(y, 200, 3900, 0, tft.height());
    
    // Ограничиваем значения
    tx = constrain(tx, 0, tft.width());
    ty = constrain(ty, 0, tft.height());
}

// ====================================================================
void handleTouch() {
    static unsigned long lastTouch = 0;
    static bool wasTouched = false;
    static int lastSelectedIndex = -1;
    
    // Проверяем, нажат ли экран
    if (digitalRead(TOUCH_IRQ) == HIGH) {
        wasTouched = false;
        return;  // Экран не нажат
    }
    
    // Антидребезг
    if (millis() - lastTouch < 150) return;
    lastTouch = millis();
    
    int tx, ty;
    getTouchPoint(tx, ty);
    
    Serial.printf("Touch at: %d, %d\n", tx, ty);
    
    // Проверяем кнопки навигации
    if (ty < 85) {
        // Левая стрелка (предыдущая страница)
        if (tx < 60 && currentPage > 0) {
            currentPage--;
            selectedIndex = currentPage * ITEMS_PER_PAGE;
            drawMainMenu();
            return;
        }
        // Правая стрелка (следующая страница)
        if (tx > tft.width() - 60 && currentPage < totalPages - 1) {
            currentPage++;
            selectedIndex = currentPage * ITEMS_PER_PAGE;
            drawMainMenu();
            return;
        }
    }
    
    // Проверяем иконки
    for (auto &zone : iconZones) {
        if (tx >= zone.x && tx <= zone.x + zone.w &&
            ty >= zone.y && ty <= zone.y + zone.h) {
            
            if (zone.fileIndex == selectedIndex) {
                // Повторное нажатие = запуск
                if (zone.fileIndex < fileList.size()) {
                    launchApp(fileList[zone.fileIndex]);
                }
            } else {
                // Первое нажатие = выбор
                selectedIndex = zone.fileIndex;
                drawMainMenu();
                Serial.println("Selected: " + fileList[selectedIndex]);
            }
            return;
        }
    }
    
    // Нажатие мимо иконок = снять выделение
    if (selectedIndex != -1) {
        selectedIndex = -1;
        drawMainMenu();
    }
}

// ====================================================================
void launchApp(const String& filename) {
    tft.fillScreen(BGCOLOR);
    tft.setTextColor(FGCOLOR, BGCOLOR);
    tft.setTextFont(2);
    tft.drawCentreString("Launching:", tft.width()/2, tft.height()/2 - 30, 1);
    tft.drawCentreString(filename, tft.width()/2, tft.height()/2, 1);
    
    // Прогресс-бар
    tft.drawRect(60, tft.height()/2 + 40, tft.width() - 120, 20, FGCOLOR);
    for (int i = 0; i <= 100; i += 10) {
        tft.fillRect(62, tft.height()/2 + 42, (tft.width() - 124) * i / 100, 16, FGCOLOR);
        delay(200);
    }
    
    Serial.println("Launching: " + filename);
    Serial.println("TODO: Add OTA/flash functionality here");
    
    // Здесь будет код прошивки
    
    delay(1000);
    drawMainMenu();
}

// ====================================================================
void loop() {
    handleTouch();
    delay(10);
}