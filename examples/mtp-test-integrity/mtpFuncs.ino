void storage_configure()
{
  const char *pn;
  DateTimeFields date;
  breakTime(Teensy3Clock.get(), date);
  const char *monthname[12]={
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  DBGSerial.printf("Date: %u %s %u %u:%u:%u\n",
    date.mday, monthname[date.mon], date.year+1900, date.hour, date.min, date.sec);

#if useProIdx == 1
  // Lets add the Program memory version:
  // checks that the LittFS program has started with the disk size specified
  if (lfsProg.begin(file_system_size)) {
    MTP.addFilesystem(lfsProg, "PgmIndx");
  } else {
    Serial.println("Error starting Program Flash storage");
  }
#endif

// lets initialize a RAM drive.


#if useExMem == 1
#if defined ARDUINO_TEENSY41
  if (external_psram_size)
    LFSRAM_SIZE = 4 * 1024 * 1024;
#endif
if (lfsram.begin(LFSRAM_SIZE)) {
    DBGSerial.printf("Ram Drive of size: %u initialized\n", LFSRAM_SIZE);
    uint32_t istore = MTP.addFilesystem(lfsram, "RAMindex");
    if (istore != 0xFFFFFFFFUL)
      MTP.useFileSystemIndexStore(istore);
    DBGSerial.printf("Set Storage Index drive to %u\n", istore);
  }
#endif

//defaults to SDIO when useExMem or useProMem = 0
  #if USE_SD==1
    #if defined SD_SCK
      SPI.setMOSI(SD_MOSI);
      SPI.setMISO(SD_MISO);
      SPI.setSCK(SD_SCK);
    #endif

    for (uint8_t i = 0; i < nsd; i++) {
      if(cs[i] != BUILTIN_SDCARD) {
        pinMode(cs[i], OUTPUT);
        digitalWrite(cs[i], HIGH);
      }
      sd_media_present_prev[i] = sdx[i].begin(cs[i]);
      if (cdPin[i] != 0xff) sdx[i].setMediaDetectPin(cdPin[i]);
        MTP.addFilesystem(sdx[i], sd_str[i]);
      if (sd_media_present_prev[i]) {
        uint64_t totalSize = sdx[i].totalSize();
        uint64_t usedSize  = sdx[i].usedSize();
        Serial.printf("SD Storage %d %d %s ",i,cs[i],sd_str[i]); 
        Serial.print(totalSize); Serial.print(" "); Serial.println(usedSize);
      }
    }
    elapsed_millis_since_last_sd_check = 0;
  #endif

#if USE_LFS_RAM==1
  for (int ii=0; ii<nfs_ram;ii++) {
    if (!ramfs[ii].begin(lfs_ram_size[ii])) {
      DBGSerial.printf("Ram Storage %d %s failed or missing",ii,lfs_ram_str[ii]);
      DBGSerial.println();
    } else {
      pn = ramfs[ii].getMediaName();
      MTP.addFilesystem(ramfs[ii], lfs_ram_str[ii]);
      uint64_t totalSize = ramfs[ii].totalSize();
      uint64_t usedSize  = ramfs[ii].usedSize();
      DBGSerial.printf("RAM Storage %d (%s) %s %llu %llu\n", ii, pn, lfs_ram_str[ii],totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_PROGM==1
  for (int ii=0; ii<nfs_progm;ii++) {
    if (!progmfs[ii].begin(lfs_progm_size[ii])) {
      DBGSerial.printf("Program Storage %d %s failed or missing",ii,lfs_progm_str[ii]);
      DBGSerial.println();
    } else {
      pn = progmfs[ii].getMediaName();
      MTP.addFilesystem(progmfs[ii], lfs_progm_str[ii]);
      uint64_t totalSize = progmfs[ii].totalSize();
      uint64_t usedSize  = progmfs[ii].usedSize();
      DBGSerial.printf("Program Storage %d (%s) %s %llu %llu\n", ii, pn, lfs_progm_str[ii],
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
      pn = qspifs[ii].getMediaName();
      MTP.addFilesystem(qspifs[ii], lfs_qspi_str[ii]);
      uint64_t totalSize = qspifs[ii].totalSize();
      uint64_t usedSize  = qspifs[ii].usedSize();
      DBGSerial.printf("QSPI Storage %d (%s) %s %llu %llu\n", ii, pn, lfs_qspi_str[ii], totalSize, usedSize);
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
      DBGSerial.printf("SPIFlash Storage %d %d %s failed or missing\n",ii,lfs_cs[ii],lfs_spi_str[ii]);
    } else {
      pn = spifs[ii].getMediaName();
      MTP.addFilesystem(spifs[ii], lfs_spi_str[ii]);
      uint64_t totalSize = spifs[ii].totalSize();
      uint64_t usedSize  = spifs[ii].usedSize();
      DBGSerial.printf("SPIFlash Storage %d (%s) %d %s %llu %llu\n", ii, pn, lfs_cs[ii], lfs_spi_str[ii],
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
      pn = nspifs[ii].getMediaName();
      MTP.addFilesystem(nspifs[ii], nspi_str[ii]);
      uint64_t totalSize = nspifs[ii].totalSize();
      uint64_t usedSize  = nspifs[ii].usedSize();
      DBGSerial.printf("Storage %d (%s) %d %s %llu %llu\n", ii, pn, nspi_cs[ii], nspi_str[ii],
        totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_QSPI_NAND == 1
  for(int ii=0; ii<qnspi_nsd;ii++) {
    if(!qnspifs[ii].begin()) {
       DBGSerial.printf("QSPI NAND Storage %d %s failed or missing",ii,qnspi_str[ii]); DBGSerial.println();
    } else {
      pn = qnspifs[ii].getMediaName();
      MTP.addFilesystem(qnspifs[ii], qnspi_str[ii]);
      uint64_t totalSize = qnspifs[ii].totalSize();
      uint64_t usedSize  = qnspifs[ii].usedSize();
      DBGSerial.printf("Storage %d (%s) %s %llu %llu\n", ii, pn, qnspi_str[ii], totalSize, usedSize);
    }
  }
#endif

#if USE_LFS_FRAM==1
  for (int ii=0; ii<qfspi_nsd;ii++) {
    if (USE_SW_PU == 1) {
      pinMode(qfspi_cs[ii],OUTPUT);
      digitalWriteFast(qfspi_cs[ii],HIGH);
    }
    if (!qfspifs[ii].begin(qfspi_cs[ii], SPI)) {
      DBGSerial.printf("SPIFlash Storage %d %d %s failed or missing",ii,qfspi_cs[ii],qfspi_str[ii]);
      DBGSerial.println();
    } else {
      pn = qfspifs[ii].getMediaName();
      MTP.addFilesystem(qfspifs[ii], qfspi_str[ii]);
      uint64_t totalSize = qfspifs[ii].totalSize();
      uint64_t usedSize  = qfspifs[ii].usedSize();
      DBGSerial.printf("SPIFlash Storage %d (%s) %d %s %llu %llu\n", ii, pn, qfspi_cs[ii], qfspi_str[ii],
        totalSize, usedSize);
    }
  }
#endif

}


int ReadAndEchoSerialChar() {
  int ch = DBGSerial.read();
  if (ch >= ' ') DBGSerial.write(ch);
  return ch;
}


#if USE_SD == 1
void checkSDChanges()  {
  if (elapsed_millis_since_last_sd_check >= TIME_BETWEEN_SD_CHECKS_MS) {
  elapsed_millis_since_last_sd_check = 0; 
  bool storage_changed = false;
  for (uint8_t i = 0; i < nsd; i++) {
    elapsedMicros em = 0;
    bool media_present = sdx[i].mediaPresent();
    if (media_present != sd_media_present_prev[i]) {
      storage_changed = true;
      sd_media_present_prev[i] = media_present;
      if (media_present) DBGSerial.printf("\n### %s(%d) inserted dt:%u\n",  sd_str[i], i, (uint32_t)em);
      else DBGSerial.printf("\n### %s(%d) removed dt:%u\n",  sd_str[i], i, (uint32_t)em);
    } //else {
      //DBGSerial.printf("  Check %s %u %u\n", sd_str[i], media_present, (uint32_t)em);
    //}
  }
  if (storage_changed) {
    MTP.send_DeviceResetEvent();
  }
}
}
#endif

#if USE_MSC == 1
void checkMSCChanges() {
  myusb.Task();

  // lets chec each of the drives.
  bool drive_list_changed = false;
  for (uint16_t drive_index = 0; drive_index < (sizeof(drive_list)/sizeof(drive_list[0])); drive_index++) {
    USBDrive *pdrive = drive_list[drive_index];
    if (*pdrive) {
      if (!drive_previous_connected[drive_index] || !pdrive->filesystemsStarted()) {
        Serial.printf("\n === Drive index %d found ===\n", drive_index);
        pdrive->startFilesystems();
        Serial.printf("\nTry Partition list");
        pdrive->printPartionTable(Serial);
        drive_list_changed = true;
        drive_previous_connected[drive_index] = true;
      }
    } else if (drive_previous_connected[drive_index]) {
      Serial.printf("\n === Drive index %d removed ===\n", drive_index);
      drive_previous_connected[drive_index] = false;
      drive_list_changed = true;
    }
  }

  // BUGBUG not 100 correct as drive could have been replaced between calls
  if (drive_list_changed) {
    bool send_device_reset = false;
    for (uint8_t i = 0; i < CNT_USBFS; i++) {
      if (*filesystem_list[i] && (filesystem_list_store_ids[i] == 0xFFFFFFFFUL)) {
        Serial.printf("Found new Volume:%u\n", i); Serial.flush();
        // Lets see if we can get the volume label:
        char volName[20];
        if (filesystem_list[i]->mscfs.getVolumeLabel(volName, sizeof(volName)))
          snprintf(filesystem_list_display_name[i], sizeof(filesystem_list_display_name[i]), "MSC%d-%s", i, volName);
        else
          snprintf(filesystem_list_display_name[i], sizeof(filesystem_list_display_name[i]), "MSC%d", i);
        filesystem_list_store_ids[i] = MTP.addFilesystem(*filesystem_list[i], filesystem_list_display_name[i]);

        // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
        if (MTP.send_StoreAddedEvent(filesystem_list_store_ids[i]) < 0) send_device_reset = true;
      }
      // Or did volume go away?
      else if ((filesystem_list_store_ids[i] != 0xFFFFFFFFUL) && !*filesystem_list[i] ) {
        Serial.printf("Remove volume: index=%d, store id:%x\n", i, filesystem_list_store_ids[i]);
        if (MTP.send_StoreRemovedEvent(filesystem_list_store_ids[i]) < 0) send_device_reset = true;
        MTP.storage()->removeFilesystem(filesystem_list_store_ids[i]);
        // Try to send store added. if > 0 it went through = 0 stores have not been enumerated
        filesystem_list_store_ids[i] = 0xFFFFFFFFUL;
      }
    }
    if (send_device_reset) MTP.send_DeviceResetEvent();
  }
}
#endif

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
  MTP.send_DeviceResetEvent();
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
  DBGSerial.println("\t2# - Select Drive # for Logging (Step 2)");
  DBGSerial.println("\tl - List files on disk");
  DBGSerial.println("\tF - Benchmark device (Good for Flash)");
  DBGSerial.println("\te - Erase files on disk with Format");
  DBGSerial.println("\ts - Start Logging data (Restarting logger will append records to existing log)");
  DBGSerial.println("\tx - Stop Logging data");
  DBGSerial.println("\td - Dump Log");
  DBGSerial.println("\tr - Reset ");
  DBGSerial.printf("\n\t%s","R - Restart Teensy");
  DBGSerial.printf("\n\t%s","i - Write Index File to disk");
  DBGSerial.printf("\n\t%s","'B, or b': Make Big file half of free space, or remove all Big files");
  DBGSerial.printf("\n\t%s","'S, or t': Make 2MB file , or remove all 2MB files");
  DBGSerial.printf("\n\t%s","'n' No verify on Write- TOGGLE");

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

void eraseFiles()
{
  //DBGSerial.println("Formating not supported at this time");
  DBGSerial.println("\n*** Erase/Format started ***");
  myfs->format(1, '.', DBGSerial);
  Serial.println("Completed, sending device reset event");
  MTP.send_DeviceResetEvent();
}

void format3()
{
  //DBGSerial.println("Formating not supported at this time");
  DBGSerial.println("\n*** Erase/Format Unused started ***");
  myfs->format(2,'.', DBGSerial );
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
  Serial.printf( "\tVerify %s bytes %llu : ", szPath, fSize );
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
    uint64_t xx, toWrite;
    toWrite = (myfs->totalSize()) - myfs->usedSize();
    if ( toWrite < 65535 ) {
      Serial.print( "Disk too full! DO :: reformat");
      return;
    }
    if ( size_bigfile < toWrite *2 )
      toWrite = size_bigfile;
    else 
      toWrite/=2;
    toWrite -= SLACK_SPACE;
    xx = toWrite;
    Serial.printf( "\nStart Big write of %llu Bytes", xx);
    uint32_t timeMe = millis();
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
    memset( someData, fileID, MYBLKSIZE );
    uint64_t hh = 0;
    uint64_t kk = toWrite/MYBLKSIZE/60;
    while ( toWrite > MYBLKSIZE && resW > 0 ) {
      resW = file3.write( someData , MYBLKSIZE );
      hh++;
      if ( !(hh % kk) ) Serial.print('.');
      toWrite -= MYBLKSIZE;
    }
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    timeMe = millis() - timeMe;
    file3 = myfs->open(myFile, FILE_WRITE);
    if ( file3.size() > 0 ) {
      filecount++;
      Serial.printf( "\nBig write %s took %5.2f Sec for %llu Bytes : file3.size()=%llu", myFile , timeMe / 1000.0, xx, file3.size() );
    }
    if ( file3 != 0 ) file3.close();
    Serial.printf( "\n\tBig write KBytes per second %5.2f \n", xx / (timeMe / 1.0) );
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
  uint32_t timeMe = micros();
  file3 = myfs->open("LargeIndexedTestfile.txt", FILE_WRITE_BEGIN);
  if (file3) {
    file3.truncate(); // Make sure we wipe out whatever was written earlier
    for (uint32_t i = 0; i < 43000*4; i++) {
      memset(write_buffer, 'A'+ (i & 0xf), sizeof(write_buffer));
      file3.printf("%06u ", i >> 2);  // 4 per physical buffer
      file3.write(write_buffer, i? 120 : 120-12); // first buffer has other data...
      file3.printf("\n");
      if ( !(i % 1024) ) Serial.print('.');

    }
    file3.setCreateTime(dtf);
    file3.setModifyTime(dtf);
    file3.close();
    
    timeMe = micros() - timeMe;
    file3 = myfs->open("LargeIndexedTestfile.txt", FILE_WRITE);
    if ( file3.size() > 0 ) {
       Serial.printf( " Total time to write %d byte: %5.2f seconds\n", file3.size(), (timeMe / 1000.0));
       Serial.printf( "\n\tBig write KBytes per second %5.2f \n", file3.size() / (timeMe / 1000.0) );
    }
    if ( file3 != 0 ) file3.close();
    Serial.println("\ndone.");
    
  }
}

void benchmark() {
  unsigned long buf[1024];
  for(uint8_t i = 0; i<10; i++){
    myFile = myfs->open("WriteSpeedTest.bin", FILE_WRITE_BEGIN);
    if (myFile) {
      const int num_write = 128;
      Serial.printf("Writing %d byte file... ", num_write * 4096);
      elapsedMillis t=0;
      for (int n=0; n < num_write; n++) {
        for (int i=0; i<1024; i++) buf[i] = random();
        myFile.write(buf, 4096);
      }
      myFile.close();
      int ms = t;
      DBGSerial.printf(" %d ms, bandwidth = %d bytes/sec", ms, num_write * 4096 * 1000 / ms);
      myfs->remove("WriteSpeedTest.bin");
    }
    DBGSerial.println();
    delay(2000);
  }
  DBGSerial.println("Bandwidth test finished");
}

const char *getFSPN(uint32_t ii) {
  FS* pfs = MTP.storage()->getStoreFS(ii);
  #if USE_LFS_QSPI==1
    if (pfs == (FS *)&qspifs[0] && USE_LFS_QSPI == 1) {
      DBGSerial.printf("(0)"); Serial.flush();
      return qspifs[0].getMediaName();
    }
  #endif
  #if useExMem == 1
    if (pfs == (FS *)&lfsram) {
      DBGSerial.printf("(1)"); Serial.flush();
      return lfsram.getMediaName();
    }
  #endif
  #if useProIdx == 1
    if (pfs == (FS *)&lfsProg) {
      DBGSerial.printf("(2)"); Serial.flush();
      return lfsProg.getMediaName();
    }
  #endif
  for (uint8_t i = 0; i < 4; i++) {
    #if USE_LFS_RAM == 1
      if ((i < nfs_ram) && pfs == (FS *)&ramfs[i]) {
        DBGSerial.printf("(3-%u)", i); Serial.flush();
        return ramfs[i].getMediaName();
      }
    #endif
    #if USE_LFS_PROGM == 1
      if ((i < nfs_progm) && pfs == (FS *)&progmfs[i]) {
        DBGSerial.printf("(4-%u)", i); Serial.flush();
        return progmfs[i].getMediaName();
      }
    #endif
    #if USE_LFS_SPI == 1
      if ((i < nfs_spi) && pfs == (FS *)&spifs[i]) {
        DBGSerial.printf("(4a-%u)", i); Serial.flush();
        return spifs[i].getMediaName();
      }
    #endif
    #if USE_LFS_NAND == 1
      if ((i < nspi_nsd) && pfs == (FS *)&nspifs[i]) {
        DBGSerial.printf("(5-%u)", i); Serial.flush();
        return nspifs[i].getMediaName();
      }
    #endif
    #if USE_LFS_QSPI_NAND == 1
      if ((i < qnspi_nsd) && pfs == (FS *)&nfs_progm[i]) {
        DBGSerial.printf("(6-%u)", i); Serial.flush();
        return qnspifs[i].getMediaName();
      }
    #endif
    #if USE_LFS_FRAM == 1
      if ((i < qfspi_nsd) && pfs == (FS *)&qfspi[i]){
        DBGSerial.printf("(7-%u)", i); Serial.flush();
        return qfspi[i].getMediaName();
      }
    #endif
   }
  DBGSerial.printf("(8)"); Serial.flush();
  return "";
}
