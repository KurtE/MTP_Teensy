/* LittleFS for Teensy
 * Copyright (c) 2020, Paul Stoffregen, paul@pjrc.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once
#include <LittleFS.h>

// Quick and dirty combine classes
class FailCallsFS : public LittleFS {
public:
  FailCallsFS() {}
  bool begin() { return false; }
  virtual File open(const char *filename, uint8_t mode = FILE_READ) {
    return File();
  }
  virtual bool exists(const char *filepath) { return false; }
  virtual bool mkdir(const char *filepath) { return false; }
  virtual bool rename(const char *oldfilepath, const char *newfilepath) {
    return false;
  }
  virtual bool remove(const char *filepath) { return false; }
  virtual bool rmdir(const char *filepath) { return false; }
  virtual uint64_t usedSize() { return 0; }
  virtual uint64_t totalSize() { return 0; }
};

// that says handle the three supported SPI memory type chips
// Based on FS, as can not directly call LittleFS specific functions
class LittleFS_SPI : public FS {
public:
  LittleFS_SPI() {}
  bool begin(uint8_t cspin, SPIClass &spiport = SPI) {
    Serial.println("LittleFS_SPI Try Flash");
    if (_lfsflash.begin(cspin, spiport)) {
      _plfs = &_lfsflash;
      _chipType = CHIP_FLASH;
      Serial.println("** OK **");
      snprintf(_displayName, sizeof(_displayName),
               (const char *)F("FLASH_%02u"), cspin);
      return true;
    }
    if (_lfsnand.begin(cspin, spiport)) {
      Serial.println("LittleFS_SPI Try NAND");
      _plfs = &_lfsnand;
      _chipType = CHIP_NAND;
      snprintf(_displayName, sizeof(_displayName), (const char *)F("NAND_%02u"),
               cspin);
      Serial.println("** OK **");
      return true;
    }

    if (_lfsfram.begin(cspin, spiport)) {
      Serial.println("LittleFS_SPI Try FRAM");
      _plfs = &_lfsfram;
      _chipType = CHIP_FRAM;
      snprintf(_displayName, sizeof(_displayName), (const char *)F("FRAM_%02u"),
               cspin);
      Serial.println("** OK **");
      return true;
    }
    _plfs = &_failsfs;
    _chipType = CHIP_NONE;
    Serial.println("** FAILED **");
    return false;
  }

  // return functions to get to underlying LittleFS functions
  // first generic one
  LittleFS *plfs() { return _plfs; }
  LittleFS_SPIFlash *SPIFlash() {
    return (_plfs == &_lfsflash) ? &_lfsflash : nullptr;
  }
  LittleFS_SPINAND *SPINAND() {
    return (_plfs == &_lfsnand) ? &_lfsnand : nullptr;
  }
  LittleFS_SPIFram *SPIFram() {
    return (_plfs == &_lfsfram) ? &_lfsfram : nullptr;
  }

  enum { CHIP_NONE = 0, CHIP_FLASH = 1, CHIP_NAND = 2, CHIP_FRAM = 3 };
  uint8_t chipType() { return _chipType; }
  const char *displayName() { return _displayName; }

  // FS class functions
  virtual File open(const char *filename, uint8_t mode = FILE_READ) {
    return _plfs->open(filename, mode);
  }
  virtual bool exists(const char *filepath) { return _plfs->exists(filepath); }
  virtual bool mkdir(const char *filepath) { return _plfs->mkdir(filepath); }
  virtual bool rename(const char *oldfilepath, const char *newfilepath) {
    return _plfs->rename(oldfilepath, newfilepath);
  }
  virtual bool remove(const char *filepath) { return _plfs->remove(filepath); }
  virtual bool rmdir(const char *filepath) { return _plfs->rmdir(filepath); }
  virtual uint64_t usedSize() { return _plfs->usedSize(); }
  virtual uint64_t totalSize() { return _plfs->totalSize(); }

private:
  LittleFS_SPIFlash _lfsflash;
  LittleFS_SPINAND _lfsnand;
  LittleFS_SPIFram _lfsfram;
  FailCallsFS _failsfs;
  LittleFS *_plfs = &_failsfs;
  uint8_t _chipType = CHIP_NONE;
  char _displayName[10];
};

#if defined(__IMXRT1062__)
// Based on FS, as can not directly call LittleFS specific functions
// that says handle either type of bottom Flash...
// on T4.1
class LittleFS_QSPI : public FS {
public:
  LittleFS_QSPI() {}
  bool begin() {
    Serial.println("LittleFS_QSPI Try Flash");
    if (_lfsqflash.begin()) {
      _plfs = &_lfsqflash;
      _chipType = CHIP_FLASH;
      Serial.println("** OK **");
      return true;
    }
    if (_lfsqnand.begin()) {
      Serial.println("LittleFS_QSPI Try NAND");
      _plfs = &_lfsqnand;
      _chipType = CHIP_NAND;
      Serial.println("** OK **");
      return true;
    }
    _plfs = &_failsfs;
    _chipType = CHIP_NONE;
    Serial.println("** FAILED **");
    return false;
  }

  // return functions to get to underlying LittleFS functions
  // first generic one
  LittleFS *plfs() { return _plfs; }
  LittleFS_QSPIFlash *QSPIFlash() {
    return (_plfs == &_lfsqflash) ? &_lfsqflash : nullptr;
  }
  LittleFS_QPINAND *QPINAND() {
    return (_plfs == &_lfsqnand) ? &_lfsqnand : nullptr;
  }

  enum { CHIP_NONE = 0, CHIP_FLASH = 1, CHIP_NAND = 2 };
  uint8_t chipType() { return _chipType; }
  const char *displayName() {
    switch (_chipType) {
    default:
      return "";
    case CHIP_FLASH:
      return (const char *)F("QFLASH");
    case CHIP_NAND:
      return (const char *)F("QNAND");
    }
  }

  // FS class functions
  virtual File open(const char *filename, uint8_t mode = FILE_READ) {
    return _plfs->open(filename, mode);
  }
  virtual bool exists(const char *filepath) { return _plfs->exists(filepath); }
  virtual bool mkdir(const char *filepath) { return _plfs->mkdir(filepath); }
  virtual bool rename(const char *oldfilepath, const char *newfilepath) {
    return _plfs->rename(oldfilepath, newfilepath);
  }
  virtual bool remove(const char *filepath) { return _plfs->remove(filepath); }
  virtual bool rmdir(const char *filepath) { return _plfs->rmdir(filepath); }
  virtual uint64_t usedSize() { return _plfs->usedSize(); }
  virtual uint64_t totalSize() { return _plfs->totalSize(); }

private:
  LittleFS_QSPIFlash _lfsqflash;
  LittleFS_QPINAND _lfsqnand;
  FailCallsFS _failsfs;
  LittleFS *_plfs = &_failsfs;
  uint8_t _chipType = CHIP_NONE;
};
#endif
