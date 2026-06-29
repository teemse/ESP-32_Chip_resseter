#include "DumpCatalog.h"

#include <SD.h>
#include <SPI.h>

namespace
{
SPIClass sdSpi(VSPI);

String lowerString(String value)
{
  value.toLowerCase();
  return value;
}

String upperString(String value)
{
  value.toUpperCase();
  return value;
}

String cleanToken(String value)
{
  value.replace('-', ' ');
  value.replace('_', ' ');
  value.trim();
  return value;
}
}

bool DumpCatalog::begin(int8_t csPin, int8_t sckPin, int8_t misoPin, int8_t mosiPin)
{
  sdSpi.begin(sckPin, misoPin, mosiPin, csPin);
  _ready = SD.begin(csPin, sdSpi);
  _lastError = _ready ? "" : "SD init failed";
  return _ready;
}

uint8_t DumpCatalog::scan(const char *rootPath)
{
  _count = 0;
  if (!_ready)
  {
    _lastError = "SD not ready";
    return 0;
  }

  File root = SD.open(rootPath);
  if (!root || !root.isDirectory())
  {
    _lastError = String(rootPath) + " missing";
    return 0;
  }

  scanDir(root);
  _lastError = _count > 0 ? "" : "No dumps";
  return _count;
}

uint8_t DumpCatalog::count() const
{
  return _count;
}

const DumpInfo &DumpCatalog::get(uint8_t index) const
{
  static DumpInfo empty;
  if (index >= _count)
  {
    return empty;
  }
  return _items[index];
}

File DumpCatalog::openDump(uint8_t index, const char *mode)
{
  if (index >= _count)
  {
    return File();
  }
  return SD.open(_items[index].path, mode);
}

File DumpCatalog::openPath(const char *path, const char *mode)
{
  return SD.open(path, mode);
}

bool DumpCatalog::ready() const
{
  return _ready;
}

String DumpCatalog::lastError() const
{
  return _lastError;
}

void DumpCatalog::scanDir(File dir)
{
  while (_count < MAX_DUMPS)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      break;
    }

    String path = entry.path();
    if (entry.isDirectory())
    {
      scanDir(entry);
    }
    else if (lowerString(path).endsWith(".bin"))
    {
      addFile(path, entry.size());
    }
    entry.close();
  }
}

bool DumpCatalog::addFile(const String &path, size_t fileSize)
{
  if (_count >= MAX_DUMPS)
  {
    return false;
  }

  DumpInfo info = parseFilename(path, fileSize);
  if (!info.valid)
  {
    return false;
  }

  _items[_count++] = info;
  return true;
}

DumpInfo DumpCatalog::parseFilename(const String &path, size_t fileSize) const
{
  DumpInfo info;
  info.path = path;
  info.size = static_cast<uint16_t>(fileSize);

  int slash = path.lastIndexOf('/');
  String filename = slash >= 0 ? path.substring(slash + 1) : path;
  String folder = "/";
  if (slash > 0)
  {
    int previousSlash = path.lastIndexOf('/', slash - 1);
    folder = previousSlash >= 0 ? path.substring(previousSlash + 1, slash) : path.substring(0, slash);
  }

  if (!lowerString(filename).endsWith(".bin"))
  {
    return info;
  }

  filename = filename.substring(0, filename.length() - 4);

  String parts[16];
  int partCount = 0;
  int start = 0;
  for (int i = 0; i <= filename.length() && partCount < 16; i++)
  {
    if (i == filename.length() || filename.charAt(i) == '_')
    {
      parts[partCount++] = filename.substring(start, i);
      start = i + 1;
    }
  }

  if (partCount < 5)
  {
    return info;
  }

  String crumToken = tokenFromEnd(parts, partCount, 0);
  if (crumToken.length() >= 2 && (crumToken[0] == 'c' || crumToken[0] == 'C'))
  {
    info.crum = crumToken.substring(1).toInt();
  }

  uint16_t sizeFromName = tokenFromEnd(parts, partCount, 1).toInt();
  if (sizeFromName > 0)
  {
    info.size = sizeFromName;
  }

  info.pinout = upperString(tokenFromEnd(parts, partCount, 2));
  info.page = upperString(tokenFromEnd(parts, partCount, 3));
  info.brand = folder != "/" && folder.length() > 0 ? upperString(folder) : upperString(parts[0]);

  String note;
  for (int i = 1; i < partCount - 4; i++)
  {
    if (note.length() > 0)
    {
      note += ' ';
    }
    note += parts[i];
  }
  info.note = cleanToken(note.length() > 0 ? note : filename);

  info.valid = info.size > 0 && info.pinout.length() > 0;
  return info;
}

String DumpCatalog::tokenFromEnd(const String parts[], int count, int fromEnd) const
{
  int index = count - 1 - fromEnd;
  if (index < 0 || index >= count)
  {
    return "";
  }
  return parts[index];
}
