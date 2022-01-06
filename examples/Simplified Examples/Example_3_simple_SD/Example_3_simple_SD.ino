#include <SD.h>
#include <MTP_Teensy.h>

#define CS_SD BUILTIN_SDCARD  // Works on T_3.6 and T_4.1
//#define CS_SD 10  // Works on SPI with this CS pin
void setup()
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }

  // mandatory to begin the MTP session.
  MTP.begin();

  // Add SD Card
  if (SD.begin(CS_SD)) {
    MTP.addFilesystem(SD, "SD Card");
    Serial.println("Added SD card using built in SDIO, or given SPI CS");
  } else {
    Serial.println("No SD Card");
  }
  Serial.println("\nSetup done");
}


void loop() {
  MTP.loop();  //This is mandatory to be placed in the loop code.
}
