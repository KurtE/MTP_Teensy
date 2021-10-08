/*
  LittleFS  datalogger

  This example shows how to log data from three analog sensors
  to an storage device such as a FLASH.

  This example code is in the public domain.
*/
#include <LittleFS.h>
#include <MTP_Teensy.h>

LittleFS_Program
    myfs; // Used to create FS using Program memory, i.e., on chip flash

File dataFile; // Specifes that dataFile is of File type

int record_count = 0;
bool write_data = false;
uint32_t diskSize;

static const uint32_t file_system_size = 1024 * 1024 * 1;

// Add in MTPD objects
MTPStorage storage;
MTPD mtpd(&storage);

void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    // wait for serial port to connect.
  }
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);

  Serial.print("Initializing LittleFS ...");

// see if the Flash is present and can be initialized:
// If using a Locked Teensy 4.0 there is a 960K block limit.
#if ARDUINO_TEENSY40
  if ((IOMUXC_GPR_GPR11 & 0x100) == 0x100) {
    // if security is active max disk size is 960x1024
    Serial.println("SECURITY ENABLED");
    if (file_system_size > 960 * 1024) {
      diskSize = 960 * 1024;
      Serial.printf("PROGRAM disk defaulted to %u bytes\n", diskSize);
    } else {
      diskSize = file_system_size;
      Serial.printf("PROGRAM disk using %u bytes\n", diskSize);
    }
  }
#else
  diskSize = file_system_size;
#endif

  // checks that the LittFS program has started with the disk size specified
  if (!myfs.begin(file_system_size)) {
    Serial.printf("Error starting %s\n", "PROGRAM FLASH DISK");
    while (1) {
      // Error, so don't do anything more - stay stuck here
    }
  }

  mtpd.begin();
  storage.addFilesystem(myfs, "Program");

  Serial.println("LittleFS initialized.");

  menu();
}

void loop() {
  if (Serial.available()) {
    char rr;
    rr = Serial.read();
    switch (rr) {
    case 'l':
      listFiles();
      break;
    case 'e':
      eraseFiles();
      break;
    case 's': {
      Serial.println("\nLogging Data!!!");
      write_data = true; // sets flag to continue to write data until new
                         // command is received
      // opens a file or creates a file if not present,  FILE_WRITE will append
      // data to
      // to the file created.
      dataFile = myfs.open("datalog.txt", FILE_WRITE);
      logData();
    } break;
    case 'x':
      stopLogging();
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
    while (Serial.read() != -1)
      ; // remove rest of characters.
  } else
    mtpd.loop();

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
    Serial.println(dataString);
    record_count += 1;
  } else {
    // if the file isn't open, pop up an error:
    Serial.println("error opening datalog.txt");
  }
  delay(100); // run at a reasonable not-too-fast speed for testing
}

void stopLogging() {
  Serial.println("\nStopped Logging Data!!!");
  write_data = false;
  // Closes the data file.
  dataFile.close();
  Serial.printf("Records written = %d\n", record_count);
  mtpd.send_DeviceResetEvent();
}

void dumpLog() {
  Serial.println("\nDumping Log!!!");
  // open the file.
  dataFile = myfs.open("datalog.txt");

  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void menu() {
  Serial.println();
  Serial.println("Menu Options:");
  Serial.println("\tl - List files on disk");
  Serial.println("\te - Erase files on disk");
  Serial.println("\ts - Start Logging data (Restarting logger will append "
                 "records to existing log)");
  Serial.println("\tx - Stop Logging data");
  Serial.println("\td - Dump Log");
  Serial.println("\th - Menu");
  Serial.println();
}

void listFiles() {
  Serial.print("\n Space Used = ");
  Serial.println(myfs.usedSize());
  Serial.print("Filesystem Size = ");
  Serial.println(myfs.totalSize());

  printDirectory(myfs);
}

void eraseFiles() {
  myfs.quickFormat(); // performs a quick format of the created di
  Serial.println("\nFiles erased !");
  mtpd.send_DeviceResetEvent();
}

void printDirectory(FS &fs) {
  Serial.println("Directory\n---------");
  printDirectory(fs.open("/"), 0);
  Serial.println();
}

void printDirectory(File dir, int numSpaces) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // Serial.println("** no more files **");
      break;
    }
    printSpaces(numSpaces);
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numSpaces + 2);
    } else {
      // files have sizes, directories do not
      printSpaces(36 - numSpaces - strlen(entry.name()));
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
