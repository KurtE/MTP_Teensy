#ifndef SDMTPCLASS_H
#define SDMTPCLASS_H
#include <SD.h>

// Setup a callback class for Littlefs storages..
// Plus helper functions for handling SD disk insertions.
class SDMTPClass : public SDClass {
public:
  // constructor
  SDMTPClass() {}
  bool begin(uint8_t csPin = 10, uint32_t maxSpeed = SD_SCK_MHZ(16), uint8_t opt = 0) {
    csPin_ = csPin;
    maxSpeed_ = maxSpeed;
    opt_ = opt;
    return SDClass::begin(csPin);
  }

  uint64_t usedSize();
  bool format(int type = 0, char progressChar = 0, Print& pr = Serial);
private:
  // helper functions
  uint8_t csPin_ = 255;
  uint8_t opt_ = 0;
  uint32_t maxSpeed_ = SD_SCK_MHZ(16);
  bool usedSize_called_ = false;
};

#endif
