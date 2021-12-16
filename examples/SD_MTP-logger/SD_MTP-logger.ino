/*
  SF  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#define PLAY_WAVE_FILES
#include "SD.h"
#include <MTP_Teensy.h>

#define USE_BUILTIN_SDCARD
#if defined(USE_BUILTIN_SDCARD) && defined(BUILTIN_SDCARD)
#define CS_SD BUILTIN_SDCARD
#else
#define CS_SD 10
#endif
#define SPI_SPEED SD_SCK_MHZ(16) // adjust to sd card

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

uint8_t active_storage = 0;

// Add in MTPD objects
MTPStorage storage;
MTPD mtpd(&storage);

typedef struct {
  uint8_t csPin;
  uint8_t cdPin;
  const char *name;
  bool media_present; 
  SDClass sd;

} SDList_t;
SDList_t myfs[] = {
  {BUILTIN_SDCARD, 0xff, "SD_Builtin"},
  {5, 4, "SD_5"},
  {10,0xff, "SD_10"}
};
#define COUNT_MYFS (sizeof(myfs)/sizeof(myfs[0])) // 2 // could do by count, but can limit how many are created...

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


// Experiment add memory FS to mainly hold the storage index
// May want to wrap this all up as well
#include <LittleFS.h>
#define LFSRAM_SIZE 65536 // probably more than enough...
LittleFS_RAM lfsram;

elapsedMillis elapsed_millis_since_last_sd_check = 0;
uint8_t index_sd_check = 0;
#define TIME_BETWEEN_SD_CHECKS_MS 2500


#define DBGSerial Serial

void setup() {

  // Open serial communications and wait for port to open:
  DBGSerial.begin(2000000);
  while (!DBGSerial && millis() < 5000) {
    // wait for serial port to connect.
  }
  if (CrashReport) {
    DBGSerial.print(CrashReport);
  }
  DBGSerial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  DBGSerial.println("Initializing SD ...");

  mtpd.begin();
  // Try to add all of them.
  bool storage_added = false;
  for (uint8_t i = 0; i < COUNT_MYFS; i++) {
    #if 1
    myfs[i].media_present = myfs[i].sd.begin(myfs[i].csPin);
    if (myfs[i].cdPin != 0xff) myfs[i].sd.setMediaDetectPin(myfs[i].cdPin);
    storage_added = true;
    storage.addFilesystem(myfs[i].sd, myfs[i].name);
    #else
    if (myfs[i].sd.begin(myfs[i].csPin)) {
      storage_added = true;
      storage.addFilesystem(myfs[i].sd, myfs[i].name);
    }
    #endif
  }
  if (!storage_added) {
    DBGSerial.println("Failed to add any valid storage objects");
    pinMode(13, OUTPUT);
    while (1) {
      digitalToggleFast(13);
      delay(250);
    }
  }

  // lets initialize a RAM drive.
  if (lfsram.begin(LFSRAM_SIZE)) {
    DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = storage.addFilesystem(lfsram, "RAM");
    if (istore != 0xFFFFFFFFUL)
      storage.setIndexStore(istore);
    DBGSerial.printf("Set Storage Index drive to %u\n", istore);
  }

  DBGSerial.println("SD initialized.");

  menu();
}

void loop() {
  if (DBGSerial.available()) {
    uint8_t command = DBGSerial.read();
    int ch = DBGSerial.read();
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
        uint8_t temp = CommandLineReadNextNumber(ch, 0);
        while (ch == ' ')
          ch = DBGSerial.read();

        DBGSerial.printf("Drive # %d Selected\n", active_storage);
        active_storage = temp;
      }
      break;
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
      dataFile = myfs[active_storage].sd.open("datalog.txt", FILE_WRITE);
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
#ifdef PLAY_WAVE_FILES
    case 'P':
      playDir(&myfs[active_storage].sd);
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
      playFile(&myfs[active_storage].sd, filename);
      DBGSerial.println("Done.");
      break;
    }
#endif
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

    if (elapsed_millis_since_last_sd_check >= TIME_BETWEEN_SD_CHECKS_MS) {
      elapsed_millis_since_last_sd_check = 0; 
      bool storage_changed = false;
      for (uint8_t i = 0; i < COUNT_MYFS; i++) {
        elapsedMicros em = 0;
        bool media_present = myfs[i].sd.mediaPresent();
        if (media_present != myfs[i].media_present) {
          storage_changed = true;
          myfs[i].media_present = media_present;
          if (media_present) DBGSerial.printf("\n### %s(%d) inserted dt:%u\n",  myfs[i].name, i, (uint32_t)em);
          else {
            DBGSerial.printf("\n### %s(%d) removed dt:%u\n",  myfs[i].name, i, (uint32_t)em);
            myfs[i].sd.sdfs.end();
          }
        } else {
          DBGSerial.printf("  Check %s %u %u\n", myfs[i].name, media_present, (uint32_t)em);
        }
      }
      if (storage_changed) {
        mtpd.send_DeviceResetEvent();
      }
    }
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
  dataFile = myfs[active_storage].sd.open("datalog.txt");

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
#ifdef PLAY_WAVE_FILES
  DBGSerial.println("\tp[filename] - play audio wave file");
  DBGSerial.println("\tP - Play all wave files");
#endif  
  DBGSerial.println("\th - Menu");
  DBGSerial.println();
}

void listFiles() {
  DBGSerial.print("\n Space Used = ");
  DBGSerial.println(myfs[active_storage].sd.usedSize());
  DBGSerial.print("Filesystem Size = ");
  DBGSerial.println(myfs[active_storage].sd.totalSize());

  printDirectory(myfs[active_storage].sd);
}

void eraseFiles() {

  if (myfs[active_storage].sd.format(0, '.')) {
    DBGSerial.println("\nFiles erased !");
    mtpd.send_DeviceResetEvent();
  }
}

void printDirectory(FS &fs) {
  DBGSerial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
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