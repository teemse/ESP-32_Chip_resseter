#include <Arduino.h>
#include "DB.h"
// #include <I2C.h>
#include <Wire.h> // Вместо I2C.h
#include <EEPROM.h>
#define POWER_PIN 2                  // Пин питания может быть другой
#define RANDOM_PIN 3                 // Пин для работы генератора случайных чисел
byte global_address_eeprom;          // Адрес чипа (адрес динамический, меняется от чипа к чипу)
byte global_id = 0;                  // Номер чипа по умолчанию
int global_all_chip_in_database;     // Кол-во чипов в базе данных
const byte *global_name_dump;        // Имя дампа
int global_size_dump = 0;            // Размер чипа
int global_change_crum = 0;          // Номер функции которая помняет серийник 0 -- замена не нужна
boolean global_button_press = false; // true - кнопка нажата Состояние кнопки (защита от повторного срабатывания)

void power_on_for_chip()
{
  digitalWrite(POWER_PIN, HIGH); // Подаем питания на A2 для запитки чипа
  delay(500);                    // Задержка для поднятия напряжения
}

void power_off_for_chip()
{
  digitalWrite(POWER_PIN, LOW); // Выключаем питания на A2 пине
}

bool search_address_chip() // Возвращаем TRUE (1) eсли все ок и FALSE (0) если плохо
{
  // Сканируем шину I2C
  for (byte address = 0; address <= 127; address++)
  {
    // Начинаем передачу на адрес
    Wire.beginTransmission(address);

    // Завершаем передачу и получаем статус
    // endTransmission() возвращает:
    // 0 = успешно
    // 1 = данные слишком длинные
    // 2 = получен NACK при передаче адреса
    // 3 = получен NACK при передаче данных
    // 4 = другая ошибка
    // 5 = таймаут (на ESP32)
    byte error = Wire.endTransmission();

    // Если ошибок нет (0) - устройство ответило
    if (error == 0)
    {
      // Сохраняем адрес
      global_address_eeprom = address;
      Serial.print(F("Device found at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
      return true;
    }
    else if (error == 2) // NACK при передаче адреса - устройство не ответило
    {
      // Просто пропускаем, устройство не найдено по этому адресу
      continue;
    }
    else // Другие ошибки (проблемы с шиной)
    {
      Serial.print(F("I2C bus error at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print(F(" error code: "));
      Serial.println(error);

      // Не возвращаем false сразу, продолжаем сканирование
      // Но если это критично, можно добавить задержку
      delay(100);
    }
  }

  // Если ничего не найдено
  Serial.println(F("No I2C devices found"));
  return false;
}

byte ReadByte(byte AddressDevice, word NumberCell) // Считываем с чипа байт AddressDevice -- Адрес чипа NumberCell -- Номер ячейки
{
  byte CalculateAddress = AddressDevice + (NumberCell / 256);
  byte CalculateCell = NumberCell % 256;

  Wire.beginTransmission(CalculateAddress);

  // Отправляем регистр для чтения
  if (global_size_dump > 256)
  {
    Wire.write(highByte(NumberCell));
    Wire.write(lowByte(NumberCell));
  }
  else
  {
    Wire.write(CalculateCell);
  }

  Wire.endTransmission(false); // false = не отправлять STOP

  // Запрашиваем 1 байт
  Wire.requestFrom(CalculateAddress, (byte)1);

  if (Wire.available())
  {
    return Wire.read();
  }

  Serial.println(F("Error ReadByte"));
  return 0;
}

void verification_dump()
{
  Serial.print(F("VERIFICATION "));
  byte error = 0;                             // Кол-во ошибок
  for (word i = 0; i < global_size_dump; i++) // Цикл
  {
    if (global_name_dump[i] != ReadByte(global_address_eeprom, i)) // Сравниваем байты в памяти и чипе
    {
      error = error + 1; // Если нашли ошибку то увеличваем счетчик
    }
  }

  // Если ошибок нет то GOOD иначе ERROR
  if (error == 0)
  {
    Serial.println(F("GOOD"));
    delay(1000);
  }
  else
  {
    Serial.println(F("ERROR"));
    delay(2000);
  }
}

// Записываем чип по байтно, с любого места. AddressDevice -- Адрес чипа NumberCell -- Номер ячейки ByteData -- Байт информации
void WriteByte(byte AddressDevice, word NumberCell, byte ByteData)
{
  byte CalculateAddress = AddressDevice + (NumberCell / 256);
  byte CalculateCell = NumberCell % 256;

  Wire.beginTransmission(CalculateAddress);

  if (global_size_dump > 256) // Отправляем регистр
  {
    Wire.write(highByte(NumberCell));
    Wire.write(lowByte(NumberCell));
  }
  else
  {
    Wire.write(CalculateCell);
  }

  Wire.write(ByteData); // Отправляем данные

  Wire.endTransmission();
  delay(5);
}

void change_crum_universal(int From, int To) // Генератор crum универсальный указывается с какого байда и по какой байт поменять цифры
{
  for (int i = From; i <= To; i++) // Меняем байты от (FROM) и до (TO)
  {
    int randomNum = random(48, 57);                 // Генерируем ANSI (48-58) а в DEC (0-9)
    WriteByte(global_address_eeprom, i, randomNum); // Записываем значение в адрес
  }
}

void print_sn_on_lcd_universal(int From, int To) // Показать CRUM от (FROM) байта до (TO) байта
{
  Serial.print(F("NEW CRUM "));    // Показываем на дисплее надпись NEW CRUM
  for (int i = From; i <= To; i++) // Считываем байты от и до
  {
    char c = (char)ReadByte(global_address_eeprom, i); // получаем ascii из hex
    Serial.print(c);
  }
  Serial.println(F(" "));
  delay(1500);                           // Задержка для просмотра номера
  Serial.println(F("CHANGE CRUM GOOD")); // Сообщаем что все ок
}

// Генератора для Samsung или Xerox где надо сменить 2 номерa Младший разряд находится в 63 байте и в 191
void change_crum_two_xerox()
{
  // Переменные где хранится младший разряд
  int temp_sn_one = 63;
  int temp_sn_two = 191;
  for (int i = 6; i > 0; i--) //  меняем 6 младших разрядов серийника
  {
    // ANSI (48-58) а в DEC (0-9)
    int randomNum = random(48, 57);
    // Записываем значение в адрес temp_sn_one, CRUM 1
    WriteByte(global_address_eeprom, temp_sn_one, randomNum);
    // Записываем значение в адрес temp_sn_two, CRUM 2
    WriteByte(global_address_eeprom, temp_sn_two, randomNum);
    // переход к старшему разряду
    temp_sn_one--;
    temp_sn_two--;
  }

  // Показываем серийный номер на lcd
  print_sn_on_lcd_universal(53, 63);
  //
  Serial.println(F("CHANGE TWO CRUM GOOD"));
}

void read_chip_and_display_it()
{

  // Показываем 16 строк
  int byte_in_str = 16;
  //
  int sizeof_chip = global_size_dump;
  //
  int num_str_in_chip = sizeof_chip / byte_in_str;
  //
  for (int i_1 = 0; i_1 < num_str_in_chip; i_1++)
  {
    for (int i_2 = 0; i_2 < 17; i_2++)
    {
      // Получаем ascii
      char a = (char)ReadByte(global_address_eeprom, i_2 + i_1 * byte_in_str);
      char b; //
      if (a < 32)
      {
        b = 32;
      } // если a 0 то ставим пробел HEX(32)
      else
      {
        b = a;
      }
    }
    delay(500);
  }
}

void extract_dump(word SizeDump) // Калькулятор https://fischl.de/hex_checksum_calculator/
{

  // Подаем питание на чип
  power_on_for_chip();

  // сканируем шину i2c на наличие чипа
  if (search_address_chip())
  {
    // Количство строк в чипе
    word CalculateCycles = SizeDump / 16;

    Serial.println(F(""));
    Serial.println(F("DUMP CHIP FOR READ"));
    Serial.println(F(""));

    // Заголовок для HEX таблицы
    Serial.println(F("     | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0E 0D 0F"));
    Serial.println(F("- - - - - - - - - - - - - - - - - - - - - - - - - - - - "));

    // Показываем дамп считаный с чипа (для просмотра)
    for (int Cycle = 0; Cycle < CalculateCycles; Cycle++)
    {
      // Показываем HEX адреса колонок
      // Получаем адрес 2 байта
      word CalculateAddressForTable = Cycle * 16;
      // Берем старший байт это тот который слева
      byte HigherByte = highByte(CalculateAddressForTable);
      // Если это число меньше 16 то добавляем 0 спереди
      if (HigherByte < 16)
      {
        Serial.print("0");
      }
      Serial.print(HigherByte, HEX);
      // Берем младший байт это тот который справа
      byte LowerByte = lowByte(CalculateAddressForTable);
      // Если это число меньше 16 то добавляем 0 спереди и показываем в HEX
      if (LowerByte < 16)
      {
        Serial.print("0");
      }
      Serial.print(LowerByte, HEX);

      // Разделитель адресов и байтов
      Serial.print(F(" | "));

      // Цикл
      for (int i = 0; i < 16; i++)
      {
        // Получаем байт из чипа
        byte GetByte = ReadByte(global_address_eeprom, i + Cycle * 16);
        // Если это число меньше 16 то добавляем 0 спереди и показываем в HEX
        if (GetByte < 16)
        {
          Serial.print(F("0"));
        }
        Serial.print(GetByte, HEX);
        // Отступ
        Serial.print(F(" "));
      }
      // Переход на новую строку
      Serial.println(F(""));
    }

    Serial.println(F(""));
    Serial.println(F("DUMP CHIP FOR SAVE IN FILE .HEX"));
    Serial.println(F(""));
    Serial.println(F("START"));

    // Показываем дамп для HEX файла (для сохранения на пк)
    for (int Cycle = 0; Cycle < CalculateCycles; Cycle++)
    {
      // Переменная CheckSum
      word CheckSum = 0;
      // начало строки всегда с :
      Serial.print(F(":"));
      // 1-й байт сколько байт в строке у нас 16 в hex это 10
      Serial.print(F("10"));
      // 2 байта адрес в чипе 0000 0010 0020 0030 и т.д.
      word Number = Cycle * 16;
      // Берем старший байт это тот который слева
      byte HigherByte = highByte(Number);
      // Если это число меньше 16 то добавляем 0 спереди
      if (HigherByte < 16)
      {
        Serial.print("0");
      }
      Serial.print(HigherByte, HEX);
      // Берем младший байт это тот который справа
      byte LowerByte = lowByte(Number);
      // Если это число меньше 16 то добавляем 0 спереди и показываем в HEX
      if (LowerByte < 16)
      {
        Serial.print("0");
      }
      Serial.print(LowerByte, HEX);
      // 1 байт 00
      Serial.print(F("00"));
      // 16 байт дамп из чипа
      for (int i = 0; i < 16; i++)
      {
        // Получаем байт из чипа
        byte GetByte = ReadByte(global_address_eeprom, i + Cycle * 16);
        // Запоминаем сумму всех байт
        CheckSum = CheckSum + GetByte;
        // Если это число меньше 16 то добавляем 0 спереди и показываем в HEX
        if (GetByte < 16)
        {
          Serial.print("0");
        }
        Serial.print(GetByte, HEX);
      }
      // 1 байт CheckSum
      // CheckSum = 256 - сумма всех байтов (вся строка) и берем только байт (8 бит) из этого числа
      word CalculateCheckSum = 16 + HigherByte + LowerByte + CheckSum;
      // Получаем 1 байт CheckSum
      byte FinalCheckSum = 256 - CalculateCheckSum;
      // Если это число меньше 16 то добавляем 0 спереди и показываем в HEX
      if (FinalCheckSum < 16)
      {
        Serial.print("0");
      }
      Serial.print(FinalCheckSum, HEX);
      // Переход на новую строку
      Serial.println(F(""));
    }
    // Конец HEX файла
    Serial.println(F(":00000001FF"));
    Serial.println(F("END"));
  }

  // Включаем питание
  power_off_for_chip();

  // Возврат в меню
}

void countdown_timer(int timer)
{
  // int timer = 8; // Время которое ждем перед прошивкой чипа
  for (int i = timer; i > 0; i--)
  {
    delay(1000); // Задержка в 1 сек перед повтором цикла
  }
}

void total_pages_on_display_ricoh()
{
  // Подаем питание на чип
  power_on_for_chip();

  // сканируем шину i2c на наличие чипа
  if (search_address_chip())
  {
    byte HigherByte = ReadByte(global_address_eeprom, 65); // Считываем 65 байт это старшый разряд
    byte LowerByte = ReadByte(global_address_eeprom, 64);  // Считываем 64 байт это младший разряд
    int Result = (HigherByte << 8) | LowerByte;            // соединяем 2 разряда в одно

    // lcd.print(Result); // показываем число на экран

    delay(5000); // 5 секунд
  }

  // Выключаем питание
  power_off_for_chip();

  // Возврат в меню
}

// Подходит только для записи чипа полностью, а не с определенного байта.
// AddressDevice -- адрес чипа пример 0х50
// SizeDump -- размер дампа пример 128 512 1024 2048 или sizeof(NameDump)
// PointerToDump -- ссылка на дамп пример dump_ricoh
// Пример WriteBytesPages(0x50, sizeof(NameDump), NameDump);
void WriteBytesPages(byte AddressDevice, word SizeDump, const byte *PointerToDump)
{
  // Адрес чипа который меняется
  byte CalculateAddress = AddressDevice;
  // Serial.print("Start Address = "); Serial.println(CalculateAddress, HEX);

  // Храним размер страницы
  byte SizePage;

  // Размер страницы у 01-02 равен 8 байт, a у 04-16 равен 16 байт
  if (SizeDump <= 256)
  {
    SizePage = 8;
  }
  else
  {
    SizePage = 16;
  }

  // Сколько циклов надо
  byte CalculateCycles = SizeDump / SizePage;

  // Запускаем цикл
  for (byte Cycle = 0; Cycle < CalculateCycles; Cycle++)
  {
    // Массив для страницы
    byte Page[SizePage];

    // Заполняем страницу (массив)
    // memcpy записывает из PROGMEM в RAM arduino страницу
    memcpy(Page, &PointerToDump[Cycle * SizePage], sizeof(Page));
    // Каждые 256 байт меняем адрес чипа чтобы записать весь чип.
    if (Cycle != 0 && Cycle * SizePage % 256 == 0)
    {
      CalculateAddress++;
    }

    // Начинаем передачу на адрес устройства
    Wire.beginTransmission(CalculateAddress);

    // Отправляем регистр (номер ячейки)
    // Важно: для адресов > 255 нужно отправлять 2 байта!
    word reg = Cycle * SizePage;
    if (SizeDump > 256)
    {
      // Для чипов > 256 байт отправляем 2-байтовый регистр (старший, затем младший)
      Wire.write(highByte(reg));
      Wire.write(lowByte(reg));
    }
    else
    {
      // Для маленьких чипов достаточно 1 байта
      Wire.write(lowByte(reg));
    }

    // Отправляем данные
    Wire.write(Page, SizePage);

    // Завершаем передачу и проверяем ошибки
    byte error = Wire.endTransmission();
    if (error != 0)
    {
      Serial.print(F("I2C Error: "));
      Serial.println(error);
    }

    // Пауза для записи байта или страницы, 5 мс по datasheet, cтавим 7 мс с запасом
    delay(5);
  }
}

// Выбор какая функция смены серийного номера заработает
void change_crum_select()
{
  switch (global_change_crum)
  {
  case 0:
    break;
  case 1:
    // SAMSUNG XEROX где 1 серийник
    // Меняем байты серийника и показываем его
    change_crum_universal(56, 63); // меняем с 56 байта по 63 байт
    print_sn_on_lcd_universal(53, 63);
    break;
  case 2:
    // SAMSUNG XEROX где 2 серийника
    change_crum_two_xerox();
    break;
  case 3:
    // RICOH SP_3600_3610_4510
    // Меняем байты серийника и показываем его
    change_crum_universal(22, 23); // меняем с 22 байта по 23 байт
    print_sn_on_lcd_universal(22, 23);
    break;
  case 4:
    // KATUSHA TK 240
    // Меняем байты серийника и показываем его
    change_crum_universal(60, 73); // меняем с 60 байта по 73 байт
    print_sn_on_lcd_universal(60, 73);
    break;
  case 5:
    // KATUSHA DR 240
    // Меняем байты серийника и показываем его
    change_crum_universal(52, 65); // меняем с 52 байта по 65 байт
    print_sn_on_lcd_universal(52, 65);
    break;
  default:
    break;
  }
}

/****************************** СКОРОСТНАЯ ПРОШИВКА ЧИПОВ МОДИФИКАЦИЯ 2025-08-07 */
void firmware()
{
  // Запись чипа страницами
  WriteBytesPages(global_address_eeprom, global_size_dump, global_name_dump);
  // Сообщение о прошивки чипа
  Serial.println(F("FIRMWARE GOOD"));
  // Проверка дампа по байтно в чипе
  verification_dump();
  // Смена серийного номера
  change_crum_select();
}

void firmware_chip_with_timer(int timer)
{
  // Таймер обратного отсчета
  countdown_timer(timer);

  // Подаем питание на чип
  power_on_for_chip();

  // сканируем шину i2c на наличие чипа
  if (search_address_chip())
  {
    // Скоростная прошивка чипа
    firmware();
  }

  // Выключаем питание
  power_off_for_chip();

  // Возврат в меню
}

void setup()
{
  // pinMode(10,OUTPUT);
  Serial.begin(9600); // инициализируем последовательное соединение для работы с ПК
  while (!Serial)
  {
    ;
  } // Ждем когда подключится ардуино к пк по usb

  Wire.begin();                       // Вместо I2c.begin()
  Wire.setClock(100000);              // Вместо I2c.timeOut(5) и pullup
  pinMode(POWER_PIN, OUTPUT);         // Пин А2 для питания чипа устанавливаем в положение OUTPUT
  randomSeed(analogRead(RANDOM_PIN)); // Пин А3 для работы генератора случайных чисел

  global_all_chip_in_database = (sizeof(datebase) / sizeof(Struct_DB)) - 1; // Подсчитываем сколько чипов в Базе
  delay(1500);
}

void loop()
{
}
