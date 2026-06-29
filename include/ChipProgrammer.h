#ifndef CHIP_PROGRAMMER_H
#define CHIP_PROGRAMMER_H

#include <Arduino.h>
#include <FS.h>

struct ChipStatus
{
  bool ok;
  String message;
  uint16_t processed;
  uint16_t errors;
};

class ChipProgrammer
{
public:
  void begin(int sdaPin, int sclPin, int powerPin, int randomPin);
  void powerOn();
  void powerOff();

  bool findAddress(uint8_t &address);
  uint8_t readByte(uint8_t address, uint16_t cell, uint16_t dumpSize);
  bool writeByte(uint8_t address, uint16_t cell, uint8_t data, uint16_t dumpSize);

  ChipStatus readToBuffer(uint8_t address, uint16_t dumpSize, uint8_t *buffer, size_t bufferSize);
  ChipStatus writeFile(uint8_t address, uint16_t dumpSize, File &file);
  ChipStatus verifyFile(uint8_t address, uint16_t dumpSize, File &file);
  ChipStatus saveToFile(uint8_t address, uint16_t dumpSize, File &file);

  void applyCrum(uint8_t address, uint8_t crumMode, uint16_t dumpSize, String &serialOut);

private:
  int _powerPin = -1;
  int _randomPin = -1;

  uint8_t deviceAddress(uint8_t baseAddress, uint16_t cell, uint16_t dumpSize) const;
  void writeRegister(uint16_t cell, uint16_t dumpSize);
  uint8_t pageSizeFor(uint16_t dumpSize) const;
  void changeCrumRange(uint8_t address, uint16_t dumpSize, int from, int to);
  String readAsciiRange(uint8_t address, uint16_t dumpSize, int from, int to);
};

#endif
