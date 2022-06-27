/*
  Simple MTP example that will add a RAM and Flash drive plus handles allowing you to insert one or more USB drives
  and shows those as well.

*/
#include <MTP_Teensy.h>
#include <USBHost_t36.h>
#include <LittleFS.h>

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// Instances for the number of USB drives you are using.
USBDrive myDrive1(myusb);
USBDrive myDrive2(myusb);
USBDrive *drive_list[] = {&myDrive1, &myDrive2};
bool drive_previous_connected[] = {false, false};

// Instances for accessing the files on each drive
USBFilesystem pf1(myusb);
USBFilesystem pf2(myusb);
USBFilesystem pf3(myusb);
USBFilesystem pf4(myusb);
USBFilesystem pf5(myusb);
USBFilesystem pf6(myusb);
USBFilesystem pf7(myusb);
USBFilesystem pf8(myusb);

USBFilesystem *filesystem_list[] = {&pf1, &pf2, &pf3, &pf4, &pf5, &pf6, &pf7, &pf8};

#define CNT_USBFS  (sizeof(filesystem_list)/sizeof(filesystem_list[0]))
uint32_t filesystem_list_store_ids[CNT_USBFS] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
char  filesystem_list_display_name[CNT_USBFS][20];


uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;

#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif
FS *SelectedFS;


class RAMStream : public Stream {
public:
  // overrides for Stream
  virtual int available() { return (tail_ - head_); }
  virtual int read() { return (tail_ != head_) ? buffer_[head_++] : -1; }
  virtual int peek() { return (tail_ != head_) ? buffer_[head_] : -1; }

  // overrides for Print
  virtual size_t write(uint8_t b) {
    if (tail_ < sizeof(buffer_)) {
      buffer_[tail_++] = b;
      return 1;
    }
    return 0;
  }

  enum { BUFFER_SIZE = 32768 };
  uint8_t buffer_[BUFFER_SIZE];
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
};


void setup() {
  // Open serial communications and wait for port to open:

  // setup to do quick and dirty ram stream until Serial or like is up...
  RAMStream rstream;
  // startup mtp.. SO to not timeout...
  MTP.PrintStream(&rstream); // Setup which stream to use...
  MTP.begin();

  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
  // set to real stream
  MTP.PrintStream(&Serial); // Setup which stream to use...

  if (CrashReport)  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  int ch;
  while ((ch = rstream.read()) != -1)
    Serial.write(ch);


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
  SelectedFS = &lfsram;  // so we don't start of with NULL pointer

  myusb.begin();

  Serial.print("Initializing USB Drives ...");
  checkUSBDrives();
  Serial.println("Initializaton complete.");

  menu();
}

void loop() {
  checkUSBDrives();
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
      SelectedFS = MTP.getFilesystemByIndex(drive_index);
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

void checkUSBDrives() {
  myusb.Task();

  // lets chec each of the drives.
  //bool drive_list_changed = false;
#if 0
  for (uint16_t drive_index = 0; drive_index < (sizeof(drive_list)/sizeof(drive_list[0])); drive_index++) {
    USBDrive *pdrive = drive_list[drive_index];
    if (*pdrive) {
      if (!drive_previous_connected[drive_index] || !pdrive->filesystemsStarted()) {
        Serial.printf("\n === Drive index %d found ===\n", drive_index);
        pdrive->startFilesystems();
        Serial.printf("\nTry Partition list");
        pdrive->printPartionTable(Serial);
        //drive_list_changed = true;
        drive_previous_connected[drive_index] = true;
      }
    } else if (drive_previous_connected[drive_index]) {
      Serial.printf("\n === Drive index %d removed ===\n", drive_index);
      drive_previous_connected[drive_index] = false;
      //drive_list_changed = true;
    }
  }
  bool send_device_reset = false;
  for (uint8_t i = 0; i < CNT_USBFS; i++) {
    if (*filesystem_list[i]) {
      // So file system is valid do we have a MTP id for it?
      if ( (filesystem_list_store_ids[i] == 0xFFFFFFFFUL)) {
        Serial.printf("Found new Volume:%u\n", i); Serial.flush();
        // Lets see if we can get the volume label:
        char volName[20];
        if (filesystem_list[i]->mscfs.getVolumeLabel(volName, sizeof(volName)))
          snprintf(filesystem_list_display_name[i], sizeof(filesystem_list_display_name[i]), "MSC%d-%s", i, volName);
        else
          snprintf(filesystem_list_display_name[i], sizeof(filesystem_list_display_name[i]), "MSC%d", i);
        filesystem_list_store_ids[i] = MTP.addFilesystem(*filesystem_list[i], filesystem_list_display_name[i]);

        // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
        if (MTP.send_StoreAddedEvent(filesystem_list_store_ids[i]) < 0) send_device_reset = true;
      } else if (filesystem_list[i]->changed()) {
        // Something with this file system changed. 
        Serial.printf("File system changed volume: index=%d, store id:%x\n", i, filesystem_list_store_ids[i]);
        Serial.println("Show Partition list");
        filesystem_list[i]->device->printPartionTable(Serial);
        filesystem_list[i]->changed(false);
      }
    }
    // Or did volume go away?
    else if ((filesystem_list_store_ids[i] != 0xFFFFFFFFUL) && !*filesystem_list[i] ) {
      Serial.printf("Remove volume: index=%d, store id:%x\n", i, filesystem_list_store_ids[i]);
      if (MTP.send_StoreRemovedEvent(filesystem_list_store_ids[i]) < 0) send_device_reset = true;
      MTP.storage()->removeFilesystem(filesystem_list_store_ids[i]);
      // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
      filesystem_list_store_ids[i] = 0xFFFFFFFFUL;
    }
  }
  if (send_device_reset) MTP.send_DeviceResetEvent();
#endif
}



void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List Drives");
  Serial.println("\t2 - Select USB Drive");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
  Serial.println("\tr - reset MTP");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles() {
  if (SelectedFS == nullptr) {
    Serial.println("Error: No Filesystem selected");
    return;
  }
  Serial.print("\n Space Used = ");
  Serial.println(SelectedFS->usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(SelectedFS->totalSize());

  File root = SelectedFS->open("/");
  printDirectory(root, 0);
  root.close();
}

void eraseFiles() {
  if (SelectedFS == nullptr) {
    Serial.println("Error: No Filesystem selected");
    return;
  }
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
