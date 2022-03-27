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
USBSerialEmu userialEMU(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &hid1, &hid2, &hid3, &hid4, &hid5,
                        &userial, &keyboard1, &joystick1
                       };
#define CNT_DEVICES (sizeof(drivers)/sizeof(drivers[0]))
const char * driver_names[CNT_DEVICES] = {"Hub1", "Hub2",  "HID1", "HID2", "HID3", "HID4", "HID5"
                                          "USERIAL1", "KB1", "JOY1D"
                                         };

bool driver_active[CNT_DEVICES] = {false, false, false, false};

// Lets also look at HID Input devices
USBHIDInput *hiddrivers[] = {&mouse1, &joystick1, &userialEMU};
#define CNT_HIDDEVICES (sizeof(hiddrivers)/sizeof(hiddrivers[0]))
const char * hid_driver_names[CNT_DEVICES] = {"Mouse1", "Joystick1", "SerEMU"};
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

LittleFS_Program myfs;
char download_file_name[256] = "Blinky.txt";

// NOTE: This option is only available on the Teensy 4.0, Teensy 4.1 and Teensy Micromod boards.
// With the additonal option for security on the T4 the maximum flash available for a
// program disk with LittleFS is 960 blocks of 1024 bytes
#define PROG_FLASH_SIZE 1024 * 1024 * 1 // Specify size to use of onboard Teensy Program Flash chip
// This creates a LittleFS drive in Teensy PCB FLash.

File dataFile;  // Specifes that dataFile is of File type


#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif

// Quick and Dirty memory Stream...
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
  // setup to do quick and dirty ram stream until Serial or like is up...
  RAMStream rstream;

  pinMode(5, OUTPUT);
  // Open serial communications and wait for port to open:
//  delay(3000);

  // startup mtp.. SO to not timeout...
  MTP.PrintStream(&rstream); // Setup which stream to use...
  MTP.begin();
  // Open serial communications and wait for port to open:
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }

#if defined(USB_MTPDISK_DUAL_SERIAL)
  SerialUSB1.begin(115200);
  Serial.println("*** WE HAVE DUAL SERIAL ***");
#endif

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
  mscDisk = &lfsram;  // so we don't start of with NULL pointer

  if (myfs.begin(PROG_FLASH_SIZE)) {
    Serial.printf("Program Drive of size: %u initialized\n", PROG_FLASH_SIZE);
    MTP.addFilesystem(myfs, "PGM");
  }

  myusb.begin();

  Serial.print("Initializing USB devices ...");
  keyboard1.attachPress(OnPress);
  keyboard1.attachExtrasPress(OnHIDExtrasPress);
  keyboard1.attachExtrasRelease(OnHIDExtrasRelease);

  Serial.println("\nInitializing USB MSC drives...");
  checkMSCChanges();
  Serial.println("MSC and MTP initialized.");

  menu();
}

char Delete = 127;

