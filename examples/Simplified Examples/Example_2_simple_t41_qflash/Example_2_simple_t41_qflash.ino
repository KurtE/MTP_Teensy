#include <LittleFS.h>
#include <MTP_Teensy.h>

#ifndef ARDUINO_TEENSY41
#error "This only runs on a Teesny 4.1 with a QSPI Flash chip installed on bottom of board"
#endif


LittleFS_QSPI qspi;
void setup()
{
  // mandatory to begin the MTP session.
  //MTP.begin();
  //Serial.begin(9600);
  // try to add QSPI Flash
  bool qspi_started;
  if ((qspi_started = qspi.begin())) {
    MTP.addFilesystem(qspi, qspi.displayName());
  }

  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  
  if (!qspi_started) {
    Serial.println("No QSPI Flash found");
  }

  if (CrashReport) Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(1000);

  
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
        fsCount = MTP.storage()->getFSCount();
        Serial.printf("\nDump Storage list(%u)\n", fsCount);
        for (uint32_t ii = 0; ii < fsCount; ii++) {
          Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii, MTP.Store2Storage(ii),
                           MTP.getFilesystemNameByIndex(ii), (uint32_t)MTP.getFilesystemByIndex(ii));
        }
        Serial.println("\nDump Index List");
        MTP.storage()->dumpIndexList();
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
