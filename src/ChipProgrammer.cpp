#include "ChipProgrammer.h"

#include <Wire.h>

void ChipProgrammer::begin(int sdaPin, int sclPin, int powerPin, int randomPin)
{
  _powerPin = powerPin;
  _randomPin = randomPin;

  Wire.begin(sdaPin, sclPin);
  Wire.setClock(100000);

  if (_powerPin >= 0)
  {
    pinMode(_powerPin, OUTPUT);
    digitalWrite(_powerPin, LOW);
  }

  if (_randomPin >= 0)
  {
    randomSeed(analogRead(_randomPin));
  }
  else
  {
    randomSeed(micros());
  }
}

void ChipProgrammer::powerOn()
{
  if (_powerPin >= 0)
  {
    digitalWrite(_powerPin, HIGH);
    delay(500);
  }
}

void ChipProgrammer::powerOff()
{
  if (_powerPin >= 0)
  {
    digitalWrite(_powerPin, LOW);
  }
}

bool ChipProgrammer::findAddress(uint8_t &address)
{
  for (uint8_t candidate = 1; candidate < 128; candidate++)
  {
    Wire.beginTransmission(candidate);
    uint8_t error = Wire.endTransmission();

    if (error == 0)
    {
      address = candidate;
      return true;
    }

    if (error != 2)
    {
      delay(10);
    }
  }

  return false;
}

uint8_t ChipProgrammer::readByte(uint8_t address, uint16_t cell, uint16_t dumpSize)
{
  uint8_t calculatedAddress = deviceAddress(address, cell, dumpSize);

  Wire.beginTransmission(calculatedAddress);
  writeRegister(cell, dumpSize);
  if (Wire.endTransmission(false) != 0)
  {
    return 0;
  }

  Wire.requestFrom(calculatedAddress, static_cast<uint8_t>(1));
  if (Wire.available())
  {
    return Wire.read();
  }

  return 0;
}

bool ChipProgrammer::writeByte(uint8_t address, uint16_t cell, uint8_t data, uint16_t dumpSize)
{
  uint8_t calculatedAddress = deviceAddress(address, cell, dumpSize);

  Wire.beginTransmission(calculatedAddress);
  writeRegister(cell, dumpSize);
  Wire.write(data);
  uint8_t error = Wire.endTransmission();
  delay(5);

  return error == 0;
}

ChipStatus ChipProgrammer::readToBuffer(uint8_t address, uint16_t dumpSize, uint8_t *buffer, size_t bufferSize)
{
  ChipStatus status{false, "Buffer too small", 0, 0};
  if (bufferSize < dumpSize)
  {
    return status;
  }

  for (uint16_t i = 0; i < dumpSize; i++)
  {
    buffer[i] = readByte(address, i, dumpSize);
    status.processed = i + 1;
  }

  status.ok = true;
  status.message = "Read complete";
  return status;
}

ChipStatus ChipProgrammer::writeFile(uint8_t address, uint16_t dumpSize, File &file)
{
  ChipStatus status{false, "Write failed", 0, 0};
  uint8_t pageSize = pageSizeFor(dumpSize);
  uint8_t page[16];

  file.seek(0);
  for (uint16_t offset = 0; offset < dumpSize; offset += pageSize)
  {
    size_t wanted = min<uint16_t>(pageSize, dumpSize - offset);
    size_t got = file.read(page, wanted);
    if (got != wanted)
    {
      status.message = "Dump read error";
      return status;
    }

    uint8_t calculatedAddress = deviceAddress(address, offset, dumpSize);
    Wire.beginTransmission(calculatedAddress);
    writeRegister(offset, dumpSize);
    Wire.write(page, wanted);

    if (Wire.endTransmission() != 0)
    {
      status.errors++;
      status.message = "I2C write error";
      return status;
    }

    delay(10);
    status.processed = offset + wanted;
  }

  status.ok = true;
  status.message = "Write complete";
  return status;
}

ChipStatus ChipProgrammer::verifyFile(uint8_t address, uint16_t dumpSize, File &file)
{
  ChipStatus status{true, "Verify OK", 0, 0};
  file.seek(0);

  for (uint16_t i = 0; i < dumpSize; i++)
  {
    int expected = file.read();
    if (expected < 0)
    {
      status.ok = false;
      status.message = "Dump read error";
      return status;
    }

    uint8_t actual = readByte(address, i, dumpSize);
    if (actual != static_cast<uint8_t>(expected))
    {
      status.errors++;
    }

    status.processed = i + 1;
  }

  if (status.errors > 0)
  {
    status.ok = false;
    status.message = "Verify mismatch";
  }

  return status;
}

ChipStatus ChipProgrammer::saveToFile(uint8_t address, uint16_t dumpSize, File &file)
{
  ChipStatus status{false, "Save failed", 0, 0};

  for (uint16_t i = 0; i < dumpSize; i++)
  {
    uint8_t value = readByte(address, i, dumpSize);
    if (file.write(&value, 1) != 1)
    {
      status.message = "SD write error";
      return status;
    }
    status.processed = i + 1;
  }

  status.ok = true;
  status.message = "Saved";
  return status;
}

void ChipProgrammer::applyCrum(uint8_t address, uint8_t crumMode, uint16_t dumpSize, String &serialOut)
{
  serialOut = "";

  switch (crumMode)
  {
  case 1:
    changeCrumRange(address, dumpSize, 56, 63);
    serialOut = readAsciiRange(address, dumpSize, 53, 63);
    break;
  case 2:
    for (int i = 0; i < 6; i++)
    {
      uint8_t value = random(48, 58);
      writeByte(address, 63 - i, value, dumpSize);
      writeByte(address, 191 - i, value, dumpSize);
    }
    serialOut = readAsciiRange(address, dumpSize, 53, 63);
    break;
  case 3:
    changeCrumRange(address, dumpSize, 22, 23);
    serialOut = readAsciiRange(address, dumpSize, 22, 23);
    break;
  case 4:
    changeCrumRange(address, dumpSize, 60, 73);
    serialOut = readAsciiRange(address, dumpSize, 60, 73);
    break;
  case 5:
    changeCrumRange(address, dumpSize, 52, 65);
    serialOut = readAsciiRange(address, dumpSize, 52, 65);
    break;
  default:
    break;
  }
}

uint8_t ChipProgrammer::deviceAddress(uint8_t baseAddress, uint16_t cell, uint16_t dumpSize) const
{
  (void)dumpSize;
  return baseAddress + (cell / 256);
}

void ChipProgrammer::writeRegister(uint16_t cell, uint16_t dumpSize)
{
  if (dumpSize > 256)
  {
    Wire.write(highByte(cell));
    Wire.write(lowByte(cell));
  }
  else
  {
    Wire.write(lowByte(cell));
  }
}

uint8_t ChipProgrammer::pageSizeFor(uint16_t dumpSize) const
{
  return dumpSize <= 256 ? 8 : 16;
}

void ChipProgrammer::changeCrumRange(uint8_t address, uint16_t dumpSize, int from, int to)
{
  for (int i = from; i <= to; i++)
  {
    writeByte(address, i, random(48, 58), dumpSize);
  }
}

String ChipProgrammer::readAsciiRange(uint8_t address, uint16_t dumpSize, int from, int to)
{
  String value;
  for (int i = from; i <= to; i++)
  {
    char c = static_cast<char>(readByte(address, i, dumpSize));
    value += c >= 32 && c <= 126 ? c : ' ';
  }
  value.trim();
  return value;
}
