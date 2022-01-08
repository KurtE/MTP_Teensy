/*
*/
#include <MTP_Teensy.h>

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

FS *myfs;

#define USE_MEMORY_FS

uint32_t MEMFS_SIZE = 65536; // probably more than enough...
#ifdef USE_MEMORY_FS
#include <MemFile.h>
MemFS memfs;
#else
#include <LittleFS.h>
LittleFS_RAM lfsram;
#endif
#include "TeensyFS.h"
TeensyFS teensyfs;


#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif


void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
//  Serial4.begin(115200);  // Use to echo stuff out for debug core.
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);

  // startup mtp.. SO to not timeout...
  MTP.begin();
// lets initialize a RAM drive.
#if defined ARDUINO_TEENSY41
  if (external_psram_size)
    MEMFS_SIZE = 4 * 1024 * 1024;
#endif
#ifdef USE_MEMORY_FS
  if (memfs.begin(MEMFS_SIZE)) {
    Serial.printf("Memory Drive of size: %u initialized\n", MEMFS_SIZE);
    uint32_t istore = MTP.addFilesystem(memfs, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  myfs = &memfs;  // so we don't start of with NULL pointer
#else
  if (lfsram.begin(MEMFS_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", MEMFS_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  myfs = &lfsram;  // so we don't start of with NULL pointer
#endif

  MTP.addFilesystem(teensyfs, "Hardware");

  Serial.println("MTP initialized.");
  menu();
}

void loop() {
  MTP.loop();

  if (Serial.available()) {
    uint8_t command = Serial.read();
    int ch = Serial.read();
    uint32_t drive_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ')
      ch = Serial.read();

    switch (command) {
    case 'l':
      listFiles();
      break;
    case 'e':
      eraseFiles();
      break;
    case '1': {
      // first dump list of storages:
      uint32_t fsCount = MTP.storage()->getFSCount();
      Serial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                      MTP.Store2Storage(ii), MTP.storage()->getStoreName(ii),
                      (uint32_t)MTP.storage()->getStoreFS(ii));
      }
      Serial.println("\nDump Index List");
      MTP.storage()->dumpIndexList();
    } break;
    case '2':
      Serial.printf("Drive # %d Selected\n", drive_index);
      myfs = MTP.storage()->getStoreFS(drive_index);
      break;
    case 'r':
      Serial.println("Send Device Reset Event");
      MTP.send_DeviceResetEvent();
      break;
    case '\r':
    case '\n':
    case 'h':
      menu();
      break;
    default:
      menu();
      break;
    }
    while (Serial.read() != -1)
      ; // remove rest of characters.
  }
}

void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List Drives (Step 1)");
  Serial.println("\t2 - Select Drive");
  Serial.println("\tl - List files");
  Serial.println("\te - Erase files");
  Serial.println("\tr - reset MTP");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles() {
  Serial.print("\n Space Used = ");
  Serial.println(myfs->usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(myfs->totalSize());

  File root = myfs->open("/");
  printDirectory(root, 0);
  root.close();
}

void eraseFiles() {
  Serial.println("\n*** Erase/Format started ***");
  myfs->format(0, '.', Serial);
  Serial.println("Completed, sending device reset event");
  MTP.send_DeviceResetEvent();
}

void printDirectory(File dir, int numSpaces) {
  DateTimeFields dtf;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // Serial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    Serial.print(entry.name());
    printSpaces(36 - numSpaces - strlen(entry.name()));

    if (entry.getCreateTime(dtf)) {
      Serial.printf(" C: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
    }

    if (entry.getModifyTime(dtf)) {
      Serial.printf(" M: %02u/%02u/%04u %02u:%02u", dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );
    }
    if (entry.isDirectory()) {
      Serial.println("  /");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      Serial.print("  ");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    Serial.print(" ");
  }
}

uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ')
    ch = Serial.read();
  if ((ch < '0') || (ch > '9'))
    return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = Serial.read();
  }
  return return_value;
}
