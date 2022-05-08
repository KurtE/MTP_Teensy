#include <SD.h>
#include <MTP_Teensy.h>
#include <USBHost_t36.h>
#include <msFilesystem.h>
#include <msDevice.h>

// Add USBHost objectsUsbFs
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub(myusb);

// MSC objects.
msController drive1(myusb);
msController drive2(myusb);
msController drive3(myusb);

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

//Add SD Card
#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD BUILTIN_SDCARD
#else
#define CS_SD 10
#endif

#define COUNT_MYFS 2 // could do by count, but can limit how many are created...
typedef struct {
  uint8_t csPin;
  const char *name;
  SDClass sd;

} SDList_t;
SDList_t myFS[] = {
  {BUILTIN_SDCARD, "SDIO"},
  {10, "SPI10"}
};

// Experiment add memory FS to mainly hold the storage index
// May want to wrap this all up as well
#include <LittleFS.h>
#define LFSRAM_SIZE 65536 // probably more than enough...
LittleFS_RAM lfsram;
LittleFS_SPIFlash flash5;
#define chipSelect 5  // use for access flash on audio or prop shield

//==================================================
FS *myfs = &lfsram;

File myFile, dataFile;
int iRecord = 0;
int line = 0;

#define BUFFER_SIZE_INDEX 128
uint8_t write_buffer[BUFFER_SIZE_INDEX];
#define buffer_mult 4
uint8_t buffer_temp[buffer_mult*BUFFER_SIZE_INDEX];

int bytesRead;
uint32_t drive_index = 0;

// These can likely be left unchanged
#define MYBLKSIZE 2048 // 2048
#define SLACK_SPACE  40960 // allow for overhead slack space :: WORKS on FLASH {some need more with small alloc units}
#define size_bigfile 1024*1024*24  //24 mb file

// Various Globals
const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0, warnLFS = 0; // Track warnings or Errors
uint32_t lCnt = 0, LoopCnt = 0; // loop counters
uint64_t rdCnt = 0, wrCnt = 0; // Track Bytes Read and Written
int filecount = 0;
int loopLimit = 0; // -1 continuous, otherwise # to count down to 0
bool bWriteVerify = true;  // Verify on Write Toggle
File file3; // Single FILE used for all functions

bool storage_added = false;

void setup()
{
  while (!Serial && millis() < 5000);
  Serial.begin(9600);

  if (CrashReport) {
    Serial.print(CrashReport);
  }
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
  delay(3000);

  storage_configure();

  myusb.begin();
  Serial.print("Initializing MSC Drives ...");
  Serial.println("\nInitializing USB MSC drives...");
  Serial.println("MSC and MTP initialized.");
  checkMSCChanges();

  if (!storage_added) {
    Serial.println("Failed to add any valid storage objects");
    pinMode(13, OUTPUT);
    while (1) {
      digitalToggleFast(13);
      delay(250);
    }
  }

  menu();
  
}

