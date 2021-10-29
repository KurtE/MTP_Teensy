/*
  SF  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include "SD.h"
#include <MTP_Teensy.h>

#define SPI_SPEED SD_SCK_MHZ(25) // adjust to sd card

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

uint8_t current_store = 0;

// Add in MTPD objects
MTPStorage storage;
MTPD mtpd(&storage);

#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY36) || defined(ARDUINO_TEENSY35)
#define USE_BUILTIN_SDCARD
SDClass sdSDIO;
#endif

const int SD_ChipSelect = 10;
SDClass sdSPI;

// Experiment add memory FS to mainly hold the storage index
// May want to wrap this all up as well
//#include "LittleFSCombine.h"
#include <LittleFS.h>
uint32_t LFSRAM_SIZE = 65536; // probably more than enough...
LittleFS_RAM lfsram;


//----------------------------------------------------------------------------
// Experiment with LittleFS_SPI wrapper
//----------------------------------------------------------------------------
#ifdef ARDUINO_TEENSY41
class lfs_qspi {
public:
  lfs_qspi(){}
  bool begin();
  inline FS * fs() { return plfs;}
  inline const char * displayName() {return display_name;}
  // You have full access to internals.
  uint8_t csPin;
  LittleFS_QSPIFlash flash;
  LittleFS_QPINAND nand;
  LittleFS *plfs = nullptr;
  char display_name[10];
};
bool lfs_qspi::begin() {
  //Serial.printf("Try QSPI");
  if (flash.begin()) {
    //Serial.println(" *** Flash ***");
    strcpy(display_name, (const char *)F("QFlash"));
    plfs = &flash;
    return true;
  } else if (nand.begin()) {
    //Serial.println(" *** Nand ***");
    strcpy(display_name, (const char *)F("QNAND"));
    plfs = &nand;
    return true;
  }
  //Serial.println(" ### Failed ###");
  return false;
}
  lfs_qspi lfsq;
#endif

//----------------------------------------------------------------------------
// Experiment with LittleFS_SPI wrapper
//----------------------------------------------------------------------------
class lfs_spi {
public:
  lfs_spi(uint8_t pin) : csPin(pin) {}
  bool begin();
  inline FS * fs() { return plfs;}
  inline const char * displayName() {return display_name;}
  // You have full access to internals.
  uint8_t csPin;
  LittleFS_SPIFlash flash;
  LittleFS_SPIFram fram;
  LittleFS_SPINAND nand;
  LittleFS *plfs = nullptr;
  char display_name[10];
};
bool lfs_spi::begin() {
  //Serial.printf("Try SPI Pin %u", csPin);
  if (flash.begin(csPin)) {
    //Serial.println(" *** Flash ***");
    sprintf(display_name, (const char *)F("Flash_%u"), csPin);
    plfs = &flash;
    return true;
  } else if (fram.begin(csPin)) {
    //Serial.println(" *** Fram ***");
    sprintf(display_name, (const char *)F("Fram_%u"), csPin);
    plfs = &fram;
    return true;
  } else if (nand.begin(csPin)) {
    //Serial.println(" *** Nand ***");
    sprintf(display_name, (const char *)F("NAND_%u"), csPin);
    plfs = &nand;
    return true;
  }
  //Serial.println(" ### Failed ###");
  return false;
}

  lfs_spi lfs_spi_list[] = {{3}, {4}, {5}, {6}};

  FS *myfs = &lfsram; // current default FS...

  static const uint32_t file_system_size = 1024 * 512;

#define DBGSerial Serial
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
    // start up MTPD early which will if asked tell the MTP
    // host that we are busy, until we have finished setting
    // up...
    DBGSerial.begin(2000000);
    mtpd.PrintStream(&rstream); // Setup which stream to use...

    mtpd.begin();

    // Open serial communications and wait for port to open:
    while (!DBGSerial && millis() < 5000) {
      // wait for serial port to connect.
    }

    // set to real stream
    mtpd.PrintStream(&DBGSerial); // Setup which stream to use...
    int ch;
    while ((ch = rstream.read()) != -1)
      DBGSerial.write(ch);

    if (CrashReport) {
      DBGSerial.print(CrashReport);
    }
    DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

    DBGSerial.printf("%u Initializing MTP Storage list ...", millis());

    DateTimeFields date;
    breakTime(Teensy3Clock.get(), date);
    const char *monthname[12] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    DBGSerial.printf("Date: %u %s %u %u:%u:%u\n",
                     date.mday, monthname[date.mon], date.year + 1900, date.hour, date.min, date.sec);

// lets initialize a RAM drive.
#if defined ARDUINO_TEENSY41
    if (external_psram_size)
      LFSRAM_SIZE = 4 * 1024 * 1024;
#endif
    if (lfsram.begin(LFSRAM_SIZE)) {
      DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
      uint32_t istore = storage.addFilesystem(lfsram, "RAM");
//    if (istore != 0xFFFFFFFFUL)
//      storage.setIndexStore(istore);
      DBGSerial.printf("Set Storage Index drive to %u\n", istore);
    }

#ifdef ARDUINO_TEENSY41
    if (lfsq.begin()) {
       storage.addFilesystem(*lfsq.fs(), lfsq.displayName());
    }
#endif

    // Now lets try our list of LittleFS SPI?
    for (uint8_t i = 0; i < (sizeof(lfs_spi_list) / sizeof(lfs_spi_list[0])); i++) {
      if (lfs_spi_list[i].begin()) {
        storage.addFilesystem(*lfs_spi_list[i].fs(), lfs_spi_list[i].displayName());
      }
    }

    SDClass sdSPI;
#if defined(USE_BUILTIN_SDCARD)
    if (sdSDIO.begin(BUILTIN_SDCARD)) {
      storage.addFilesystem(sdSDIO, "SD_Builtin");
    }
#endif

    if (sdSPI.begin(SD_ChipSelect)) {
      storage.addFilesystem(sdSPI, "SD_SPI");
    }

    DBGSerial.printf("%u Storage list initialized.\n", millis());
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
        DBGSerial.println("\nDump Index List");
        storage.dumpIndexList();
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

  void eraseFiles() {

  }

  void printDirectory(FS * pfs) {
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
