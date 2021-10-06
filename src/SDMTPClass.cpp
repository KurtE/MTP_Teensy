
#include <SDMTPClass.h>
#include <mscFS.h>

#if defined(MAINTAIN_FREE_CLUSTER_COUNT) && (MAINTAIN_FREE_CLUSTER_COUNT == 0)
#warning "###############################################################"
#warning "# MAINTAIN_FREE_CLUSTER_COUNT is 0 in SdFatConfig.h           #"
#warning "# Large Fat32 SD cards will likely fail to work properly      #"
#warning "###############################################################"
#endif

//=============================================================================
// try to get the right FS for this store and then call it's format if we have
// one...
// Here is for LittleFS...
PFsLib pfsLIB;
bool SDMTPClass::formatStore(MTPStorage_SD *mtpstorage, uint32_t store,
                                uint32_t user_token, uint32_t p2) {
  // Lets map the user_token back to oint to our object...
  MTPD::PrintStream()->printf(
      "Format Callback: user_token:%x store: %u p2:%u \n", user_token,
      store, p2);
  SDClass *psd = (SDClass *)user_token;

  if (psd->sdfs.fatType() == FAT_TYPE_FAT12) {
    MTPD::PrintStream()->printf("    Fat12 not supported\n");
    return false;
  }

  // For all of these the fat ones will do on post_process
  PFsVolume partVol;
  if (!partVol.begin(psd->sdfs.card(), true, 1))
    return false;
  bool success = pfsLIB.formatter(partVol);
  return success;
}

uint64_t SDMTPClass::usedSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                                uint32_t user_token) {
  // Courious how often called and how long it takes...
  MTPD::PrintStream()->printf(
      "\n\n}}}}}}}}} SDMTPClass::usedSizeCB called %x %u %u cs:%u ",
      (uint32_t)mtpstorage, store, user_token, csPin_);
  if (!disk_valid_) {
    MTPD::PrintStream()->println("* not inserted *");
    return 0l;
  }
  MTPD::PrintStream()->printf("ft:%u\n", sdfs.vol()->fatType());
  elapsedMillis em = 0;
  uint64_t us;
  if (info_sector_free_clusters_)
    us = (sdfs.clusterCount() - info_sector_free_clusters_) *
         (uint64_t)sdfs.bytesPerCluster();
  else
    us = usedSize();

  MTPD::PrintStream()->printf("us:%u t:%u\n", (uint32_t)us, (uint32_t)em);
  return us;
}

uint64_t SDMTPClass::totalSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                                 uint32_t user_token) {
  // Courious how often called and how long it takes...
  MTPD::PrintStream()->printf(
      "\n\n{{{{{{{{{ SDMTPClass::totalSizeCB called %x %u %u cs:%u ",
      (uint32_t)mtpstorage, store, user_token, csPin_);
  if (!disk_valid_) {
    MTPD::PrintStream()->println("* not inserted *");
    return 0l;
  }
  MTPD::PrintStream()->printf("ft:%u\n", sdfs.vol()->fatType());
  elapsedMillis em = 0;
  uint64_t us = totalSize();
  MTPD::PrintStream()->println(em, DEC);
  return us;
}

void SDMTPClass::dateTime(uint16_t *date, uint16_t *time, uint8_t *ms10) {
  uint32_t now = Teensy3Clock.get();
  if (now < 315532800) { // before 1980
    *date = 0;
    *time = 0;
    *ms10 = 0;
  } else {
    DateTimeFields datetime;
    breakTime(now, datetime);
    *date = FS_DATE(datetime.year + 1900, datetime.mon + 1, datetime.mday);
    *time = FS_TIME(datetime.hour, datetime.min, datetime.sec);
    *ms10 = datetime.sec & 1 ? 100 : 0;
  }
  MTPD::PrintStream()->printf("### dateTime: called %x %x %x\n", *date, *time,
                              *ms10);
}

//===================================================
// SD Testing for disk insertion.
#if defined(ARDUINO_TEENSY41)
#define _SD_DAT3 46
#elif defined(ARDUINO_TEENSY40)
#define _SD_DAT3 38
#elif defined(ARDUINO_TEENSY_MICROMOD)
#define _SD_DAT3 38
#elif defined(ARDUINO_TEENSY35) || defined(ARDUINO_TEENSY36)
#define _SD_DAT3 62
#endif

bool SDMTPClass::init(bool add_if_missing) {
#ifdef BUILTIN_SDCARD
  if (csPin_ == BUILTIN_SDCARD) {
    if (!(disk_valid_ = sdfs.begin(SdioConfig(FIFO_SDIO)))) {
      if (!add_if_missing)
        return false; // not here and don't add...
#ifdef _SD_DAT3
      cdPin_ = _SD_DAT3;
      pinMode(_SD_DAT3, INPUT_PULLDOWN);
      loop_tasks_ = LOOP_TASKS_CHECK_INSERT;
      disk_inserted_time_ = 0;
// return true;
#else
      return false;
#endif
    }
  } else
#endif
  {
    MTPD::PrintStream()->printf("Trying to open SPI config: %u %u %u %u\n",
                                csPin_, cdPin_, opt_, maxSpeed_);
    if (!(disk_valid_ =
              sdfs.begin(SdSpiConfig(csPin_, opt_, maxSpeed_, port_)))) {
      MTPD::PrintStream()->println("    Failed to open");
      if (!add_if_missing || (cdPin_ == 0xff))
        return false; // Not found and no add or not detect
      disk_inserted_time_ = 0;
      // return true;
      loop_tasks_ = LOOP_TASKS_CHECK_INSERT;
      pinMode(cdPin_, INPUT_PULLUP); // connects I think to SDCard frame ground
    }
  }
  addFSToStorage(false); // do the work to add us to the storage list.
  return true;
}