uint8_t current_store = 0;
uint8_t storage_index = '0';
void loop() {
  checkMSCChanges();
  MTP.loop();
  
  if (Serial.available()) {
    uint8_t command = Serial.read();
    int ch = Serial.read();
    if ('2'==command) storage_index = CommandLineReadNextNumber(ch, 0);
    while (ch == ' ') ch = Serial.read();

    uint32_t fsCount;
    switch (command) {
    case '1': {
      // first dump list of storages:
      uint32_t fsCount = MTP.getFilesystemCount();
      Serial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                         MTP.Store2Storage(ii), MTP.getFilesystemNameByIndex(ii),
                         (uint32_t)MTP.getFilesystemByIndex(ii));
      }
    }
    //Serial.println("\nDump Index List");
    //MTP.storage()->dumpIndexList();
    break;
    case '2':
      if (storage_index < MTP.getFilesystemCount()) {
        Serial.printf("Storage Index %u Name: %s Selected\n", storage_index,
        MTP.getFilesystemNameByIndex(storage_index));
        myfs = MTP.getFilesystemByIndex(storage_index);
        current_store = storage_index;
      } else {
        Serial.printf("Storage Index %u out of range\n", storage_index);
      }
      break;
    case 'l':
      listFiles();
      break;    
    case 'b':
      bigFile( 0 ); // delete
      command = 0;
      break;
    case 'B':
      bigFile( 1 ); // CREATE
      command = 0;
      break;
    case 's':
      bigFile2MB( 0 ); // CREATE
      command = 0;
      break;
    case 'S':
      bigFile2MB( 1 ); // CREATE
      command = 0;
     break;
    case 'n': // No Verify on write
      bWriteVerify = !bWriteVerify;
      bWriteVerify ? Serial.print(" Write Verify on: ") : Serial.print(" Write Verify off: ");
     command = 0;
      break;
    case 'i':
	    writeIndexFile();
	    break;
    case 'r':
      Serial.println("Reset");
      MTP.send_DeviceResetEvent();
      break;
    case 'R':
      Serial.print(" RESTART Teensy ...");
      delay(100);
      SCB_AIRCR = 0x05FA0004;
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

void storage_configure() {
  for (uint8_t i = 0; i < COUNT_MYFS; i++) {
    if (myFS[i].sd.begin(myFS[i].csPin)) {
      storage_added = true;
      MTP.addFilesystem(myFS[i].sd, myFS[i].name);
    }
  }


  // lets initialize a RAM drive.
  if (lfsram.begin(LFSRAM_SIZE)) {
    Serial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "Prog");
    if (istore != 0xFFFFFFFFUL)
      MTP.useFileSystemIndexFileStore(istore);
    Serial.printf("Set Storage Index drive to %u\n", istore);
    storage_added = true;
  }

  if (!flash5.begin(chipSelect, SPI)) {
    Serial.printf("Error starting %s\n", "SPI FLASH");
    //while (1) {
      // Error, so don't do anything more - stay stuck here
    //}
    if(!storage_added) storage_added = false;
  } else {
      MTP.addFilesystem(flash5, "sFlash6");
      Serial.println("LittleFS initialized.");
      storage_added = true;
  }
 

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
        storage_added = true;
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
  Serial.printf("\n%s","1 - List USB Drives (Step 1)");
  Serial.printf("\n%s","2 - Select USB Drive for Logging (Step 2)");
  Serial.printf("\n%s","l - List files on disk");
  Serial.printf("\n%s","r - Reset MTP");
  Serial.printf("\n%s","R - Restart Teensy");
  Serial.printf("\n%s","i - Write Index File to disk");
  Serial.printf("\n%s","'B, or b': Make Big file half of free space, or remove all Big files");
  Serial.printf("\n%s","'S, or s': Make 2MB file , or remove all 2MB files");
  Serial.printf("\n%s","'n' No verify on Write- TOGGLE");
  Serial.printf("\n%s","'h' - Menu");
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

void readVerify( char szPath[], char chNow ) {
  uint32_t timeMe = micros();
  file3 = myfs->open(szPath);
  if ( 0 == file3 ) {
    Serial.printf( "\tV\t Fail File open %s\n", szPath );
    errsLFS++;
  }
  char mm;
  char chNow2 = chNow + lowOffset;
  uint32_t ii = 0;
  while ( file3.available() ) {
    file3.read( &mm , 1 );
    rdCnt++;
    //Serial.print( mm ); // show chars as read
    ii++;
    if ( 0 == (ii / lowShift) % 2 ) {
      if ( chNow2 != mm ) {
        Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow2, mm, mm, ii );
        errsLFS++;
        break;
      }
    }
    else {
      if ( chNow != mm ) {
        Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
        errsLFS++;
        break;
      }
    }
  }
  Serial.printf( "  Verify %u Bytes ", ii );
  if (ii != file3.size()) {
    Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu", ii, file3.size() );
    errsLFS++;
  }
  file3.close();
  timeMe = micros() - timeMe;
  Serial.printf( " @KB/sec %5.2f", ii / (timeMe / 1000.0) );
}

bool bigVerify( char szPath[], char chNow ) {
  uint32_t timeMe = micros();
  file3 = myfs->open(szPath);
  uint64_t fSize;
  if ( 0 == file3 ) {
    return false;
  }
  char mm;
  uint32_t ii = 0;
  uint32_t kk = file3.size() / 50;
  fSize = file3.size();
  Serial.printf( "\tVerify %s bytes %lu : ", szPath, fSize );
  while ( file3.available() ) {
    file3.read( &mm , 1 );
    rdCnt++;
    ii++;
    if ( !(ii % kk) ) Serial.print('.');
    if ( chNow != mm ) {
      Serial.printf( "<Bad Byte!  %c! = %c [0x%X] @%u\n", chNow, mm, mm, ii );
      errsLFS++;
      break;
    }
    if ( ii > fSize ) { // catch over length return makes bad loop !!!
      Serial.printf( "\n\tFile LEN Corrupt!  FS returning over %u bytes\n", fSize );
      errsLFS++;
      break;
    }
  }
  if (ii != file3.size()) {
    Serial.printf( "\n\tRead Count fail! :: read %u != f.size %llu\n", ii, file3.size() );
    errsLFS++;
  }
  else
    Serial.printf( "\tGOOD! >>  bytes %lu", ii );
  file3.close();
  timeMe = micros() - timeMe;
  Serial.printf( "\n\tBig read&compare KBytes per second %5.2f \n", ii / (timeMe / 1000.0) );
  if ( 0 == ii ) return false;
  return true;
}



