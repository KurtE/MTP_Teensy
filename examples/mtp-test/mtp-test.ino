#include <SD.h>
#include <MTP_Teensy.h>

//---------------------------------------------------
// Select drives you want to create
//---------------------------------------------------
#define USE_SD  1         // SDFAT based SDIO and SPI
#ifdef ARDUINO_TEENSY41
#define USE_LFS_RAM 0     // T4.1 PSRAM (or RAM)
#else
#define USE_LFS_RAM 0     // T4.1 PSRAM (or RAM)
#endif
#ifdef ARDUINO_TEENSY_MICROMOD
#define USE_LFS_QSPI 0    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_SPI 0     // SPI Flash
#define USE_LFS_NAND 0
#define USE_LFS_QSPI_NAND 0
#define USE_LFS_FRAM 0
#else
#define USE_LFS_QSPI 1    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_SPI 1     // SPI Flash
#define USE_LFS_NAND 0
#define USE_LFS_QSPI_NAND 0
#define USE_LFS_FRAM 0
#endif
#define USE_MSC 3    // set to > 0 experiment with MTP (USBHost.t36 + mscFS)
#define USE_SW_PU  0 //set to 1 if SPI devices does not have PUs,
            // https://www.pjrc.com/better-spi-bus-design-in-3-steps/


#define DBGSerial Serial
File dataFile;  // Specifes that dataFile is of File type
int record_count = 0;
bool write_data = false;
uint8_t current_store = 0;

//=============================================================================
// Global defines
//=============================================================================


MTPStorage_SD storage;
MTPD    mtpd(&storage);

//=============================================================================
// MSC & SD classes
//=============================================================================
#if USE_SD==1
#include <SDMTPClass.h>

#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD  BUILTIN_SDCARD
#else
#define CS_SD 10
#endif
#define SPI_SPEED SD_SCK_MHZ(16)  // adjust to sd card 

#define COUNT_MYFS  1  // could do by count, but can limit how many are created...
SDMTPClass mySD[] = {
                      {mtpd, storage, "SDIO", CS_SD}, 
                      {mtpd, storage, "SD8", 8, 9, SHARED_SPI, SPI_SPEED}
                    };
//SDMTPClass mySD(mtpd, storage, "SD10", 10, 0xff, SHARED_SPI, SPI_SPEED);

#endif


//========================================================================
//This puts a the index file in memory as opposed to an SD Card in memory
//and uses it as the starting pointer to a filesystem so keep no matter
//========================================================================
#include "LittleFS.h"
#define LFSRAM_SIZE 65536  // probably more than enough...
LittleFS_RAM lfsram;
#include <LFS_MTP_Callback.h>  //callback for LittleFS format
LittleFSMTPCB lfsmtpcb;
FS *myfs = &lfsram; // current default FS...
  
//=========================================================================
// USB MSC Class setup
//=========================================================================
#if USE_MSC > 0
#include <USB_MSC_MTP.h>
USB_MSC_MTP usbmsc(mtpd, storage);
#endif