void loop() {
  checkMSCChanges();
  MTP.loop();
  CheckForDeviceChanges();
#if defined(USB_MTPDISK_DUAL_SERIAL)
  if (userial && userial.available()) {
    while (userial.available()) SerialUSB1.write(userial.read());
  }
  if (SerialUSB1.available()) {
    for (;;) {
      int ch = SerialUSB1.read();
      if (ch == -1) break;
      if (userial)userial.write(ch);
      //Serial.println(ch, HEX);
    }
  }
#else
  if (userial && userial.available()) {
    Serial.print("$$USerial:");
    while (userial.available()) Serial.write(userial.read());
  }
#endif
  if (Serial.available()) {
    uint8_t command = Serial.read();
    int ch = Serial.read();
    if (command == '$') {
      while (ch != -1) {
        // hack use $ to signal ctrl_
        if (ch == '$') {
          ch = Serial.read();
          //Serial.print("$: ");Serial.println(ch);
          if (ch == '$') ch = Delete;
          if (ch == 10) { ch &= 0x1f; break; }
          ch &= 0x1f; // get into ctrl range
        }

        if (userial) userial.write(ch);
        ch = Serial.read();
      }

    } else {
      while (ch == ' ')  ch = Serial.read();
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
        {
          uint32_t drive_index = CommandLineReadNextNumber(ch, 0);
          Serial.printf("Drive # %d Selected\n", drive_index);
          mscDisk = MTP.getFilesystemByIndex(drive_index);
        }
        break;
      case 'r':
        Serial.println("Send Device Reset Event");
        MTP.send_DeviceResetEvent();
        break;
      case 'd':
        readFile(ch);
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
}

void readFile(int ch)
{
  // see if we have a filename
  if (ch > ' ') {
    // maybe new file name
    char *psz = download_file_name;
    while (ch > ' ') {
      *psz++ = ch;
      ch = Serial.read();
    }
  }

  Serial.printf("\nDownloading  File%s!!!\n", download_file_name);
  // open the file.
  dataFile = myfs.open(download_file_name);

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      //Serial.println(dataFile.read(), DEC);
      int pycomm = dataFile.read();
      while (pycomm != -1) {
        // hack use $ to signal ctrl_
        if (pycomm == '$') {
          pycomm = dataFile.read();
          if (pycomm == -1) break;
          //Serial.print("$: ");Serial.println(pycomm);
          if (pycomm == '$') pycomm = Delete;
          //if (pycomm == 10) { pycomm &= 0x1f; break; }
          else pycomm &= 0x1f; // get into ctrl range
          //Serial.println(pycomm);
        }

        if (userial) userial.write(pycomm);
        pycomm = Serial.read();
        //Serial.print("1: ");Serial.println(pycomm);
      }
      if (userial && userial.available()) {
        Serial.print("$$USerial:");
        while (userial.available()) Serial.write(userial.read());
      }
    }
    dataFile.close();
    Serial.println("Download Complete");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.printf("error opening %s\n", download_file_name);
  }
}

