#include <SD.h>
#include <MTP_Teensy.h>

#define CS_SD BUILTIN_SDCARD  // Works on T_3.6 and T_4.1
#define CS_SD2 10

#ifdef CS_SD2
SDClass sdSPI;
#endif

#if defined(__IMXRT1062__)
#include <LittleFS.h>
static const uint32_t file_system_size = 1024 * 512;
LittleFS_Program lfsProg; // Used to create FS on the Flash memory of the chip
#endif

#ifdef ARDUINO_TEENSY41
extern "C" uint8_t external_psram_size;
#endif

class RAMStream : public Stream {
public:
  // overrides for Stream
  virtual int available() { return (tail_ - head_); }
  virtual int read() { return (tail_ != head_) ? buffer_[head_++] : -1; }
  virtual int peek() { return (tail_ != head_) ? buffer_[head_] : -1; }

  // overrides for Print
  virtual size_t write(uint8_t b) {
    if (tail_ < buffer_size) {
      buffer_[tail_++] = b;
      return 1;
    }
    return 0;
  }

  enum { BUFFER_SIZE = 32768 };
//  uint8_t buffer_[BUFFER_SIZE];
  uint8_t *buffer_ = nullptr;
  uint32_t buffer_size = BUFFER_SIZE;
  uint32_t head_ = 0;
  uint32_t tail_ = 0;
};

RAMStream rstream;

//#define CS_SD 10  // Works on SPI with this CS pin
void setup()
{

  // see if external memory
#ifdef ARDUINO_TEENSY41
  if (external_psram_size) {
    rstream.buffer_size = 2097152;
    rstream.buffer_ = (uint8_t*)extmem_malloc(rstream.buffer_size);
    Serial.printf("extmem_malloc %p %u %u\n", rstream.buffer_, rstream.buffer_size, external_psram_size);
  }
#endif
  if (!rstream.buffer_) {
    rstream.buffer_ = (uint8_t*)malloc(rstream.buffer_size);
    Serial.printf("malloc %p %u\n", rstream.buffer_, rstream.buffer_size);
  }


  // mandatory to begin the MTP session.
  //MTP.PrintStream(&rstream); // Setup which stream to use...  MTP.begin();
  MTP.begin();

  Serial.begin(9600);
  while (!Serial && millis() < 5000) {
    // wait for serial port to connect.
  }

  if (CrashReport) Serial.print(CrashReport);

  // Add SD Card
  if (SD.begin(CS_SD)) {
    Serial.println("Added SD card using built in SDIO, or given SPI CS");
  } else {
    Serial.println("No SD Card");
  }
  MTP.addFilesystem(SD, "SD1");
#ifdef CS_SD2
  if (sdSPI.begin(CS_SD2)) {
    Serial.println("Added SD2 card using built in SDIO, or given SPI CS");
  } else {
    Serial.println("SD2 card not present");
  }
  MTP.addFilesystem(sdSPI, "SD2");
#endif

#if defined(__IMXRT1062__)
  // Lets add the Program memory version:
  // checks that the LittFS program has started with the disk size specified
  if (lfsProg.begin(file_system_size)) {
    MTP.addFilesystem(lfsProg, "Program");
  } else {
    Serial.println("Error starting Program Flash storage");
  }
#endif



  //MTP.useFileSystemIndexFileStore(MTPStorage::INDEX_STORE_MEM_FILE);
  Serial.println("\nSetup done");
}


uint8_t print_buffer[256];
void print_capture_data() {

  #ifdef ARDUINO_TEENSY41
  Serial.printf("Capture size: %d out of %d PS:%u\n", rstream.available(), rstream.buffer_size, external_psram_size);
  #else
  Serial.printf("Capture size: %d out of %d\n", rstream.available(), rstream.buffer_size);
  #endif
  
  int avail;
  while ((avail = rstream.available())) {
    if (avail > (int)sizeof(print_buffer)) avail = sizeof(print_buffer);

    int avail_for_write = Serial.availableForWrite();
    if (avail_for_write < avail) avail = avail_for_write;
    rstream.readBytes(print_buffer, avail);
    Serial.write(print_buffer, avail);

  } 


  int ch;
  while ((ch = rstream.read()) != -1)
    Serial.write(ch);
}

void loop() {
  MTP.loop();  //This is mandatory to be placed in the loop code.

  if (Serial.available()) {
    uint8_t command = Serial.read();
    switch (command) {
    case 'c':
      // start capture debug info
      rstream.head_ = 0;
      rstream.tail_ = 0;
      MTP.PrintStream(&rstream); // Setup which stream to use...
      Serial.println("Capturing MTP debug output");
      break;
    case 's':
      Serial.println("Stop Captured data");
      rstream.head_ = 0;
      rstream.tail_ = 0;
      break;
    case 'p':
      MTP.PrintStream(&Serial); // Setup which stream to use...
      Serial.println("Print Captured data");
      print_capture_data();
      rstream.head_ = 0;
      rstream.tail_ = 0;
      break;
    case 'r':
      Serial.println("Reset");
      MTP.send_DeviceResetEvent();
      break;
    case 'd':
      // first dump list of storages:
      {
        uint32_t fsCount = MTP.getFilesystemCount();
        Serial.printf("\nDump Storage list(%u)\n", fsCount);
        for (uint32_t ii = 0; ii < fsCount; ii++) {
          Serial.printf("store:%u storage:%x name:%s fs:%x\n", ii,
                        MTP.Store2Storage(ii), MTP.getFilesystemNameByIndex(ii),
                        (uint32_t)MTP.getFilesystemNameByIndex(ii));
        }
        Serial.println("\nDump Index List");
        MTP.storage()->dumpIndexList();
      }
      break;
    default:  
      Serial.println("Menu");
      Serial.println("\t c - start capture debug data");
      Serial.println("\t p - Stop capture and print");
      Serial.println("\t s - stop capture and discard");
      Serial.println("\t d - dump storage info");
      Serial.println("\t r - Send MTP Reset");
    }
    while (Serial.read() != -1);
  }

}
