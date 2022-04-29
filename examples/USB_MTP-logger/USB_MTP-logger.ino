/*
  LittleFS  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include <MTP_Teensy.h>
//#include <mscFS.h>
#include <msFilesystem.h>
#include <msDevice.h>

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

// Add in MTPD objects

// Add USBHost objectsUsbFs
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub(myusb);

// MSC objects.
msDevice drive1(myusb);
msDevice drive2(myusb);
msDevice drive3(myusb);

msFilesystem msFS1(myusb);
msFilesystem msFS2(myusb);
msFilesystem msFS3(myusb);
msFilesystem msFS4(myusb);
msFilesystem msFS5(myusb);

// Quick and dirty
msFilesystem *pmsFS[] = {&msFS1, &msFS2, &msFS3, &msFS4, &msFS5};
#define CNT_MSC  (sizeof(pmsFS)/sizeof(pmsFS[0]))
uint32_t pmsfs_store_ids[CNT_MSC] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
char  pmsFS_display_name[CNT_MSC][20];

msDevice *pdrives[] {&drive1, &drive2, &drive3};
#define CNT_DRIVES  (sizeof(pdrives)/sizeof(pdrives[0]))
bool drive_previous_connected[CNT_DRIVES] = {false, false, false};

FS *mscDisk;

#include <LittleFS.h>
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;

#include "BogusFS.h"
BogusFS bogusfs;


#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif


void setup() {
  pinMode(5, OUTPUT);
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
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
    LFSRAM_SIZE = 4 * 1024 * 1024;
#endif
  if (lfsram.begin(LFSRAM_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "RAM");
    Serial.printf("Set Storage Index drive to %u\n", istore);
  }
  mscDisk = &lfsram;  // so we don't start of with NULL pointer

  MTP.addFilesystem(bogusfs, "Bogus");

  myusb.begin();

  Serial.print("Initializing MSC Drives ...");

  Serial.println("\nInitializing USB MSC drives...");

  Serial.println("MSC and MTP initialized.");
  checkMSCChanges();

  menu();
}

void loop() {
  checkMSCChanges();
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
      uint32_t fsCount = MTP.getFilesystemCount();
      Serial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                      MTP.Store2Storage(ii), MTP.getFilesystemNameByIndex(ii),
                      (uint32_t)MTP.getFilesystemNameByIndex(ii));
      }
      Serial.println("\nDump Index List");
      MTP.storage()->dumpIndexList();
    } break;
    case '2':
      Serial.printf("Drive # %d Selected\n", drive_index);
      mscDisk = MTP.getFilesystemByIndex(drive_index);
      break;
    case 'd':
      dumpLog();
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

  if (write_data)
    logData();
}

void checkMSCChanges() {
  myusb.Task();

  USBMSCDevice mscDrive;
  PFsLib pfsLIB;
  for (uint8_t i=0; i < CNT_DRIVES; i++) {
    if (*pdrives[i]) {
      if (!drive_previous_connected[i]) {
        if (mscDrive.begin(pdrives[i])) {
          Serial.printf("\nUSB Drive: %u connected\n", i);
          pfsLIB.mbrDmp(&mscDrive, (uint32_t)-1, Serial);
          Serial.printf("\nTry Partition list");
          pfsLIB.listPartitions(&mscDrive, Serial);
          drive_previous_connected[i] = true;
        }
      }
    } else {
      drive_previous_connected[i] = false;
    }
  }
  bool send_device_reset = false;
  for (uint8_t i = 0; i < CNT_MSC; i++) {
    if (*pmsFS[i] && (pmsfs_store_ids[i] == 0xFFFFFFFFUL)) {
      Serial.printf("Found new Volume:%u\n", i); Serial.flush();
      // Lets see if we can get the volume label:
      char volName[20];
      if (pmsFS[i]->mscfs.getVolumeLabel(volName, sizeof(volName)))
        snprintf(pmsFS_display_name[i], sizeof(pmsFS_display_name[i]), "MSC%d-%s", i, volName);
      else
        snprintf(pmsFS_display_name[i], sizeof(pmsFS_display_name[i]), "MSC%d", i);
      pmsfs_store_ids[i] = MTP.addFilesystem(*pmsFS[i], pmsFS_display_name[i]);

      // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
      if (MTP.send_StoreAddedEvent(pmsfs_store_ids[i]) < 0) send_device_reset = true;
    }
    // Or did volume go away?
    else if ((pmsfs_store_ids[i] != 0xFFFFFFFFUL) && !*pmsFS[i] ) {
      if (MTP.send_StoreRemovedEvent(pmsfs_store_ids[i]) < 0) send_device_reset = true;
      MTP.storage()->removeFilesystem(pmsfs_store_ids[i]);
      // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
      pmsfs_store_ids[i] = 0xFFFFFFFFUL;
    }
  }
  if (send_device_reset) MTP.send_DeviceResetEvent();
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
  MTP.send_DeviceResetEvent();
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
      MTP.send_DeviceResetEvent();
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
