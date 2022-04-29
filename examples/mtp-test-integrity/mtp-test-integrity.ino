#include <MTP_Teensy.h>
#include <LittleFS.h>
#include <Entropy.h>

#include "constants.h"
//---------------------------------------------------
// Select drives you want to create
//---------------------------------------------------
#define USE_SD  1         // SDFAT based SDIO and SPI
#ifdef ARDUINO_TEENSY41
#define USE_LFS_RAM 1     // T4.1 PSRAM (or RAM)
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
#define USE_LFS_QSPI 0    // T4.1 QSPI
#define USE_LFS_PROGM 1   // T4.4 Progam Flash
#define USE_LFS_SPI 0     // SPI Flash
#define USE_LFS_NAND 0
#define USE_LFS_QSPI_NAND 0
#define USE_LFS_FRAM 0
#endif
#define USE_MSC 1    // set to > 0 experiment with  (USBHost.t36 + mscFS)
#define USE_SW_PU  1 //set to 1 if SPI devices do not have PUs,
                     // https://www.pjrc.com/better-spi-bus-design-in-3-steps/

#define memBoard  0  //SPI: 1 original(2NAND+2FLASH), 2 - adesto to Q256JW (4FLASH), 3 - Q256+Q01(2FLASH)
// if both useProIdx and useExMem = 0 defaults to using SD Card.
#define useProIdx 1 // use program memory for index file
#define useExMem  0 // use PSRAM for index file

//Used a store of index file
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;
LittleFS_Program lfsProg; // Used to create FS on the Flash memory of the chip
static const uint32_t file_system_size = 1024 * 1024 * 1;

#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif
//-------------------------------------------
FS *myfs = &lfsram;

//=============================================================================
// MSC & SD classes
//=============================================================================
#if USE_SD==1
#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD  BUILTIN_SDCARD
#else
#define CS_SD 10
#endif
#endif


// SDClasses 
#if USE_SD==1
#include <SD.h>
  // edit SPI to reflect your configuration (following is for T4.1)
  #define SD_MOSI 11
  #define SD_MISO 12
  #define SD_SCK  13

  #define SPI_SPEED SD_SCK_MHZ(25)  // adjust to sd card 
  elapsedMillis elapsed_millis_since_last_sd_check = 0;
  #define TIME_BETWEEN_SD_CHECKS_MS 1000
  bool sdio_previously_present;

  const char *sd_str[]={"sdio", "EXT1"}; // edit to reflect your configuration
  const int cs[] = {BUILTIN_SDCARD, 10}; // edit to reflect your configuration
  const int cdPin[] = {0xff, 0xff};
  const int nsd = sizeof(sd_str)/sizeof(const char *);
  bool sd_media_present_prev[nsd];
  
SDClass sdx[nsd];
#endif

// =======================================================================
// Set up MSC Drive file systems on different storage media
// =======================================================================
#if USE_MSC == 1
#include <USBHost_t36.h>
//#include <mscFS.h>
#include <msFilesystem.h>
#include <msDevice.h>

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

msController *pdrives[] {&drive1, &drive2, &drive3};
#define CNT_DRIVES  (sizeof(pdrives)/sizeof(pdrives[0]))
bool drive_previous_connected[CNT_DRIVES] = {false, false, false};
#endif

// =======================================================================
// Set up LittleFS file systems on different storage media
// =======================================================================

#if USE_LFS_FRAM == 1 || USE_LFS_NAND == 1 || USE_LFS_PROGM == 1 || USE_LFS_QSPI == 1 || USE_LFS_QSPI_NAND == 1 || \
  USE_LFS_RAM == 1 || USE_LFS_SPI == 1
#endif

#if USE_LFS_RAM==1
const char *lfs_ram_str[] = {"RAM0","RAM1"};  // edit to reflect your configuration
const int lfs_ram_size[] = {200000, 2000000}; // edit to reflect your configuration
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
const int lfs_progm_size[] = {4000000}; // edit to reflect your configuration
const int nfs_progm = sizeof(lfs_progm_str)/sizeof(const char *);
LittleFS_Program progmfs[nfs_progm];
#endif

#if USE_LFS_SPI==1
#if memBoard == 1
const char *lfs_spi_str[]={"sflash5", "sflash6"}; // edit to reflect your configuration
const int lfs_cs[] = {5, 6 }; // edit to reflect your configuration
#elif memBoard == 2
const char *lfs_spi_str[]={"Adesto", "Q64", "Q256JWIM", "Q256JWIM"}; // edit to reflect your configuration
const int lfs_cs[] = {3, 4, 5, 7}; // edit to reflect your configuration
#elif memBoard == 3
const char *lfs_spi_str[]={"Q256IQ", "Q01"}; // edit to reflect your configuration
const int lfs_cs[] = {3, 4}; // edit to reflect your configuration
#endif
const int nfs_spi = sizeof(lfs_spi_str)/sizeof(const char *);
LittleFS_SPIFlash spifs[nfs_spi];
#endif

