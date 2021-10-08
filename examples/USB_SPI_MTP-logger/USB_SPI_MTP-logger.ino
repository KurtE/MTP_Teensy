/*
  LittleFS  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include <MTP_Teensy.h>
// Classes used for MSC drives
#include <USB_MSC_MTP.h>
#include <mscFS.h>

#include "LittleFS.h"
//=============================================================================
// LittleFS classes
//=============================================================================
// Setup a callback class for Littlefs storages..
#include <LFS_MTP_Callback.h> //callback for LittleFS format
LittleFSMTPCB lfsmtpcb;       // sets up the pointer for LittleFs formatting

//========================================================================
// This puts a the index file in memory as opposed to an SD Card in memory
//========================================================================
#define LFSRAM_SIZE 65536 // probably more than enough...
LittleFS_RAM lfsram;
FS *myfs = &lfsram; // current default FS...

File dataFile; // Specifes that dataFile is of File type
int record_count = 0;
bool write_data = false;
uint32_t diskSize;
uint8_t current_store = 0;

// Add in MTPD objects
MTPStorage storage;
MTPD mtpd(&storage);

// This should be called after setting MTPD objects to setup MSC Drives
USB_MSC_MTP usbmsc(mtpd, storage);

// Setup up arrays for using 2 SPI NOR Flash Chips
const char *lfs_spi_str[] = {"sflash5",
                             "sflash6"}; // edit to reflect your configuration
const int lfs_cs[] = {5, 6};             // edit to reflect your configuration
const int nfs_spi = sizeof(lfs_spi_str) / sizeof(const char *);
LittleFS_SPIFlash spifs[nfs_spi];

// Since we have only 1 NAND SPI flash, setup will be in storage Configure
LittleFS_SPINAND lfsNAND;
#define nandCS 4

#define DBGSerial Serial

// convient for setting up multiple storage configurations
void storage_configure() {
#if (1)
  // lets initialize a RAM drive and set the index file to RAM Drive.
  if (lfsram.begin(LFSRAM_SIZE)) {
    DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    lfsmtpcb.set_formatLevel(
        true); // sets formating to lowLevelFormat, false indicates quickFormat
    uint32_t istore = storage.addFilesystem(lfsram, "RAM", &lfsmtpcb,
                                            (uint32_t)(LittleFS *)&lfsram);
    if (istore != 0xFFFFFFFFUL)
      storage.setIndexStore(istore);
    DBGSerial.printf("Set Storage Index drive to %u\n", istore);
  }
#else
  storage.setIndexStore(0); // Default configuration
  DBGSerial.printf("Storage Index drive to 0\n");

#endif
  // configure MTP for SPI NOR Flash
  for (int ii = 0; ii < nfs_spi; ii++) {
    pinMode(lfs_cs[ii], OUTPUT);
    digitalWriteFast(lfs_cs[ii], HIGH);
    if (!spifs[ii].begin(lfs_cs[ii], SPI)) {
      Serial.printf("SPIFlash Storage %d %d %s failed or missing", ii,
                    lfs_cs[ii], lfs_spi_str[ii]);
      Serial.println();
    } else {
      storage.addFilesystem(spifs[ii], lfs_spi_str[ii], &lfsmtpcb,
                            (uint32_t)(LittleFS *)&spifs[ii]);
      uint64_t totalSize = spifs[ii].totalSize();
      uint64_t usedSize = spifs[ii].usedSize();
      Serial.printf("SPIFlash Storage %d %d %s ", ii, lfs_cs[ii],
                    lfs_spi_str[ii]);
      Serial.print(totalSize);
      Serial.print(" ");
      Serial.println(usedSize);
    }
  }

  delay(100);
  // Configure NAND SPI Flash chip
  pinMode(nandCS, OUTPUT);
  digitalWriteFast(nandCS, HIGH);
  if (!lfsNAND.begin(nandCS, SPI)) {
    Serial.printf("NAND SPIFlash Storage %d %d %s failed or missing", 0, nandCS,
                  "NAND7");
    Serial.println();
  } else {
    storage.addFilesystem(lfsNAND, "NAND3", &lfsmtpcb,
                          (uint32_t)(LittleFS *)&lfsNAND);
    uint64_t totalSize = lfsNAND.totalSize();
    uint64_t usedSize = lfsNAND.usedSize();
    Serial.printf("NAND SPIFlash Storage %d %d %s ", 0, nandCS, "NAND3");
    Serial.print(totalSize);
    Serial.print(" ");
    Serial.println(usedSize);
  }

  DBGSerial.println("\nInitializing USB MSC drives...");
  usbmsc.checkUSBStatus(true);
}

void setup() {
  // let mscusb stuff startup as soon as possible
  usbmsc.begin();

  // Open serial communications and wait for port to open:
  DBGSerial.begin(115200);
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }
  DBGSerial.print(CrashReport);
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  DBGSerial.print("Initializing MSC Drives ...");

  mtpd.begin(); // New method removing need to initize MTPD

  DBGSerial.println("Initializing LittleFS and/or SD Drives");
  storage_configure();

  DBGSerial.println("MSC and MTP initialized.");

  menu();
}

void loop() {
  if (DBGSerial.available()) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    uint8_t storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ')
      ch = DBGSerial.read();

    switch (command) {
    case '1': {
      // first dump list of storages:
      uint32_t fsCount = storage.getFSCount();
      DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        DBGSerial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                         mtpd.Store2Storage(ii), storage.getStoreName(ii),
                         (uint32_t)storage.getStoreFS(ii));
      }
      // DBGSerial.println("\nDump Index List");
      // storage.dumpIndexList();
    } break;
    case '2': {
      if (storage_index < storage.getFSCount()) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index,
                         storage.getStoreName(storage_index));
        myfs = storage.getStoreFS(storage_index);
        current_store = storage_index;
      } else {
        DBGSerial.printf("Storage Index %u out of range\n", storage_index);
      }
    } break;

    case 'l':
      listFiles();
      break;
    case 'e':
      eraseFiles();
      break;
    case 's': {
      DBGSerial.println("\nLogging Data!!!");
      write_data = true; // sets flag to continue to write data until new
                         // command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append
      // data to
      // to the file created.
      dataFile = myfs->open("datalog.txt", FILE_WRITE);
      logData();
    } break;
    case 'x':
      stopLogging();
      break;
    case 'r':
      DBGSerial.println("Reset");
      mtpd.send_DeviceResetEvent();
      break;
    case 'd':
      dumpLog();
      break;
    case '\r':
    case '\n':
    case 'h':
      menu();
      break;
    }
    while (DBGSerial.read() != -1)
      ; // remove rest of characters.
  } else {
    mtpd.loop();
    usbmsc.checkUSBStatus(false);
  }

  if (write_data)
    logData();
}

void logData() {
  // make a string for assembling the data to log:
  String dataString = "";

  // read three sensors and append to the string:
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    dataString += String(sensor);
    if (analogPin < 2) {
      dataString += ",";
    }
  }

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    // print to the serial port too:
    DBGSerial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    DBGSerial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging() {
  DBGSerial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  DBGSerial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}

void dumpLog() {
  DBGSerial.println("\nDumping Log!!!");
  // open the file.
  dataFile = myfs->open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      DBGSerial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    DBGSerial.println("error opening datalog.txt");
  }
}

void menu() {
  DBGSerial.println();
  DBGSerial.println("Menu Options:");
  DBGSerial.println("\t1 - List Drives (Step 1)");
  DBGSerial.println("\t2 - Select Drive for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\te - Erase files on disk");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append "
                    "records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tr - Reset MTP");
  DBGSerial.println("\th - Menu");
  DBGSerial.println();
}

void listFiles() {
  DBGSerial.print("\n Space Used = ");
  DBGSerial.println(myfs->usedSize());
  DBGSerial.print("Filesystem Size = ");
  DBGSerial.println(myfs->totalSize());

  printDirectory(myfs);
}

extern PFsLib pfsLIB;
void eraseFiles() {

#if 1
  // Lets try asking storage for enough stuff to call it's format code
  bool send_device_reset = false;
  MTPStorageInterfaceCB *callback = storage.getCallback(current_store);
  uint32_t user_token = storage.getUserToken(current_store);
  if (callback) {
    send_device_reset =
        (callback->formatStore(&storage, current_store, user_token, 0, true) ==
         MTPStorageInterfaceCB::FORMAT_SUCCESSFUL);
  }
  if (send_device_reset) {
    DBGSerial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();
  } else {
    DBGSerial.println("\n failed !");
  }

#else
  PFsVolume partVol;
  if (!partVol.begin(myfs->sdfs.card(), true, 1)) {
    DBGSerial.println("Failed to initialize partition");
    return;
  }
  if (pfsLIB.formatter(partVol)) {
    DBGSerial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();
  }
#endif
}

void printDirectory(FS *pfs) {
  DBGSerial.println("Directory\n---------");
  printDirectory(pfs->open("/"), 0);
  DBGSerial.println();
}

void printDirectory(File dir, int numSpaces) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // DBGSerial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    DBGSerial.print(entry.name());
    if (entry.isDirectory()) {
      DBGSerial.println("/");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      printSpaces(36 - numSpaces - strlen(entry.name()));
      DBGSerial.print("  ");
      DBGSerial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

void printSpaces(int num) {
  for (int i = 0; i < num; i++) {
    DBGSerial.print(" ");
  }
}

uint32_t CommandLineReadNextNumber(int &ch, uint32_t default_num) {
  while (ch == ' ')
    ch = DBGSerial.read();
  if ((ch < '0') || (ch > '9'))
    return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = DBGSerial.read();
  }
  return return_value;
}