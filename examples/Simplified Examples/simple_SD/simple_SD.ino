#include <SD.h>
#include <MTP_Teensy.h>

#define CS_SD BUILTIN_SDCARD  // Works on T_3.6 and T_4.1
//#define CS_SD 10  // Works on SPI with this CS pin

MTPStorage storage;     // TODO: storage inside MTP instance, not separate class
MTPD    MTP(&storage);  // TODO: MTP instance created by default

void setup()
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }

  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(1000);

  // mandatory to begin the MTP session.
  MTP.begin();

  // Add SD Card
  if (SD.begin(CS_SD)) {
    storage.addFilesystem(SD, "SD Card");
    Serial.println("Added SD card using built in SDIO, or given SPI CS");
  } else {
    Serial.println("No SD Card");
  }
  Serial.println("\nSetup done");
}


void loop() {
  MTP.loop();  //This is mandatory to be placed in the loop code.
}
