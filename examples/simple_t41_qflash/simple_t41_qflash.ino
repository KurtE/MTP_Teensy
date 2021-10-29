#include <SD.h>
#include <LittleFS.h>
#include <MTP_Teensy.h>

#define DBGSerial Serial

MTPStorage storage;     // TODO: storage inside MTP instance, not separate class
MTPD    MTP(&storage);  // TODO: MTP instance created by default

LittleFS_QSPIFlash qspiflash;
LittleFS_QPINAND   qspinand;

void setup()
{
  DBGSerial.begin(9600);
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }

  DBGSerial.print(CrashReport);
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(1000);

  // mandatory to begin the MTP session.
  MTP.begin();
  
  // try to add QSPI Flash
  if (qspiflash.begin()) {
    storage.addFilesystem(qspiflash, "QSPI Flash");
    DBGSerial.println("Added QSPI Flash");
  } else if (qspinand.begin()) {
    storage.addFilesystem(qspinand, "QSPI NAND");
    DBGSerial.println("Added QSPI Nand");
  } else {
    DBGSerial.println("No QSPI Flash found");
  }
  DBGSerial.println("\nSetup done");
}


void loop()
{
  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();

    uint32_t fsCount;
    switch (command) {
      case '1':
        // first dump list of storages:
        fsCount = storage.getFSCount();
        DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
        for (uint32_t ii = 0; ii < fsCount; ii++) {
          DBGSerial.printf("store:%u storage:%x name:%s fs:%x\n", ii, MTP.Store2Storage(ii),
                           storage.getStoreName(ii), (uint32_t)storage.getStoreFS(ii));
        }
        DBGSerial.println("\nDump Index List");
        storage.dumpIndexList();
        break;
      case'r':
        DBGSerial.println("Reset");
        MTP.send_DeviceResetEvent();
        break;

      case '\r':
      case '\n':
      case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
  } else {
    MTP.loop();
    //#if USE_MSC > 0
    //usbmsc.checkUSBStatus(false);
    //#endif
  }
}

void menu()
{
  DBGSerial.println();
  DBGSerial.println("Menu Options:");
  DBGSerial.println("\t1 - List Drives (Step 1)");
  DBGSerial.println("\tr - Reset MTP");
  DBGSerial.println();
}