#if USE_LFS_NAND == 1
const char *nspi_str[]={"WINBOND1G", "WINBOND2G"};     // edit to reflect your configuration
const int nspi_cs[] = {3, 4}; // edit to reflect your configuration
const int nspi_nsd = sizeof(nspi_cs)/sizeof(int);
LittleFS_SPINAND nspifs[nspi_nsd]; // needs to be declared if LittleFS is used in storage.h
#endif

#if USE_LFS_QSPI_NAND == 1
const char *qnspi_str[]={"WIN-M02"};     // edit to reflect your configuration
const int qnspi_nsd = sizeof(qnspi_str)/sizeof(const char *);
LittleFS_QPINAND qnspifs[qnspi_nsd]; // needs to be declared if LittleFS is used in storage.h
#endif

#if USE_LFS_FRAM == 1
const char *qfspi_str[]={"Fram1", "Fram2", "Fram3"};     // edit to reflect your configuration
const int qfspi_cs[] = {3,4,5}; // edit to reflect your configuration
const int qfspi_nsd = sizeof(qfspi_cs)/sizeof(int);
LittleFS_SPIFram qfspifs[qfspi_nsd]; // needs to be declared if LittleFS is used in storage.h
#endif


void setup()
{
    storage_configure();
    
  while (!Serial && !DBGSerial.available() && millis() < 5000) 

  DBGSerial.print(CrashReport);
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);  

  //This is mandatory to begin the d session.
  //MTP.begin();
  //delay(2000);


  #if USE_MSC == 1
  myusb.begin();
  //DBGSerial.print("Initializing MSC Drives ...");
  //DBGSerial.println("\nInitializing USB MSC drives...");
  //DBGSerial.println("MSC and  initialized.");
  checkMSCChanges();
  #endif

  //Serial.printf("USBPHY1_TX = %08X\n", USBPHY1_TX);
  //USBPHY1_TX = 0x10090901;
  //Serial.printf("USBPHY1_TX = %08X\n", USBPHY1_TX);
  //DBGSerial.println("\nSetup done");

  menu();
}

uint8_t storage_index = '0';
void loop()
{
  if ( DBGSerial.available() ) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
    if ('2'==command) storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = DBGSerial.read();

    uint32_t fsCount;
    switch (command) {
    case '1':
      // first dump list of storages:
      fsCount = MTP.getFilesystemCount();
      DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        DBGSerial.printf("store:%u storage:%x name:%s fs:%x pn:", ii,
                         MTP.Store2Storage(ii), MTP.getFilesystemNameByIndex(ii),
                         (uint32_t)MTP.getFilesystemByIndex(ii));
        Serial.flush();        
        DBGSerial.println(getFSPN(ii));
      }
      //DBGSerial.println("\nDump Index List");
      //MTP.storage()->dumpIndexList();
      break;
    case '2':
      if (storage_index < MTP.getFilesystemCount()) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index,
        MTP.getFilesystemNameByIndex(storage_index));
        myfs = MTP.getFilesystemByIndex(storage_index);
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
    case 'f': 
      format3();
      break;
    case 'x': stopLogging(); break;
    case'r':
      DBGSerial.println("Reset");
      MTP.send_DeviceResetEvent();
      break;
    case 'd': dumpLog(); break;
    case 'b':
      bigFile( 0 ); // delete
      command = 0;
      break;
    case 'B':
      bigFile( 1 ); // CREATE
      command = 0;
      break;
    case 't':
      bigFile2MB( 0 ); // CREATE
      command = 0;
      break;
    case 'S':
      bigFile2MB( 1 ); // CREATE
      command = 0;
     break;
    case 'n': // No Verify on write
      bWriteVerify = !bWriteVerify;
      bWriteVerify ? DBGSerial.print(" Write Verify on: ") : DBGSerial.print(" Write Verify off: ");
     command = 0;
      break;
    case 'i':
      writeIndexFile();
      break;
    case 'R':
      DBGSerial.print(" RESTART Teensy ...");
      delay(100);
      SCB_AIRCR = 0x05FA0004;
      break;
    case 'F':
      Entropy.Initialize();
      randomSeed(Entropy.random());
      benchmark();
      break;    
    case '\r':
    case '\n':
    case 'h': menu(); break;
    }
    while (DBGSerial.read() != -1) ; // remove rest of characters.
  } else {
    #if USE_MSC == 1
    checkMSCChanges();
    #endif
    #if USE_SD == 1
    checkSDChanges();
    #endif
    MTP.loop();
  }

  if (write_data) logData();
}