bool SDMTPClass::loop(MTPStorage_SD *mtpstorage, uint32_t store,
                      uint32_t user_token) {
  // MTPD::PrintStream()->printf("SDMTPClass::loop %u\n", csPin_);

  if (loop_tasks_ & LOOP_TASKS_CHECK_INSERT) {
// Check for insertions and in some case removals of disks...
#ifdef _SD_DAT3
    if (csPin_ == BUILTIN_SDCARD) {
      // BUGBUG:: what if SDCard is slow startup and first try disables the
      // csPin?
      // some of this and the normal pins can maybe be combined although not
      // sure as for cases of detecting removal and above to maybe handle slow
      // card start...
      if (digitalRead(cdPin_)) {
        delay(1); // give it some time to settle;
        if (!digitalRead(cdPin_))
          return true; // double check.
        if (disk_inserted_time_ == 0)
          disk_inserted_time_ = millis(); // when did we detect it...
        delay(25);                        // time to stabilize
        MTPD::PrintStream()->printf("\n*** SDIO Card Inserted ***");

        // Note this will probably disable the SD_DAT3 pin... i.e. convert it to
        // SDIO mode
        if (!(disk_valid_ = sdfs.begin(SdioConfig(FIFO_SDIO)))) {
#ifdef _SD_DAT3
// pinMode(_SD_DAT3, INPUT_PULLDOWN);  // it failed try to reinit again
#endif
          MTPD::PrintStream()->println("    Begin Failed");
          if ((millis() - disk_inserted_time_) > DISK_INSERT_TEST_TIME) {
            // pinMode(cdPin_, INPUT_DISABLE);
            loop_tasks_ = false; // only try this once
            MTPD::PrintStream()->println("    Time Out");
          }
          return true; // bail
        }

        loop_tasks_ = LOOP_TASKS_NONE; // only try this once
      }
    } else
#endif
    {

      // delayMicroseconds(5);
      if (digitalRead(cdPin_)) {
        delay(1); // give it some time to settle;
        if (!digitalRead(cdPin_))
          return true; // double check.
        if (disk_inserted_time_ == 0)
          disk_inserted_time_ = millis(); // when did we detect it...
        // looks like SD Inserted. so disable the pin for now...
        // BUGBUG for SPI ones with extra IO pins can do more...

        delay(25); // time to stabilize
        MTPD::PrintStream()->printf(
            "*** SD SPI Card (%u %u %u %u) Inserted ***\n", csPin_, cdPin_,
            opt_, maxSpeed_);
        if (!(disk_valid_ =
                  sdfs.begin(SdSpiConfig(csPin_, opt_, maxSpeed_, port_)))) {
          MTPD::PrintStream()->println("    Begin Failed");
          if ((millis() - disk_inserted_time_) > DISK_INSERT_TEST_TIME) {
            pinMode(cdPin_, INPUT_DISABLE);
            loop_tasks_ = LOOP_TASKS_NONE; // only try this once
            MTPD::PrintStream()->println("    Time Out");
          }
          // pinMode(cdPin_, INPUT_PULLDOWN);  // BUGBUG retury?  But first
          // probably only when other removed?
          return true;
        }
        // not sure yet.. may not do this after it works.
        pinMode(cdPin_, INPUT_DISABLE);
        loop_tasks_ = LOOP_TASKS_NONE; // only try this once
      }
      addFSToStorage(true); // do the work to add us to the storage list.
    }
  }
  return true;
}

void SDMTPClass::addFSToStorage(bool send_events) {
  // Lets first get size so won't hold up later
  if (disk_valid_) {
    // only called if the disk is actually there...
    uint64_t ts = totalSize();
    MTPD::PrintStream()->print("Total Size: ");
    MTPD::PrintStream()->print(ts);

    // BUGBUG:: There are issues with FAT32, that the
    // default usedSize can take many (like sometimes 10) seconds
    // to compute... So if done at program startup (like here), can
    // cause side effects like maybe MTP bails or Serial does not work
    // If done when MTP enums storages, can cause MTP to bail...
    // So lets try to shortcut it.

    uint64_t us;
    if (sdfs.vol()->fatType() == 32) {
      // Lets try pfsVolume code...
      PFsVolume partVol;
      if (partVol.begin(sdfs.card(), true, 1)) {
        info_sector_free_clusters_ = partVol.getFSInfoSectorFreeClusterCount();
        us = (sdfs.clusterCount() - info_sector_free_clusters_) *
             (uint64_t)sdfs.bytesPerCluster();
      } else {
        us = ts / 2; // Hack say we have half space...
      }

    } else {
      us = usedSize();
    }
    MTPD::PrintStream()->print(" Used Size: ");
    MTPD::PrintStream()->println(us);
  }

  // The SD is valid now...
  uint32_t store = storage_.getStoreID(sdc_name_);
  if (store != 0xFFFFFFFFUL) {
    // if(send_events) mtpd_.send_StoreRemovedEvent(store);
    delay(50);
    // mtpd_.send_StorageInfoChangedEvent(store);
    // if(send_events) mtpd_.send_StoreAddedEvent(store);
    MTPD::PrintStream()->println(
        "SDMTPClass::addFSToStorage - Disk in list, try reset event");
    if (send_events)
      mtpd_.send_DeviceResetEvent();

  } else {
    // not in our list, try adding it
    MTPD::PrintStream()->println("addFSToStorage: Added FS");
    store_ =
        storage_.addFilesystem(*this, sdc_name_, this, (uint32_t)(void *)this);
    if (send_events)
      mtpd_.send_StoreAddedEvent(store_);
  }
}
