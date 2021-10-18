#include <MTP_Teensy.h>
#include <FS.h>
#include <LittleFS.h>
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;


class FormatFS : public FS
  {
  public:
    FormatFS(uint32_t to) : format_to_(to) {}
    File open(const char *filename, uint8_t mode = FILE_READ) { return f;}
    bool exists(const char *filepath) {return false;}
    bool mkdir(const char *filepath) { return false;}
    bool rename(const char *oldfilepath, const char *newfilepath) { return false;}
    bool remove(const char *filepath) { return false; }
    bool rmdir(const char *filepath) { return false; }
    uint64_t usedSize() { return 0; }
    uint64_t totalSize() { return 1000000l; }
    bool format(int type = 0, char progressChar = 0, Print& pr = Serial) {
      delay(format_to_);
      return true;
    }
  private:
    uint32_t format_to_;
    File f;
  };

FormatFS ffs10(1000);
FormatFS ffs20(2000);
FormatFS ffs25(2500);
FormatFS ffs30(3000);
FormatFS ffs31(3100);
FormatFS ffs35(3500);
FormatFS ffs50(5000);

#define DBGSerial Serial

MTPStorage storage;     // TODO: storage inside MTP instance, not separate class
MTPD    MTP(&storage);  // TODO: MTP instance created by default
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

  if (lfsram.begin(LFSRAM_SIZE)) {
    storage.addFilesystem(lfsram, "RAM");
  }

  storage.addFilesystem(ffs10, "FFS1000");
  storage.addFilesystem(ffs20, "FFS2000");
  storage.addFilesystem(ffs25, "FFS2500");
  storage.addFilesystem(ffs30, "FFS3000");
  storage.addFilesystem(ffs31, "FFS3100");
  storage.addFilesystem(ffs35, "FFS3500");
  storage.addFilesystem(ffs50, "FFS5000");
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
