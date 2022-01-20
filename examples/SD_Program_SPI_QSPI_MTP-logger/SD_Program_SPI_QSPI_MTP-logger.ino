/*
  SF  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/

#define PLAY_WAVE_FILES

#include "SD.h"
#include <MTP_Teensy.h>

#define SPI_SPEED SD_SCK_MHZ(25) // adjust to sd card

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

uint8_t current_store = 0;


#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY36) || defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY_MICROMOD) || defined(ARDUINO_TEENSY40)
#define USE_BUILTIN_SDCARD
SDClass sdSDIO;
bool sdio_previously_present;
#endif

#define ENABLE_SPI_SD_MEDIA_PRESENT
const int SD_ChipSelect = 10;
SDClass sdSPI;
elapsedMillis elapsed_millis_since_last_sd_check = 0;
bool auto_sd_mediaPresent = false;
bool sdspi_previously_present;
#define TIME_BETWEEN_SD_CHECKS_MS 1000

#include <LittleFS.h>
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;

LittleFS_Program lfsProg; // Used to create FS on the Flash memory of the chip

#ifdef ARDUINO_TEENSY41
LittleFS_QSPI lfsqspi;
#endif

// Experiment with LittleFS_SPI wrapper
LittleFS_SPI lfsspi[] = {{3}, {4}, {5}, {6}, {7}};
#define CLFSSPIPINS (sizeof(lfsspi) / sizeof(lfsspi[0]))

FS *myfs = &lfsProg; // current default FS...
int myfs_index = 0;
int index_sdio_storage = -1;
int index_sdspi_storage = -1;

static const uint32_t file_system_size = 1024 * 512;

#define DBGSerial Serial
#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif

#ifdef PLAY_WAVE_FILES
#include <Audio.h>

// GUItool: begin automatically generated code
AudioPlaySdWav           playWav; //xy=154,422
AudioPlaySdRaw           playRaw; //xy=154,422
AudioOutputI2S           i2s1;           //xy=334,89
AudioConnection          patchCord3(playWav, 0, i2s1, 0);
AudioConnection          patchCord4(playWav, 1, i2s1, 1);
AudioConnection          patchCord7(playRaw, 0, i2s1, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=240,153
// GUItool: end automatically generated code
float volume = 0.7f;
char filename[256] = "2001/stop.wav";
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
    if (Serial) {
      sendBufferToSerial();
      return Serial.write(b);
    }
    if (tail_ < sizeof(buffer_)) {
      buffer_[tail_++] = b;
      return 1;
    }
    return 0;
  }
  virtual size_t write(const uint8_t *buffer, size_t size) {
    if (Serial) {
      sendBufferToSerial();
      return Serial.write(buffer, size);
    }
    size_t free_write = sizeof(buffer_) - tail_;
    if (free_write < size) size = free_write;
    if (size) memcpy(&buffer_[tail_], buffer, size);
    tail_ += size;
    return size;

  }

  inline void sendBufferToSerial() {
    if (tail_) {
      Serial.write(buffer_, tail_);
      tail_ = 0;
    }
  }

  enum { BUFFER_SIZE = 32768 };
  uint8_t buffer_[BUFFER_SIZE];
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
};

void setup() {
  // setup to do quick and dirty ram stream until Serial or like is up...
  RAMStream rstream;
  // start up MTPD early which will if asked tell the MTP
  // host that we are busy, until we have finished setting
  // up...
  MTP.PrintStream(&rstream); // Setup which stream to use...
  rstream.printf("+++ before MTP.begin() %u\n", millis());

  //MTP.begin();

  // set to real stream
  DBGSerial.printf("%u Initializing MTP Storage list ...", millis());

#if defined(__IMXRT1062__)
  // Lets add the Program memory version:
  // checks that the LittFS program has started with the disk size specified
  if (lfsProg.begin(file_system_size)) {
    MTP.addFilesystem(lfsProg, "Program");
  } else {
    Serial.println("Error starting Program Flash storage");
  }
#endif

// lets initialize a RAM drive.
#if defined ARDUINO_TEENSY41
  if (external_psram_size)
    LFSRAM_SIZE = 4 * 1024 * 1024;
#endif
  if (lfsram.begin(LFSRAM_SIZE)) {
    rstream.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "RAM");
    if (istore != 0xFFFFFFFFUL) {
      MTP.storage()->setIndexStore(istore);
      rstream.printf("Set Storage Index drive to %u\n", istore);
    }
  }

#ifdef ARDUINO_TEENSY41
  if (lfsqspi.begin()) {
    MTP.addFilesystem(*lfsqspi.fs(), lfsqspi.displayName());
  } else {
    Serial.println("T4.1 does not have external Flash chip");
  }
#endif

  for (uint8_t i = 0; i < CLFSSPIPINS; i++) {
    if (lfsspi[i].begin()) {
      MTP.addFilesystem(lfsspi[i], lfsspi[i].displayName());
    }
  }

#if defined(USE_BUILTIN_SDCARD)
  // always add
  sdio_previously_present = sdSDIO.begin(BUILTIN_SDCARD);
  index_sdio_storage = MTP.addFilesystem(sdSDIO, "SD_Builtin");
  //MTP.setIndexStore(index_sdio_storage);
  //rstream.printf("Set Storage Index drive to %u\n", index_sdio_storage);
#endif

  #ifdef ENABLE_SPI_SD_MEDIA_PRESENT
  sdspi_previously_present = sdSPI.begin(SD_ChipSelect);
  index_sdspi_storage = MTP.addFilesystem(sdSPI, "SD_SPI");
  Serial.printf("*** SD SPI(%u) added FS: %u %u\n", SD_ChipSelect, sdspi_previously_present, index_sdspi_storage);
  #else
  if (sdSPI.begin(SD_ChipSelect)) {
    index_sdspi_storage = MTP.addFilesystem(sdSPI, "SD_SPI");
  } else {
    rstream.printf("SD_SPI(%d) not added", SD_ChipSelect);
    index_sdspi_storage = -1; 
  }
  #endif
  elapsed_millis_since_last_sd_check = 0;

  // Open serial communications and wait for port to open:
  rstream.printf("+++ before wait on !Serial %u\n", millis());
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }
  //rstream.begin(2000000); // don't call it wastes 2 seconds. 
  rstream.printf("+++ after wait on !Serial %u\n", millis());
  MTP.PrintStream(&DBGSerial); // Setup which stream to use...
  rstream.sendBufferToSerial();

  if (CrashReport) {
    DBGSerial.print(CrashReport);
  }
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  DBGSerial.printf("%u Storage list initialized.\n", millis());

#ifdef PLAY_WAVE_FILES
  //Setup Audio
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  DBGSerial.printf("Setup Audio\n");
  AudioMemory(40);

  sgtl5000_1.enable();
  sgtl5000_1.volume(volume);

#endif


  menu();
}

const char *getFSPN(uint32_t ii) {
  FS* pfs = MTP.storage()->getStoreFS(ii);
  // total set of hacks...
  if (pfs == (FS *)&lfsram) return lfsram.getMediaName();
  if (pfs == (FS *)&lfsProg) return lfsProg.getMediaName();
  #ifdef ARDUINO_TEENSY41
  if (pfs == (FS *)&lfsqspi) return lfsqspi.getMediaName();
  #endif
  for (uint8_t i = 0; i < CLFSSPIPINS; i++) {
    if (pfs == (FS *)&lfsspi[i]) return lfsspi[i].getMediaName();
  }
  return "";
}

void loop() {
  if (DBGSerial.available()) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();

    switch (command) {
    case '1': {
      // first dump list of storages:
      uint32_t fsCount = MTP.storage()->getFSCount();
      DBGSerial.printf("\nDump Storage list(%u)\n", fsCount);
      for (uint32_t ii = 0; ii < fsCount; ii++) {
        DBGSerial.printf("store:%u storage:%x name:%s fs:%x media:", ii,
                         MTP.Store2Storage(ii), MTP.storage()->getStoreName(ii),
                         (uint32_t)MTP.storage()->getStoreFS(ii));
        Serial.flush();  
        const char *pfspn = getFSPN(ii);     
        DBGSerial.printf("%s(%p)\n", pfspn, pfspn);
      }
      DBGSerial.println("\nDump Index List");
      MTP.storage()->dumpIndexList();
    } break;
    case '2': {
      uint8_t storage_index = CommandLineReadNextNumber(ch, 0);
      while (ch == ' ') {
        ch = DBGSerial.read();
      }
      if (storage_index < MTP.storage()->getFSCount()) {
        DBGSerial.printf("Storage Index %u Name: %s Selected\n", storage_index,
                         MTP.storage()->getStoreName(storage_index));
        myfs_index = storage_index;
        myfs = MTP.storage()->getStoreFS(storage_index);
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
    case 'f':
      lowLevelFormat();
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
      MTP.send_DeviceResetEvent();
      break;
    case 'd':
      dumpLog();
      break;
    case 'c': 
      {
        while (ch == ' ') ch = Serial.read();
        if ((ch == 'a') || (ch == 'A')) auto_sd_mediaPresent = true;
        else if ((ch == 'f') || (ch == 'F')) auto_sd_mediaPresent = false;
        else checkSDMediaChanges();
        break;  
      }
#ifdef PLAY_WAVE_FILES
    case 'P':
      playDir(myfs);
      DBGSerial.println("Finished Playlist");
      break;
    case 'p':
    {
      DBGSerial.print("Playing file: ");
      while (ch == ' ') ch = Serial.read();
      if (ch > ' ') {
        char *psz = filename;
        while (ch > ' ') {
          *psz++ = ch;
          ch = Serial.read();
        }
        *psz = '\0';
      }

      DBGSerial.println(filename);
      // Start playing the file.  This sketch continues to
      // run while the file plays.
      playFile(myfs, filename);
      DBGSerial.println("Done.");
      break;
    }
#endif
    case '3':
      MTP.storage()->printClearRecordReadWriteCounts();
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
    MTP.loop();
    if (auto_sd_mediaPresent && (elapsed_millis_since_last_sd_check >= TIME_BETWEEN_SD_CHECKS_MS)) {
      elapsed_millis_since_last_sd_check = 0; 
      checkSDMediaChanges();
    }
  }

  if (write_data)
    logData();
}

void checkSDMediaChanges() {
  #ifdef USE_BUILTIN_SDCARD
  elapsedMicros em = 0;
  bool sdio_present = sdSDIO.mediaPresent();
  DBGSerial.printf("Check SD IO %u %u %u\n", sdio_present, sdio_previously_present, (uint32_t)em);
  if (sdio_present != sdio_previously_present) {
    sdio_previously_present = sdio_present;
    if (sdio_present) DBGSerial.printf("###SD Media inserted(%d)\n", index_sdio_storage);
    else {
      sdSDIO.sdfs.end();
      DBGSerial.printf("###SD Media Removed(%d)\n", index_sdio_storage);
    }
    MTP.send_DeviceResetEvent();
  }
  #endif
  #ifdef ENABLE_SPI_SD_MEDIA_PRESENT
  elapsedMicros emspi = 0;
  bool sdspi_present = sdSPI.mediaPresent();
  DBGSerial.printf("Check SD SPI %u %u %u\n", sdspi_present, sdspi_previously_present, (uint32_t)emspi);
  if (sdspi_present != sdspi_previously_present) {
    sdspi_previously_present = sdspi_present;
    if (sdspi_present) DBGSerial.printf("###SD SPI Media inserted(%d)\n", index_sdspi_storage);
    else {
      DBGSerial.printf("###SD SPI Media Removed(%d)\n", index_sdspi_storage);
      sdSPI.sdfs.end();
    } 
    MTP.send_DeviceResetEvent();
  }
#endif
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
  MTP.send_DeviceResetEvent();
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
  DBGSerial.println("\t2# - Select Drive for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\te - Erase/Format disk");
  DBGSerial.println("\tf - Low Level Format (LittleFS)");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append "
                    "records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tc[A/F] - Check SD media changes A-Auto, F-Auto Off, else now");
  DBGSerial.println("\tr - Reset MTP");
#ifdef PLAY_WAVE_FILES
  DBGSerial.println("\tp[filename] - play audio wave file");
  DBGSerial.println("\tP - Play all wave files");
#endif  
  DBGSerial.println("\t3 - Debug print and clear Storage cache calls");
  DBGSerial.println("\tP - Play all wave files");
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

void eraseFiles() {
  DBGSerial.println("\n*** Erase/Format started ***");
  myfs->format(0, '.', DBGSerial);
  Serial.println("Completed, sending device reset event");
  MTP.send_DeviceResetEvent();
}

void lowLevelFormat() {
  DBGSerial.print("\n*** Low Level Format ");
  if ((myfs_index == index_sdio_storage) || (myfs_index == index_sdspi_storage)) {
    DBGSerial.println("Not supported(yet) on SD drives ***");
    return;
  }
  // see if I get away with simple cast
  Serial.println("started ***");
  myfs->format(1, '.', DBGSerial);  // try first by using the hack of 1...
  Serial.println("Completed, sending device reset event");
  MTP.send_DeviceResetEvent();
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
      // DBGSerial.priBBBntln("** no more files **");
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

#ifdef PLAY_WAVE_FILES
void playFile(FS* pfs, const char *filename)
{

  if (strstr(filename, ".WAV") != NULL || strstr(filename, ".wav") != NULL ) {
    Serial.printf("Playing file: '%s'\n", filename);
    while (Serial.read() != -1) ; // clear out any keyboard data...
    bool audio_began = playWav.play(pfs, filename);
    if(!audio_began) {
      Serial.println("  >>> Wave file failed to play");
      return;
    }
    delay(5);
    while (playWav.isPlaying()) {
      if (Serial.available()){Serial.println("User Abort"); break;}
      delay(250);
    }
    playWav.stop();
    delay(250);
  } else if (strstr(filename, ".RAW") != NULL || strstr(filename, ".raw") != NULL ) {
    Serial.printf("Playing file: '%s'\n", filename);
    while (Serial.read() != -1) ; // clear out any keyboard data...
    bool audio_began = playRaw.play(pfs, filename);
    if(!audio_began) {
      Serial.println("  >>> Wave file failed to play");
      return;
    }
    delay(5);
    while (playRaw.isPlaying()) {
      if (Serial.available()){Serial.println("User Abort"); break;}
      delay(250);
    }
    playRaw.stop();
    delay(250);
  } else {
    Serial.printf("File %s is not a wave file\n", filename);
  }
  while (Serial.read() != -1) ;
}
void playDir(FS *pfs) {
  DBGSerial.println("Playing files on device");
  playAll(pfs, pfs->open("/"));
  DBGSerial.println();
}

void playAll(FS* pfs, File dir){
  char filename[64];
  char filnam[64];
   while(true) {
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       // rewind to begining of directory and start over
       dir.rewindDirectory();
       break;
     }
     //DBGSerial.print(entry.name());
     if (entry.isDirectory()) {
       //DBGSerial.println("Directory/");
       //do nothing for now
       //DBGSerial.println(entry.name());
       playAll(pfs, entry);
     } else {
       // files have sizes, directories do not
       //DBGSerial.print("\t\t");
       //DBGSerial.println(entry.size(), DEC);
       // files have sizes, directories do not
       strcpy(filename, dir.name());
       if(strlen(dir.name()) > 0) strcat(filename, "/");
       strcat(filename, strcpy(filnam, entry.name()));
       playFile(pfs, filename);
     }
   entry.close();
 }
}
#endif