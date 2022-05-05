/*
  This example shows the use of a available LittleFS wrapper function for multiple
  SPI chips which can include FRAM, NOR FLASH and NAND FLASH.

  This example code is in the public domain.
*/
#include "SD.h"
#include <LittleFS.h>
#include <MTP_Teensy.h>


#define SD_ChipSelect BUILTIN_SDCARD  // Teensy 3.6, 4.1 and some Micromod carrier boards
//#define SD_ChipSelect  10;

SDClass sd;

/*
   There are two wrapper classes available for use with LittleFS:
   1. lfs_spi: for SPI memory chips
   2. lfs_qspi: for QSPI memory chips - NOR or NAND Flash

   Using these wrappers makes it a bit simpler as all you need to remember
   are the chip select pins for the memory chips on SPI not whether its a NAND or NOR or FRAM

*/
// So for this example lets assume we memory on pins 3, 4, 5 and 6.
// This creates an LittleFS_SPIxxxx instance for each chip
LittleFS_SPI lfs_spi_list[] = {{3}, {4}, {5}, {6}};

void setup() {
  // start up MTPD early which will if asked tell the MTP
  // host that we are busy, until we have finished setting
  // up...
  Serial.begin(2000000);
  MTP.begin();

  // Open serial communications and wait for port to open:
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }

  if (CrashReport) {
    Serial.print(CrashReport);
  }

  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  Serial.printf("%u Initializing MTP Storage list ...", millis());


  // Now lets try our list of LittleFS SPI?
  for (uint8_t i = 0; i < (sizeof(lfs_spi_list) / sizeof(lfs_spi_list[0])); i++) {
    pinMode(lfs_spi_list[i].csPin_, OUTPUT);
    digitalWrite(lfs_spi_list[i].csPin_, HIGH);
  }

  for (uint8_t i = 0; i < (sizeof(lfs_spi_list) / sizeof(lfs_spi_list[0])); i++) {
    if (lfs_spi_list[i].begin()) {
      MTP.addFilesystem(*lfs_spi_list[i].fs(), lfs_spi_list[i].displayName());
    } else {
      Serial.printf("Storage not added for pin %d\n", lfs_spi_list[i].csPin_);
    }
  }

  if (sd.begin(SD_ChipSelect)) {
    MTP.addFilesystem(sd, "SDCard");
  } else {
    Serial.println("No SD card available");
  }

  Serial.printf("%u Storage list initialized.\n", millis());
}


void loop() {
  MTP.loop();
}