// =======================================================================
// Set up LittleFS file systems on different storage media
// =======================================================================
#if USE_LFS_RAM==1
const char *lfs_ram_str[] = {"RAM1", "RAM2"};  // edit to reflect your configuration
const int lfs_ram_size[] = {200'000,4'000'000}; // edit to reflect your configuration
const int nfs_ram = sizeof(lfs_ram_str)/sizeof(const char *);
LittleFS_RAM ramfs[nfs_ram];
#endif

#if USE_LFS_QSPI==1
const char *lfs_qspi_str[]={"QSPI"};     // edit to reflect your configuration
const int nfs_qspi = sizeof(lfs_qspi_str)/sizeof(const char *);
LittleFS_QSPIFlash qspifs[nfs_qspi];
#endif

#if USE_LFS_PROGM==1
const char *lfs_progm_str[]={"PROGM"};     // edit to reflect your configuration
const int lfs_progm_size[] = {1'000'000}; // edit to reflect your configuration
const int nfs_progm = sizeof(lfs_progm_str)/sizeof(const char *);
LittleFS_Program progmfs[nfs_progm];
#endif

#if USE_LFS_SPI==1
const char *lfs_spi_str[]={"sflash5", "sflash6"}; // edit to reflect your configuration
const int lfs_cs[] = {5, 6}; // edit to reflect your configuration
const int nfs_spi = sizeof(lfs_spi_str)/sizeof(const char *);
LittleFS_SPIFlash spifs[nfs_spi];
#endif

#if USE_LFS_NAND == 1
const char *nspi_str[]={"WINBOND1G", "WINBOND2G"};     // edit to reflect your configuration
const int nspi_cs[] = {3, 4}; // edit to reflect your configuration
const int nspi_nsd = sizeof(nspi_cs)/sizeof(int);
LittleFS_SPINAND nspifs[nspi_nsd]; // needs to be declared if LittleFS is used in storage.h
#endif



void storage_configure()
{
  DateTimeFields date;
  breakTime(Teensy3Clock.get(), date);
  const char *monthname[12]={
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  DBGSerial.printf("Date: %u %s %u %u:%u:%u\n",
    date.mday, monthname[date.mon], date.year+1900, date.hour, date.min, date.sec);

#if 1
  // lets initialize a RAM drive. 
  if (lfsram.begin(LFSRAM_SIZE)) {
    DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    lfsmtpcb.set_formatLevel(true);  //sets formating to lowLevelFormat
    uint32_t istore = storage.addFilesystem(lfsram, "RAM", &lfsmtpcb, (uint32_t)(LittleFS*)&lfsram);
    if (istore != 0xFFFFFFFFUL) storage.setIndexStore(istore);
    DBGSerial.printf("Set Storage Index drive to %u\n", istore);
  }
#else
   storage.setIndexStore(0);
   DBGSerial.println("Set Storage Index to default drive 0");
#endif
    
#if USE_SD == 1
  // Try to add all of them. 
  bool storage_added = false;
  for (uint8_t i = 0 ; i < COUNT_MYFS; i++) {
    storage_added |= mySD[i].init(true);
  }
  if (!storage_added) {
    DBGSerial.println("Failed to add any valid storage objects");
    pinMode(13, OUTPUT);
    while (1) {
      digitalToggleFast(13);
      delay(250);
    }
  }
  
  DBGSerial.println("SD initialized.");
#endif


#if USE_LFS_RAM==1
  for (int ii=0; ii<nfs_ram;ii++) {
    if (!ramfs[ii].begin(lfs_ram_size[ii])) {
      DBGSerial.printf("Ram Storage %d %s failed or missing",ii,lfs_ram_str[ii]);
      DBGSerial.println();
    } else {
      storage.addFilesystem(ramfs[ii], lfs_ram_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&ramfs[ii]);
      uint64_t totalSize = ramfs[ii].totalSize();
      uint64_t usedSize  = ramfs[ii].usedSize();
      DBGSerial.printf("RAM Storage %d %s %llu %llu\n", ii, lfs_ram_str[ii],
        totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_PROGM==1
  for (int ii=0; ii<nfs_progm;ii++) {
    if (!progmfs[ii].begin(lfs_progm_size[ii])) {
      DBGSerial.printf("Program Storage %d %s failed or missing",ii,lfs_progm_str[ii]);
      DBGSerial.println();
    } else {
      storage.addFilesystem(progmfs[ii], lfs_progm_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&progmfs[ii]);
      uint64_t totalSize = progmfs[ii].totalSize();
      uint64_t usedSize  = progmfs[ii].usedSize();
      DBGSerial.printf("Program Storage %d %s %llu %llu\n", ii, lfs_progm_str[ii],
        totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_QSPI==1
  for(int ii=0; ii<nfs_qspi;ii++) {
    if(!qspifs[ii].begin()) {
      DBGSerial.printf("QSPI Storage %d %s failed or missing",ii,lfs_qspi_str[ii]);
      DBGSerial.println();
    } else {
      storage.addFilesystem(qspifs[ii], lfs_qspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&qspifs[ii]);
      uint64_t totalSize = qspifs[ii].totalSize();
      uint64_t usedSize  = qspifs[ii].usedSize();
      DBGSerial.printf("QSPI Storage %d %s %llu %llu\n", ii, lfs_qspi_str[ii], totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_SPI==1
  for (int ii=0; ii<nfs_spi;ii++) {
    if (USE_SW_PU == 1) {
      pinMode(lfs_cs[ii],OUTPUT);
      digitalWriteFast(lfs_cs[ii],HIGH);
    }
    if (!spifs[ii].begin(lfs_cs[ii], SPI)) {
      DBGSerial.printf("SPIFlash Storage %d %d %s failed or missing",ii,lfs_cs[ii],lfs_spi_str[ii]);      DBGSerial.println();
    } else {
      storage.addFilesystem(spifs[ii], lfs_spi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&spifs[ii]);
      uint64_t totalSize = spifs[ii].totalSize();
      uint64_t usedSize  = spifs[ii].usedSize();
      DBGSerial.printf("SPIFlash Storage %d %d %s %llu %llu\n", ii, lfs_cs[ii], lfs_spi_str[ii],
        totalSize, usedSize);
    }
  }
#endif
#if USE_LFS_NAND == 1
  for(int ii=0; ii<nspi_nsd;ii++) {
    if (USE_SW_PU == 1) {
      pinMode(nspi_cs[ii],OUTPUT);
      digitalWriteFast(nspi_cs[ii],HIGH);
    }
    if(!nspifs[ii].begin(nspi_cs[ii], SPI)) {
      DBGSerial.printf("SPIFlash NAND Storage %d %d %s failed or missing",ii,nspi_cs[ii],nspi_str[ii]);
      DBGSerial.println();
    } else {
      storage.addFilesystem(nspifs[ii], nspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&nspifs[ii]);
      uint64_t totalSize = nspifs[ii].totalSize();
      uint64_t usedSize  = nspifs[ii].usedSize();
      DBGSerial.printf("Storage %d %d %s %llu %llu\n", ii, nspi_cs[ii], nspi_str[ii],
        totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_QSPI_NAND == 1
  for(int ii=0; ii<qnspi_nsd;ii++) {
    if(!qnspifs[ii].begin()) {
       DBGSerial.printf("QSPI NAND Storage %d %s failed or missing",ii,qnspi_str[ii]); DBGSerial.println();
    } else {
      storage.addFilesystem(qnspifs[ii], qnspi_str[ii], &lfsmtpcb, (uint32_t)(LittleFS*)&qnspi_str[ii]);
      uint64_t totalSize = qnspifs[ii].totalSize();
      uint64_t usedSize  = qnspifs[ii].usedSize();
      DBGSerial.printf("Storage %d %s %llu %llu\n", ii, qnspi_str[ii], totalSize, usedSize);
    }
  }
#endif

// Start USBHost_t36, HUB(s) and USB devices.
#if USE_MSC > 0
  DBGSerial.println("\nInitializing USB MSC drives...");
  usbmsc.checkUSBStatus(true);
#endif

}

void setup()
{
#if USE_MSC_FAT > 0
  // let msusb stuff startup as soon as possible
  usbmsc.begin();
#endif 
  
  // Open serial communications and wait for port to open:
#if defined(USB_MTPDISK_SERIAL)
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }
#else
  //while(!DBGSerial.available()); // comment if you do not want to wait for
                                   // terminal (otherwise press any key to continue)
  while (!Serial && !DBGSerial.available() && millis() < 5000) 
  //myusb.Task(); // or third option to wait up to 5 seconds and then continue
#endif

  DBGSerial.print(CrashReport);
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);
  
  //This is mandatory to begin the mtpd session.
  mtpd.begin();
  
  storage_configure();
  
  DBGSerial.println("\nSetup done");
  
}

int ReadAndEchoSerialChar() {
  int ch = DBGSerial.read();
  if (ch >= ' ') DBGSerial.write(ch);
  return ch;
}

void loop()
{
  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    uint8_t storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = DBGSerial.read();

    uint32_t fsCount;
    switch (command) {
    case '1':
      // first dump list of storages:
      fsCount = storage.getFSCount();
      DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        DBGSerial.printf("store:%u storage:%x name:%s fs:%x\n", ii, mtpd.Store2Storage(ii),
          storage.getStoreName(ii), (uint32_t)storage.getStoreFS(ii));
      }
      DBGSerial.println("\nDump Index List");
      storage.dumpIndexList();
      break;
    case '2':
      if (storage_index < storage.getFSCount()) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index,
          storage.getStoreName(storage_index));
        myfs = storage.getStoreFS(storage_index);
        current_store = storage_index;
      } else {
        DBGSerial.printf("Storage Index %u out of range\n", storage_index);
      }
      break;

    case 'l': listFiles(); break;
    case 'e': eraseFiles(); break;
    case 's':
      DBGSerial.println("\nLogging Data!!!");
      write_data = true;   // sets flag to continue to write data until new command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append data to
      // to the file created.
      dataFile = myfs->open("datalog.txt", FILE_WRITE);
      logData();
      break;
    case 'x': stopLogging(); break;
    case'r':
      DBGSerial.println("Reset");
      mtpd.send_DeviceResetEvent();
      break;
    case 'd': dumpLog(); break;
    case '\r':
    case '\n':
    case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
  } else {
    mtpd.loop();
    #if USE_MSC > 0
    usbmsc.checkUSBStatus(false);
    #endif
  }

  if (write_data) logData();
}

void logData()
{
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

void stopLogging()
{
  DBGSerial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  DBGSerial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}


void dumpLog()
{
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

void menu()
{
  DBGSerial.println();
  DBGSerial.println("Menu Options:");
  DBGSerial.println("\t1 - List Drives (Step 1)");
  DBGSerial.println("\t2 - Select Drive for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\te - Erase files on disk");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tr - Reset MTP");
  DBGSerial.println("\th - Menu");
  DBGSerial.println();
}

void listFiles()
{
  DBGSerial.print("\n Space Used = ");
  DBGSerial.println(myfs->usedSize());
  DBGSerial.print("Filesystem Size = ");
  DBGSerial.println(myfs->totalSize());

  printDirectory(myfs);
}
#if USE_MSC > 0
extern PFsLib pfsLIB;
#endif
void eraseFiles()
{

#if 1
  // Lets try asking storage for enough stuff to call it's format code
  bool send_device_reset = false;
  MTPStorageInterfaceCB *callback = storage.getCallback(current_store);
  uint32_t user_token = storage.getUserToken(current_store);
  if (callback) {
    send_device_reset = (callback->formatStore(&storage, current_store, user_token, 
          0, true) == MTPStorageInterfaceCB::FORMAT_SUCCESSFUL);
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
    if (! entry) {
      //DBGSerial.println("** no more files **");
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
  while (ch == ' ') ch = DBGSerial.read();
  if ((ch < '0') || (ch > '9')) return default_num;

  uint32_t return_value = 0;
  while ((ch >= '0') && (ch <= '9')) {
    return_value = return_value * 10 + ch - '0';
    ch = DBGSerial.read();
  }
  return return_value;
}
