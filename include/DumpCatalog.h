#ifndef DUMP_CATALOG_H
#define DUMP_CATALOG_H

#include <Arduino.h>
#include <FS.h>

constexpr uint8_t MAX_DUMPS = 64;

struct DumpInfo
{
  String path;
  String brand;
  String page;
  String pinout;
  String note;
  uint16_t size = 0;
  uint8_t crum = 0;
  bool valid = false;
};

class DumpCatalog
{
public:
  bool begin(int8_t csPin, int8_t sckPin, int8_t misoPin, int8_t mosiPin);
  uint8_t scan(const char *rootPath = "/dumps");
  uint8_t count() const;
  const DumpInfo &get(uint8_t index) const;
  File openDump(uint8_t index, const char *mode = FILE_READ);
  File openPath(const char *path, const char *mode);
  bool ready() const;
  String lastError() const;

private:
  DumpInfo _items[MAX_DUMPS];
  uint8_t _count = 0;
  bool _ready = false;
  String _lastError;

  void scanDir(File dir);
  bool addFile(const String &path, size_t fileSize);
  DumpInfo parseFilename(const String &path, size_t fileSize) const;
  String tokenFromEnd(const String parts[], int count, int fromEnd) const;
};

#endif
