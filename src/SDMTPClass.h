#ifndef SDMTPCLASS_H
#define SDMTPCLASS_H

#include <MTP_Teensy.h>
#include <SD.h>
#include <mscFS.h>

// Setup a callback class for Littlefs storages..
// Plus helper functions for handling SD disk insertions.
class SDMTPClass : public SDClass, public MTPStorageInterfaceCB {
public:
  // constructor
  SDMTPClass(MTPD &mtpd, MTPStorage_SD &storage, const char *sdc_name,
             uint8_t csPin, uint8_t cdPin = 0xff, uint8_t opt = SHARED_SPI,
             uint32_t maxSpeed = SD_SCK_MHZ(50), SpiPort_t *port = &SPI)
      : mtpd_(mtpd), storage_(storage), sdc_name_(sdc_name), csPin_(csPin),
        cdPin_(cdPin), opt_(opt), maxSpeed_(maxSpeed), port_(port) {
    // Make sure we have a date time callback set...
    FsDateTime::callback = dateTime;
  };

  // simple parameter retrieve methods
  const char *name(void) { return sdc_name_; }
  uint8_t csPin(void) { return csPin_; }
  uint8_t cdPin(void) { return cdPin_; }
  uint32_t store(void) { return store_; }

  //---------------------------------------------------------------------------
  // Callback function overrides.
  uint8_t formatStore(MTPStorage_SD *mtpstorage, uint32_t store,
                      uint32_t user_token, uint32_t p2, bool post_process);
  uint64_t totalSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                       uint32_t user_token);
  uint64_t usedSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                      uint32_t user_token);
  bool loop(MTPStorage_SD *mtpstorage, uint32_t store, uint32_t user_token);

  // Support functions for SD Insertions.
  bool init(bool add_if_missing);

  // date time callback from SD library
  static void dateTime(uint16_t *date, uint16_t *time, uint8_t *ms10);

private:
  // helper functions
  void addFSToStorage(bool send_events);

  // local variables.
  MTPD &mtpd_;
  MTPStorage_SD &storage_;
  const char *sdc_name_;
  uint8_t csPin_;
  uint8_t cdPin_; // chip detect
  uint8_t opt_;
  uint32_t maxSpeed_;
  SpiPort_t *port_;
  uint32_t store_;

  // Currently SDIO only, may add. SPI ones later
  enum { LOOP_TASKS_NONE = 0, LOOP_TASKS_CHECK_INSERT = 0x01 };
  uint8_t loop_tasks_ = LOOP_TASKS_NONE;
  bool disk_valid_ = false;
  uint32_t info_sector_free_clusters_ = 0;
  uint32_t disk_inserted_time_ = 0;
  enum { DISK_INSERT_TEST_TIME = 2000 };
};

#endif
