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

//----------------------------------------------------------------------------
// Experiment with LittleFS_SPI wrapper
//----------------------------------------------------------------------------
#ifdef ARDUINO_TEENSY41
class lfs_qspi {
public:
  lfs_qspi(){}
  bool begin();
  inline LittleFS * fs() { return plfs;}
  inline const char * displayName() {return display_name;}
  // You have full access to internals.
  uint8_t csPin;
  LittleFS_QSPIFlash flash;
  LittleFS_QPINAND nand;
  LittleFS *plfs = nullptr;
  char display_name[10];
};
bool lfs_qspi::begin() {
  //Serial.printf("Try QSPI");
  if (flash.begin()) {
    //Serial.println(" *** Flash ***");
    strcpy(display_name, (const char *)F("QFlash"));
    plfs = &flash;
    return true;
  } else if (nand.begin()) {
    //Serial.println(" *** Nand ***");
    strcpy(display_name, (const char *)F("QNAND"));
    plfs = &nand;
    return true;
  }
  //Serial.println(" ### Failed ###");
  return false;
}
  lfs_qspi lfsq;
#endif

//----------------------------------------------------------------------------
// Experiment with LittleFS_SPI wrapper
//----------------------------------------------------------------------------
class lfs_spi {
public:
  lfs_spi(uint8_t pin) : csPin(pin) {}
  bool begin();
  inline LittleFS * fs() { return plfs;}
  inline const char * displayName() {return display_name;}
  // You have full access to internals.
  uint8_t csPin;
  LittleFS_SPIFlash flash;
  LittleFS_SPIFram fram;
  LittleFS_SPINAND nand;
  LittleFS *plfs = nullptr;
  char display_name[10];
};
bool lfs_spi::begin() {
  //Serial.printf("Try SPI Pin %u", csPin);
  if (flash.begin(csPin)) {
    //Serial.println(" *** Flash ***");
    sprintf(display_name, (const char *)F("Flash_%u"), csPin);
    plfs = &flash;
    return true;
  } else if (fram.begin(csPin)) {
    //Serial.println(" *** Fram ***");
    sprintf(display_name, (const char *)F("Fram_%u"), csPin);
    plfs = &fram;
    return true;
  } else if (nand.begin(csPin)) {
    //Serial.println(" *** Nand ***");
    sprintf(display_name, (const char *)F("NAND_%u"), csPin);
    plfs = &nand;
    return true;
  }
  //Serial.println(" ### Failed ###");
  return false;
}
