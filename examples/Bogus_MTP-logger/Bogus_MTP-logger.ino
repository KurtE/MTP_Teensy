/*
  LittleFS  datalogger

  Analyzes incoming LargeIndexedTestfile.txt file, detects lost USB packets

    program to create LargeIndexedTestfile.txt, msg 421
    https://forum.pjrc.com/threads/68139?p=292501&viewfull=1#post292501

    info, msg 461
    https://forum.pjrc.com/threads/68139?p=293064&viewfull=1#post293064

    reproducible test run, msg 563
    https://forum.pjrc.com/threads/68139?p=293786&viewfull=1#post293786

    summary, msg 569
    https://forum.pjrc.com/threads/68139?p=293921&viewfull=1#post293921

    USB packet loss problem fixed, msg 627
    https://forum.pjrc.com/threads/68139?p=294860&viewfull=1#post294860

  This example code is in the public domain.
*/
#include <MTP_Teensy.h>

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

// Add in MTPD objects
MTPStorage storage;
MTPD mtpd(&storage);

FS *mscDisk;

#define USE_MEMORY_FS

uint32_t MEMFS_SIZE = 65536; // probably more than enough...
#ifdef USE_MEMORY_FS
#include <MemFile.h>
MemFS memfs;
#else
#include <LittleFS.h>
LittleFS_RAM lfsram;
#endif
#include "BogusFS.h"
BogusFS bogusfs;


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
  mtpd.begin();
// lets initialize a RAM drive.
#if defined ARDUINO_TEENSY41
  if (external_psram_size)
    MEMFS_SIZE = 4 * 1024 * 1024;
#endif
#ifdef USE_MEMORY_FS
  if (memfs.begin(MEMFS_SIZE)) {
    Serial.printf("Memory Drive of size: %u initialized\n", MEMFS_SIZE);
    uint32_t istore = storage.addFilesystem(memfs, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  mscDisk = &memfs;  // so we don't start of with NULL pointer
#else
  if (lfsram.begin(MEMFS_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", MEMFS_SIZE);
    uint32_t istore = storage.addFilesystem(lfsram, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  mscDisk = &lfsram;  // so we don't start of with NULL pointer
#endif

  storage.addFilesystem(bogusfs, "Bogus");

  Serial.println("Bogus and MTP initialized.");
  menu();
}

void loop() {
  mtpd.loop();

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
    case 's': {
      Serial.println("\nLogging Data!!!");
      write_data = true; // sets flag to continue to write data until new
      // command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append
      // data to
      // to the file created.
      dataFile = mscDisk->open("datalog.txt", FILE_WRITE);
      logData();
    } break;
    case 'x':
      stopLogging();
      break;
    case '1': {
      // first dump list of storages:
      uint32_t fsCount = storage.getFSCount();
      Serial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                      mtpd.Store2Storage(ii), storage.getStoreName(ii),
                      (uint32_t)storage.getStoreFS(ii));
      }
      Serial.println("\nDump Index List");
      storage.dumpIndexList();
    } break;
    case '2':
      Serial.printf("Drive # %d Selected\n", drive_index);
      mscDisk = storage.getStoreFS(drive_index);
      break;
    case 'd':
      dumpLog();
      break;
    case 'r':
      Serial.println("Send Device Reset Event");
      mtpd.send_DeviceResetEvent();
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
    Serial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging() {
  Serial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  Serial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}

void dumpLog() {
  Serial.println("\nDumping Log!!!");
  // open the file.
  dataFile = mscDisk->open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}


void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List USB Drives (Step 1)");
  Serial.println("\t2 - Select USB Drive for Logging (Step 2)");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
  Serial.println("\ts - Start Logging data (Restarting logger will append "
                 "records to existing log)");
  Serial.println("\tx - Stop Logging data");
  Serial.println("\td - Dump Log");
  Serial.println("\tr - reset MTP");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles() {
  Serial.print("\n Space Used = ");
  Serial.println(mscDisk->usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(mscDisk->totalSize());

  File root = mscDisk->open("/");
  printDirectory(root, 0);
  root.close();
}

void eraseFiles() {
  /*
    PFsVolume partVol;
    if (!partVol.begin(sdx[1].sdfs.card(), true, 1)) {
      Serial.println("Failed to initialize partition");
      return;
    }
    if (pfsLIB.formatter(partVol)) {
      Serial.println("\nFiles erased !");
      mtpd.send_DeviceResetEvent();
    }
    */
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