void bigFile( int doThis ) {
  char myFile[] = "/0_bigfile.txt";
  char fileID = '0' - 1;
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};

  if ( 0 == doThis ) {  // delete File
    Serial.printf( "\nDelete with read verify all #bigfile's\n");
    do {
      fileID++;
      myFile[1] = fileID;
      if ( myfs->exists(myFile) && bigVerify( myFile, fileID) ) {
        filecount--;
        myfs->remove(myFile);
      }
      else break; // no more of these
    } while ( 1 );
  }
  else {  // FILL DISK
    uint32_t resW = 1;
    
    char someData[MYBLKSIZE];
    uint32_t xx, toWrite;
    toWrite = (myfs->totalSize()) - myfs->usedSize();
    if ( toWrite < 65535 ) {
      Serial.print( "Disk too full! DO :: reformat");
      return;
    }
    toWrite = size_bigfile;
    toWrite -= SLACK_SPACE;
    xx = toWrite;
    Serial.printf( "\nStart Big write of %u Bytes", xx);
    uint32_t timeMe = micros();
    file3 = nullptr;
    do {
      if ( file3 ) file3.close();
      fileID++;
      myFile[1] = fileID;
      file3 = myfs->open(myFile, FILE_WRITE);
    } while ( fileID < '9' && file3.size() > 0);
    if ( fileID == '9' ) {
      Serial.print( "Disk has 9 halves 0-8! DO :: b or q or F");
      return;
    }
    memset( someData, fileID, 2048 );
    int hh = 0;
    while ( toWrite > 2048 && resW > 0 ) {
      resW = file3.write( someData , 2048 );
      hh++;
      if ( !(hh % 40) ) Serial.print('.');
      toWrite -= 2048;
    }
    xx = toWrite;
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    timeMe = micros() - timeMe;
    file3 = myfs->open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
    Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs->usedSize(), myfs->totalSize());
    if ( myfs->usedSize() == myfs->totalSize() ) {
      Serial.printf("\n\n\tWARNING: DISK FULL >>>>>  Bytes Used: %llu, Bytes Total:%llu\n\n", myfs->usedSize(), myfs->totalSize());
      warnLFS++;
    }
    if ( resW < 0 ) {
      Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
      errsLFS++;
      myfs->remove(myFile);
    }
  }
}

void bigFile2MB( int doThis ) {
  char myFile[] = "/0_2MBfile.txt";
  char fileID = '0' - 1;
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};

  if ( 0 == doThis ) {  // delete File
    Serial.printf( "\nDelete with read verify all #bigfile's\n");
    do {
      fileID++;
      myFile[1] = fileID;
      if ( myfs->exists(myFile) && bigVerify( myFile, fileID) ) {
        filecount--;
        myfs->remove(myFile);
      }
      else break; // no more of these
    } while ( 1 );
  }
  else {  // FILL DISK
    uint32_t resW = 1;
    
    char someData[2048];
    uint32_t xx, toWrite;
    toWrite = 2048 * 1000;
    if ( toWrite > (65535 + (myfs->totalSize() - myfs->usedSize()) ) ) {
      Serial.print( "Disk too full! DO :: q or F");
      return;
    }
    xx = toWrite;
    Serial.printf( "\nStart Big write of %u Bytes", xx);
    uint32_t timeMe = micros();
    file3 = nullptr;
    do {
      if ( file3 ) file3.close();
      fileID++;
      myFile[1] = fileID;
      file3 = myfs->open(myFile, FILE_WRITE);
    } while ( fileID < '9' && file3.size() > 0);
    if ( fileID == '9' ) {
      Serial.print( "Disk has 9 files 0-8! DO :: b or q or F");
      return;
    }
    memset( someData, fileID, 2048 );
    int hh = 0;
    while ( toWrite >= 2048 && resW > 0 ) {
      resW = file3.write( someData , 2048 );
      hh++;
      if ( !(hh % 40) ) Serial.print('.');
      toWrite -= 2048;
    }
    xx -= toWrite;
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    timeMe = micros() - timeMe;
    file3 = myfs->open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %lu Bytes : file3.size()=%llu", myFile , timeMe / 1000000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1000.0) );
    Serial.printf("\nBytes Used: %llu, Bytes Total:%llu\n", myfs->usedSize(), myfs->totalSize());
    if ( myfs->usedSize() == myfs->totalSize() ) {
      Serial.printf("\n\n\tWARNING: DISK FULL >>>>>  Bytes Used: %llu, Bytes Total:%llu\n\n", myfs->usedSize(), myfs->totalSize());
      warnLFS++;
    }
    if ( resW < 0 ) {
      Serial.printf( "\nBig write ERR# %i 0x%X \n", resW, resW );
      errsLFS++;
      myfs->remove(myFile);
    }
  }
}

void writeIndexFile() 
{
  DateTimeFields dtf = {0, 10, 7, 0, 22, 7, 121};
  // open the file.
  Serial.println("Write Large Index File");
  file3 = myfs->open("LargeIndexedTestfile.txt", FILE_WRITE_BEGIN);
  if (file3) {
    file3.truncate(); // Make sure we wipe out whatever was written earlier
    for (uint32_t i = 0; i < 43000*4; i++) {
      memset(write_buffer, 'A'+ (i & 0xf), sizeof(write_buffer));
      file3.printf("%06u ", i >> 2);  // 4 per physical buffer
      file3.write(write_buffer, i? 120 : 120-12); // first buffer has other data...
      file3.printf("\n");
    }
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    Serial.println("\ndone.");
  }
}
