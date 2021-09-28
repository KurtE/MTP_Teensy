#ifndef USB_MSC_MTP_H
#define USB_MSC_MTP_H

#if defined(__IMXRT1062__) || defined(ARDUINO_TEENSY36)
#include <MTP_Teensy.h>
#include <mscFS.h>

#define USE_MSC_FAT 3 // set to > 0 experiment with MTP (USBHost.t36 + mscFS)
#define USE_MSC_FAT_VOL 8 // Max MSC FAT Volumes.

class USB_MSC_MTP : public MSCClass, public MTPStorageInterfaceCB {
public:
  USB_MSC_MTP(MTPD &mtpd, MTPStorage_SD &storage)
      : mtpd_(mtpd), storage_(storage){};
  void begin();
  bool mbrDmp(msController *pdrv);
  void checkUSBStatus(bool fInit);
  void dump_hexbytes(const void *ptr, int len);
  uint8_t formatStore(MTPStorage_SD *mtpstorage, uint32_t store,
                      uint32_t user_token, uint32_t p2, bool post_process);
  uint64_t usedSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                      uint32_t user_token);

private:
  MTPD &mtpd_;
  MTPStorage_SD &storage_;

  bool msDrive_previous[USE_MSC_FAT]; // Was this drive there the previous time
                                      // through?
  MSCClass msc[USE_MSC_FAT_VOL];
  char nmsc_str[USE_MSC_FAT_VOL][20];

  uint16_t msc_storage_index[USE_MSC_FAT_VOL];
  uint8_t msc_drive_index[USE_MSC_FAT_VOL]; // probably can find easy way not to
                                            // need this.

#define DEFAULT_FILESIZE 1024
};

#else
// Only those Teensy support USB
#warning "Only Teensy 3.6 and 4.x support MSC"
#endif
#endif