void checkMSCChanges() {
  myusb.Task();

  USBMSCDevice mscDrive;
  PFsLib pfsLIB;
  for (uint8_t i = 0; i < CNT_DRIVES; i++) {
    if (*pdrives[i]) {
      if (!drive_previous_connected[i]) {
        Serial.println("\n@@@@@@@@@@@@@@@ NEW Drives @@@@@@@@@@@");
        if (mscDrive.begin(pdrives[i])) {
          Serial.println("\t ## new drive");
          Serial.printf("\nUSB Drive: %u connected\n", i);
          pfsLIB.mbrDmp(&mscDrive, (uint32_t) - 1, Serial);
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
  Serial.println("\td[<filename>] - download to MicroPython");
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
        } else if (drivers[i] == &userial) {
          // Lets try first outputting something to our USerial to see if it will go out...
          userial.begin(115200);
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

void OnPress(int key)
{
  Serial.print("key '");
  switch (key) {
  case KEYD_UP       : Serial.print("UP"); break;
  case KEYD_DOWN    : Serial.print("DN"); break;
  case KEYD_LEFT     : Serial.print("LEFT"); break;
  case KEYD_RIGHT   : Serial.print("RIGHT"); break;
  case KEYD_INSERT   : Serial.print("Ins"); break;
  case KEYD_DELETE   : Serial.print("Del"); break;
  case KEYD_PAGE_UP  : Serial.print("PUP"); break;
  case KEYD_PAGE_DOWN: Serial.print("PDN"); break;
  case KEYD_HOME     : Serial.print("HOME"); break;
  case KEYD_END      : Serial.print("END"); break;
  case KEYD_F1       : Serial.print("F1"); break;
  case KEYD_F2       : Serial.print("F2"); break;
  case KEYD_F3       : Serial.print("F3"); break;
  case KEYD_F4       : Serial.print("F4"); break;
  case KEYD_F5       : Serial.print("F5"); break;
  case KEYD_F6       : Serial.print("F6"); break;
  case KEYD_F7       : Serial.print("F7"); break;
  case KEYD_F8       : Serial.print("F8"); break;
  case KEYD_F9       : Serial.print("F9"); break;
  case KEYD_F10      : Serial.print("F10"); break;
  case KEYD_F11      : Serial.print("F11"); break;
  case KEYD_F12      : Serial.print("F12"); break;
  default: Serial.print((char)key); break;
  }
  Serial.print("'  ");
  Serial.print(key);
  Serial.print(" MOD: ");
  Serial.print(keyboard1.getModifiers(), HEX);
  Serial.print(" OEM: ");
  Serial.print(keyboard1.getOemKey(), HEX);
  Serial.print(" LEDS: ");
  Serial.println(keyboard1.LEDS(), HEX);
}

void OnHIDExtrasPress(uint32_t top, uint16_t key)
{
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key press:");
  Serial.print(key, HEX);
  if (top == 0xc0000) {
    switch (key) {
    case  0x20 : Serial.print(" - +10"); break;
    case  0x21 : Serial.print(" - +100"); break;
    case  0x22 : Serial.print(" - AM/PM"); break;
    case  0x30 : Serial.print(" - Power"); break;
    case  0x31 : Serial.print(" - Reset"); break;
    case  0x32 : Serial.print(" - Sleep"); break;
    case  0x33 : Serial.print(" - Sleep After"); break;
    case  0x34 : Serial.print(" - Sleep Mode"); break;
    case  0x35 : Serial.print(" - Illumination"); break;
    case  0x36 : Serial.print(" - Function Buttons"); break;
    case  0x40 : Serial.print(" - Menu"); break;
    case  0x41 : Serial.print(" - Menu  Pick"); break;
    case  0x42 : Serial.print(" - Menu Up"); break;
    case  0x43 : Serial.print(" - Menu Down"); break;
    case  0x44 : Serial.print(" - Menu Left"); break;
    case  0x45 : Serial.print(" - Menu Right"); break;
    case  0x46 : Serial.print(" - Menu Escape"); break;
    case  0x47 : Serial.print(" - Menu Value Increase"); break;
    case  0x48 : Serial.print(" - Menu Value Decrease"); break;
    case  0x60 : Serial.print(" - Data On Screen"); break;
    case  0x61 : Serial.print(" - Closed Caption"); break;
    case  0x62 : Serial.print(" - Closed Caption Select"); break;
    case  0x63 : Serial.print(" - VCR/TV"); break;
    case  0x64 : Serial.print(" - Broadcast Mode"); break;
    case  0x65 : Serial.print(" - Snapshot"); break;
    case  0x66 : Serial.print(" - Still"); break;
    case  0x80 : Serial.print(" - Selection"); break;
    case  0x81 : Serial.print(" - Assign Selection"); break;
    case  0x82 : Serial.print(" - Mode Step"); break;
    case  0x83 : Serial.print(" - Recall Last"); break;
    case  0x84 : Serial.print(" - Enter Channel"); break;
    case  0x85 : Serial.print(" - Order Movie"); break;
    case  0x86 : Serial.print(" - Channel"); break;
    case  0x87 : Serial.print(" - Media Selection"); break;
    case  0x88 : Serial.print(" - Media Select Computer"); break;
    case  0x89 : Serial.print(" - Media Select TV"); break;
    case  0x8A : Serial.print(" - Media Select WWW"); break;
    case  0x8B : Serial.print(" - Media Select DVD"); break;
    case  0x8C : Serial.print(" - Media Select Telephone"); break;
    case  0x8D : Serial.print(" - Media Select Program Guide"); break;
    case  0x8E : Serial.print(" - Media Select Video Phone"); break;
    case  0x8F : Serial.print(" - Media Select Games"); break;
    case  0x90 : Serial.print(" - Media Select Messages"); break;
    case  0x91 : Serial.print(" - Media Select CD"); break;
    case  0x92 : Serial.print(" - Media Select VCR"); break;
    case  0x93 : Serial.print(" - Media Select Tuner"); break;
    case  0x94 : Serial.print(" - Quit"); break;
    case  0x95 : Serial.print(" - Help"); break;
    case  0x96 : Serial.print(" - Media Select Tape"); break;
    case  0x97 : Serial.print(" - Media Select Cable"); break;
    case  0x98 : Serial.print(" - Media Select Satellite"); break;
    case  0x99 : Serial.print(" - Media Select Security"); break;
    case  0x9A : Serial.print(" - Media Select Home"); break;
    case  0x9B : Serial.print(" - Media Select Call"); break;
    case  0x9C : Serial.print(" - Channel Increment"); break;
    case  0x9D : Serial.print(" - Channel Decrement"); break;
    case  0x9E : Serial.print(" - Media Select SAP"); break;
    case  0xA0 : Serial.print(" - VCR Plus"); break;
    case  0xA1 : Serial.print(" - Once"); break;
    case  0xA2 : Serial.print(" - Daily"); break;
    case  0xA3 : Serial.print(" - Weekly"); break;
    case  0xA4 : Serial.print(" - Monthly"); break;
    case  0xB0 : Serial.print(" - Play"); break;
    case  0xB1 : Serial.print(" - Pause"); break;
    case  0xB2 : Serial.print(" - Record"); break;
    case  0xB3 : Serial.print(" - Fast Forward"); break;
    case  0xB4 : Serial.print(" - Rewind"); break;
    case  0xB5 : Serial.print(" - Scan Next Track"); break;
    case  0xB6 : Serial.print(" - Scan Previous Track"); break;
    case  0xB7 : Serial.print(" - Stop"); break;
    case  0xB8 : Serial.print(" - Eject"); break;
    case  0xB9 : Serial.print(" - Random Play"); break;
    case  0xBA : Serial.print(" - Select DisC"); break;
    case  0xBB : Serial.print(" - Enter Disc"); break;
    case  0xBC : Serial.print(" - Repeat"); break;
    case  0xBD : Serial.print(" - Tracking"); break;
    case  0xBE : Serial.print(" - Track Normal"); break;
    case  0xBF : Serial.print(" - Slow Tracking"); break;
    case  0xC0 : Serial.print(" - Frame Forward"); break;
    case  0xC1 : Serial.print(" - Frame Back"); break;
    case  0xC2 : Serial.print(" - Mark"); break;
    case  0xC3 : Serial.print(" - Clear Mark"); break;
    case  0xC4 : Serial.print(" - Repeat From Mark"); break;
    case  0xC5 : Serial.print(" - Return To Mark"); break;
    case  0xC6 : Serial.print(" - Search Mark Forward"); break;
    case  0xC7 : Serial.print(" - Search Mark Backwards"); break;
    case  0xC8 : Serial.print(" - Counter Reset"); break;
    case  0xC9 : Serial.print(" - Show Counter"); break;
    case  0xCA : Serial.print(" - Tracking Increment"); break;
    case  0xCB : Serial.print(" - Tracking Decrement"); break;
    case  0xCD : Serial.print(" - Pause/Continue"); break;
    case  0xE0 : Serial.print(" - Volume"); break;
    case  0xE1 : Serial.print(" - Balance"); break;
    case  0xE2 : Serial.print(" - Mute"); break;
    case  0xE3 : Serial.print(" - Bass"); break;
    case  0xE4 : Serial.print(" - Treble"); break;
    case  0xE5 : Serial.print(" - Bass Boost"); break;
    case  0xE6 : Serial.print(" - Surround Mode"); break;
    case  0xE7 : Serial.print(" - Loudness"); break;
    case  0xE8 : Serial.print(" - MPX"); break;
    case  0xE9 : Serial.print(" - Volume Up"); break;
    case  0xEA : Serial.print(" - Volume Down"); break;
    case  0xF0 : Serial.print(" - Speed Select"); break;
    case  0xF1 : Serial.print(" - Playback Speed"); break;
    case  0xF2 : Serial.print(" - Standard Play"); break;
    case  0xF3 : Serial.print(" - Long Play"); break;
    case  0xF4 : Serial.print(" - Extended Play"); break;
    case  0xF5 : Serial.print(" - Slow"); break;
    case  0x100: Serial.print(" - Fan Enable"); break;
    case  0x101: Serial.print(" - Fan Speed"); break;
    case  0x102: Serial.print(" - Light"); break;
    case  0x103: Serial.print(" - Light Illumination Level"); break;
    case  0x104: Serial.print(" - Climate Control Enable"); break;
    case  0x105: Serial.print(" - Room Temperature"); break;
    case  0x106: Serial.print(" - Security Enable"); break;
    case  0x107: Serial.print(" - Fire Alarm"); break;
    case  0x108: Serial.print(" - Police Alarm"); break;
    case  0x150: Serial.print(" - Balance Right"); break;
    case  0x151: Serial.print(" - Balance Left"); break;
    case  0x152: Serial.print(" - Bass Increment"); break;
    case  0x153: Serial.print(" - Bass Decrement"); break;
    case  0x154: Serial.print(" - Treble Increment"); break;
    case  0x155: Serial.print(" - Treble Decrement"); break;
    case  0x160: Serial.print(" - Speaker System"); break;
    case  0x161: Serial.print(" - Channel Left"); break;
    case  0x162: Serial.print(" - Channel Right"); break;
    case  0x163: Serial.print(" - Channel Center"); break;
    case  0x164: Serial.print(" - Channel Front"); break;
    case  0x165: Serial.print(" - Channel Center Front"); break;
    case  0x166: Serial.print(" - Channel Side"); break;
    case  0x167: Serial.print(" - Channel Surround"); break;
    case  0x168: Serial.print(" - Channel Low Frequency Enhancement"); break;
    case  0x169: Serial.print(" - Channel Top"); break;
    case  0x16A: Serial.print(" - Channel Unknown"); break;
    case  0x170: Serial.print(" - Sub-channel"); break;
    case  0x171: Serial.print(" - Sub-channel Increment"); break;
    case  0x172: Serial.print(" - Sub-channel Decrement"); break;
    case  0x173: Serial.print(" - Alternate Audio Increment"); break;
    case  0x174: Serial.print(" - Alternate Audio Decrement"); break;
    case  0x180: Serial.print(" - Application Launch Buttons"); break;
    case  0x181: Serial.print(" - AL Launch Button Configuration Tool"); break;
    case  0x182: Serial.print(" - AL Programmable Button Configuration"); break;
    case  0x183: Serial.print(" - AL Consumer Control Configuration"); break;
    case  0x184: Serial.print(" - AL Word Processor"); break;
    case  0x185: Serial.print(" - AL Text Editor"); break;
    case  0x186: Serial.print(" - AL Spreadsheet"); break;
    case  0x187: Serial.print(" - AL Graphics Editor"); break;
    case  0x188: Serial.print(" - AL Presentation App"); break;
    case  0x189: Serial.print(" - AL Database App"); break;
    case  0x18A: Serial.print(" - AL Email Reader"); break;
    case  0x18B: Serial.print(" - AL Newsreader"); break;
    case  0x18C: Serial.print(" - AL Voicemail"); break;
    case  0x18D: Serial.print(" - AL Contacts/Address Book"); break;
    case  0x18E: Serial.print(" - AL Calendar/Schedule"); break;
    case  0x18F: Serial.print(" - AL Task/Project Manager"); break;
    case  0x190: Serial.print(" - AL Log/Journal/Timecard"); break;
    case  0x191: Serial.print(" - AL Checkbook/Finance"); break;
    case  0x192: Serial.print(" - AL Calculator"); break;
    case  0x193: Serial.print(" - AL A/V Capture/Playback"); break;
    case  0x194: Serial.print(" - AL Local Machine Browser"); break;
    case  0x195: Serial.print(" - AL LAN/WAN Browser"); break;
    case  0x196: Serial.print(" - AL Internet Browser"); break;
    case  0x197: Serial.print(" - AL Remote Networking/ISP Connect"); break;
    case  0x198: Serial.print(" - AL Network Conference"); break;
    case  0x199: Serial.print(" - AL Network Chat"); break;
    case  0x19A: Serial.print(" - AL Telephony/Dialer"); break;
    case  0x19B: Serial.print(" - AL Logon"); break;
    case  0x19C: Serial.print(" - AL Logoff"); break;
    case  0x19D: Serial.print(" - AL Logon/Logoff"); break;
    case  0x19E: Serial.print(" - AL Terminal Lock/Screensaver"); break;
    case  0x19F: Serial.print(" - AL Control Panel"); break;
    case  0x1A0: Serial.print(" - AL Command Line Processor/Run"); break;
    case  0x1A1: Serial.print(" - AL Process/Task Manager"); break;
    case  0x1A2: Serial.print(" - AL Select Tast/Application"); break;
    case  0x1A3: Serial.print(" - AL Next Task/Application"); break;
    case  0x1A4: Serial.print(" - AL Previous Task/Application"); break;
    case  0x1A5: Serial.print(" - AL Preemptive Halt Task/Application"); break;
    case  0x200: Serial.print(" - Generic GUI Application Controls"); break;
    case  0x201: Serial.print(" - AC New"); break;
    case  0x202: Serial.print(" - AC Open"); break;
    case  0x203: Serial.print(" - AC Close"); break;
    case  0x204: Serial.print(" - AC Exit"); break;
    case  0x205: Serial.print(" - AC Maximize"); break;
    case  0x206: Serial.print(" - AC Minimize"); break;
    case  0x207: Serial.print(" - AC Save"); break;
    case  0x208: Serial.print(" - AC Print"); break;
    case  0x209: Serial.print(" - AC Properties"); break;
    case  0x21A: Serial.print(" - AC Undo"); break;
    case  0x21B: Serial.print(" - AC Copy"); break;
    case  0x21C: Serial.print(" - AC Cut"); break;
    case  0x21D: Serial.print(" - AC Paste"); break;
    case  0x21E: Serial.print(" - AC Select All"); break;
    case  0x21F: Serial.print(" - AC Find"); break;
    case  0x220: Serial.print(" - AC Find and Replace"); break;
    case  0x221: Serial.print(" - AC Search"); break;
    case  0x222: Serial.print(" - AC Go To"); break;
    case  0x223: Serial.print(" - AC Home"); break;
    case  0x224: Serial.print(" - AC Back"); break;
    case  0x225: Serial.print(" - AC Forward"); break;
    case  0x226: Serial.print(" - AC Stop"); break;
    case  0x227: Serial.print(" - AC Refresh"); break;
    case  0x228: Serial.print(" - AC Previous Link"); break;
    case  0x229: Serial.print(" - AC Next Link"); break;
    case  0x22A: Serial.print(" - AC Bookmarks"); break;
    case  0x22B: Serial.print(" - AC History"); break;
    case  0x22C: Serial.print(" - AC Subscriptions"); break;
    case  0x22D: Serial.print(" - AC Zoom In"); break;
    case  0x22E: Serial.print(" - AC Zoom Out"); break;
    case  0x22F: Serial.print(" - AC Zoom"); break;
    case  0x230: Serial.print(" - AC Full Screen View"); break;
    case  0x231: Serial.print(" - AC Normal View"); break;
    case  0x232: Serial.print(" - AC View Toggle"); break;
    case  0x233: Serial.print(" - AC Scroll Up"); break;
    case  0x234: Serial.print(" - AC Scroll Down"); break;
    case  0x235: Serial.print(" - AC Scroll"); break;
    case  0x236: Serial.print(" - AC Pan Left"); break;
    case  0x237: Serial.print(" - AC Pan Right"); break;
    case  0x238: Serial.print(" - AC Pan"); break;
    case  0x239: Serial.print(" - AC New Window"); break;
    case  0x23A: Serial.print(" - AC Tile Horizontally"); break;
    case  0x23B: Serial.print(" - AC Tile Vertically"); break;
    case  0x23C: Serial.print(" - AC Format"); break;

    }
  }
  Serial.println();
}

void OnHIDExtrasRelease(uint32_t top, uint16_t key)
{
  Serial.print("HID (");
  Serial.print(top, HEX);
  Serial.print(") key release:");
  Serial.println(key, HEX);
}

