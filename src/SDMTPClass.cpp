#include <SDMTPClass.h>

uint64_t SDMTPClass::usedSize() {
  // bypass if have called before or are dedicated SPI...
  Serial.printf("&&& SDMTPClass::usedSize %u %u %lu %u\n", usedSize_called_, csPin_, maxSpeed_, opt_);

  if (usedSize_called_ || (csPin_ >= BUILTIN_SDCARD ) || (opt_ == DEDICATED_SPI)) {
    return SDClass::usedSize();
  }

  pinMode(csPin_, OUTPUT);
  digitalWrite(csPin_, LOW);
  Serial.println(">>> Set dedicated SPI");
  sdfs.begin(SdSpiConfig(csPin_, DEDICATED_SPI, maxSpeed_));
  uint64_t used_size = SDClass::usedSize();
  usedSize_called_ = true;
  digitalWrite(csPin_, HIGH);
  Serial.println(">>> Set back to shared SPI");
  sdfs.begin(SdSpiConfig(csPin_, SHARED_SPI, maxSpeed_));
  return used_size;
}

bool SDMTPClass::format(int type, char progressChar, Print& pr) {
  // if built-in sdcard or not known pass it directly to SDClass
  if (csPin_ >= BUILTIN_SDCARD ) return SDClass::format(type, progressChar, pr);

  SdCard *card = sdfs.card();
  if (!card) return false; // no SD card
  uint32_t sectors = card->sectorCount();
  if (sectors <= 12288) return false; // card too small
  uint8_t *buf = (uint8_t *)malloc(512);
  if (!buf) return false; // unable to allocate memory
  bool ret;
  //Serial.printf("CS PIN IN USE: %d\n", cspin);
  if (opt_ != DEDICATED_SPI) {
    pinMode(csPin_, OUTPUT);
    digitalWrite(csPin_, LOW);
    sdfs.begin(SdSpiConfig(csPin_, DEDICATED_SPI, maxSpeed_));
  }

  if (sectors > 67108864) {
#ifdef __arm__
    ExFatFormatter exFatFormatter;
    ret = exFatFormatter.format(card, buf, &pr);
#else
    ret = false;
#endif
  } else {
    FatFormatter fatFormatter;
    ret = fatFormatter.format(card, buf, &pr);
  }
  free(buf);
  if (ret || (opt_ != DEDICATED_SPI)) {
    digitalWrite(csPin_, HIGH);
    sdfs.begin(SdSpiConfig(csPin_, SHARED_SPI, maxSpeed_));
  }
  return ret;
}
