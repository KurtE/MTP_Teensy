/*
  LittleFS  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include <USBHost_t36.h>
#include <MTP_Teensy.h>
//#include <mscFS.h>
#include <msFilesystem.h>
#include <msDevice.h>


// Add in MTPD objects

// Add USBHost objectsUsbFs
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);

// MSC objects.
msDevice drive1(myusb);
msDevice drive2(myusb);
msDevice drive3(myusb);

msFilesystem msFS1(myusb);
msFilesystem msFS2(myusb);
msFilesystem msFS3(myusb);
msFilesystem msFS4(myusb);
msFilesystem msFS5(myusb);

// See if any HID devices
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);
USBHIDParser hid5(myusb);
MouseController mouse1(myusb);
JoystickController joystick1(myusb);
KeyboardController keyboard1(myusb);
USBSerial_BigBuffer userial(myusb, 1); // Handles anything up to 512 bytes

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &hid4, &hid5, 
    &userial, &keyboard1, &joystick1};
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2",  "HID1", "HID2", "HID3", "HID4", "HID5"
      "USERIAL1", "KB1", "JOY1D" };

bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&mouse1, &joystick1};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"Mouse1", "Joystick1"};
bool hid_driver_active[CNT_DEVICES] = {false, false};


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

  if (CrashReport)  Serial.print(CrashReport);
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
  CheckForDeviceChanges();
  
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
      mscDisk = MTP.getFilesystemByIndex(drive_index);
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

void checkMSCChanges() {
  myusb.Task();

  USBMSCDevice mscDrive;
  PFsLib pfsLIB;
  for (uint8_t i=0; i < CNT_DRIVES; i++) {
    if (*pdrives[i]) {
      if (!drive_previous_connected[i]) {
        Serial.println("\n@@@@@@@@@@@@@@@ NEW Drives @@@@@@@@@@@");
        if (mscDrive.begin(pdrives[i])) {
          Serial.println("\t ## new drive");
          Serial.printf("\nUSB Drive: %u connected\n", i);
          pfsLIB.mbrDmp(&mscDrive, (uint32_t)-1, Serial);
          Serial.println("\nTry Partition list");
          pfsLIB.listPartitions(&mscDrive, Serial);
          drive_previous_connected[i] = true;
        }
        Serial.println("\n@@@@@@@@@@@@@@@ NEW Drives  Completed. @@@@@@@@@@@");
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


void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\t1 - List USB Drives (Step 1)");
  Serial.println("\t2 - Select USB Drive");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
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


void CheckForDeviceChanges() {
  for (uint8_t i = 0; i < CNT_DEVICES; i++) {
    if (*drivers[i] != driver_active[i]) {
      if (driver_active[i]) {
        Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
        driver_active[i] = false;
      } else {
        Serial.printf("*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
        driver_active[i] = true;

        const uint8_t *psz = drivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = drivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = drivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);

        // Note: with some keyboards there is an issue that they don't output in boot protocol mode
        // and may not work.  The above code can try to force the keyboard into boot mode, but there
        // are issues with doing this blindly with combo devices like wireless keyboard/mouse, which
        // may cause the mouse to not work.  Note: the above id is in the builtin list of
        // vendor IDs that are already forced
        if (drivers[i] == &keyboard1) {
          if (keyboard1.idVendor() == 0x04D9) {
            Serial.println("Gigabyte vendor: force boot protocol");
            // Gigabyte keyboard
            keyboard1.forceBootProtocol();
          }
        }
      }
    }
  }

  for (uint8_t i = 0; i < CNT_HIDDEVICES; i++) {
    if (*hiddrivers[i] != hid_driver_active[i]) {
      if (hid_driver_active[i]) {
        Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
        hid_driver_active[i] = false;
      } else {
        Serial.printf("*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
        hid_driver_active[i] = true;

        const uint8_t *psz = hiddrivers[i]->manufacturer();
        if (psz && *psz) Serial.printf("  manufacturer: %s\n", psz);
        psz = hiddrivers[i]->product();
        if (psz && *psz) Serial.printf("  product: %s\n", psz);
        psz = hiddrivers[i]->serialNumber();
        if (psz && *psz) Serial.printf("  Serial: %s\n", psz);
      }
    }
  }
}