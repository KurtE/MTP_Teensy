#include <LittleFS.h>
#include <MTP_Teensy.h>

#ifndef ARDUINO_TEENSY41
#error "This only runs on a Teesny 4.1 with QSPI chips installed on bottom of board"
#endif

MTPStorage storage;     // TODO: storage inside MTP instance, not separate class
MTPD    MTP(&storage);  // TODO: MTP instance created by default

LittleFS_QSPI qspi;
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
  
  // try to add QSPI Flash
  if (qspi.begin()) {
    storage.addFilesystem(qspi, qspi.displayName());
  } else {
    Serial.println("No QSPI Flash found");
  }
  Serial.println("\nSetup done");
}


void loop()
{
  if ( Serial.available() ) {
    uint8_t command = Serial.read();

    uint32_t fsCount;
    switch (command) {
      case '1':
        // first dump list of storages:
        fsCount = storage.getFSCount();
        Serial.printf("\nDump Storage list(%u)\n", fsCount);
        for (uint32_t ii = 0; ii < fsCount; ii++) {
          Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii, MTP.Store2Storage(ii),
                           storage.getStoreName(ii), (uint32_t)storage.getStoreFS(ii));
        }
        Serial.println("\nDump Index List");
        storage.dumpIndexList();
        break;
      case'r':
        Serial.println("Reset");
        MTP.send_DeviceResetEvent();
        break;

      case '\r':
      case '\n':
      case 'h': menu(); break;
    }
    while (Serial.read() != -1) ; // remove rest of characters.
  } else {
    MTP.loop();
  }
}

void menu()
{
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List Drives (Step 1)");
  Serial.println("\tr - Reset MTP");
  Serial.println();
}
