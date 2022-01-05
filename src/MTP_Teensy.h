// MTP.h - Teensy MTP Responder library
// Copyright (C) 2017 Fredrik Hubinette <hubbe@hubbe.net>
//
// With updates from MichaelMC and Yoong Hor Meng <yoonghm@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// modified for SDFS by WMXZ
// Modified by KurtE and mjs513 for Teensy Integration. 

#ifndef MTP_TEENSY_H
#define MTP_TEENSY_H

#if !defined(USB_MTPDISK) && !defined(USB_MTPDISK_SERIAL)
#error "You need to select USB Type: 'MTP Disk (Experimental)'"
#endif

#include "core_pins.h"
#include "usb_dev.h"
extern "C" int usb_mtp_sendEvent(const void *buffer, uint32_t len,
                                 uint32_t timeout);
extern "C" int usb_init_events(void);

#include "MTP_Storage.h"
// modify strings if needed (see MTP.cpp how they are used)
#define MTP_MANUF "PJRC"
#define MTP_MODEL "Teensy"
#define MTP_VERS "1.0"
#define MTP_SERNR "1234"
#define MTP_NAME "Teensy"

#define USE_EVENTS 1

// probably ok to default larger verbose output on these for now
#if defined(__IMXRT1062__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
#define MTP_VERBOSE_PRINT_CONTAINER 1
#endif

extern "C" {
extern volatile uint8_t usb_configuration;
}

// MTP Responder.
class MTPD {
public:
  explicit MTPD(MTPStorage *storage) : storage_(storage) {}
  int begin();

  static inline Stream *PrintStream(void) { return printStream_; }
  static void PrintStream(Stream *stream) { printStream_ = stream; }

private:
  friend class MTPStorage;
  MTPStorage *storage_;
  static Stream *printStream_;

  struct MTPHeader {
    uint32_t len;            // 0
    uint16_t type;           // 4
    uint16_t op;             // 6
    uint32_t transaction_id; // 8
  };

  struct MTPContainer {
    uint32_t len;            // 0
    uint16_t type;           // 4
    uint16_t op;             // 6
    uint32_t transaction_id; // 8
    uint32_t params[5];      // 12
  } __attribute__((__may_alias__));

  typedef struct {
    uint16_t len;   // number of data bytes
    uint16_t index; // position in processing data
    uint16_t size;  // total size of buffer
    uint8_t *data;  // pointer to the data
    void *usb;      // packet info (needed on Teensy 3)
  } packet_buffer_t;

  packet_buffer_t receive_buffer = {0, 0, 0, NULL, NULL};
  packet_buffer_t transmit_buffer = {0, 0, 0, NULL, NULL};
  packet_buffer_t event_buffer = {0, 0, 0, NULL, NULL};
  bool receive_bulk(uint32_t timeout);
  void free_received_bulk();
  void allocate_transmit_bulk();
  int transmit_bulk();
  void allocate_transmit_event();
  int transmit_event();

  //bool op_needs_callback_; // can this be deleted?

#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)
  usb_packet_t *data_buffer_ = NULL; // TODO: delete me
  void get_buffer(); // TODO: delete me
  void receive_buffer_wait(); // TODO: delete me
  bool receive_buffer_timeout(uint32_t to); // TODO: delete me
//  inline MTPContainer *contains (usb_packet_t *receive_buffer) { return
//  (MTPContainer*)(receive_buffer->buf);  }
// possible events for T3.xx ?

#elif defined(__IMXRT1062__)
#define MTP_RX_SIZE MTP_RX_SIZE_480
#define MTP_TX_SIZE MTP_TX_SIZE_480
  int  mtp_rx_size_ = MTP_RX_SIZE; // // TODO: delete me
  int  mtp_tx_size_ = MTP_TX_SIZE; // // TODO: delete me

  uint8_t tx_data_buffer[MTP_TX_SIZE] __attribute__((aligned(32)));

  static const uint32_t DISK_BUFFER_SIZE = 4 * 1024;
  uint32_t disk_pos = 0;

  int push_packet(uint8_t *data_buffer, uint32_t len); // TODO: delete me
  int fetch_packet(uint8_t *data_buffer); // TODO: delete me
  int pull_packet(uint8_t *data_buffer); // TODO: delete me

  static const uint32_t SENDOBJECT_READ_TIMEOUT_MS = 1000;
  uint8_t rx_data_buffer[MTP_RX_SIZE] __attribute__((aligned(32)));
  static uint8_t disk_buffer_[DISK_BUFFER_SIZE] __attribute__((aligned(32)));

#endif

  static uint32_t sessionID_;
  //static time_t
  //getTeensyTime(); // callback function to allow teensy to initialize the clock

  bool write_get_length_ = false;
  uint32_t write_length_ = 0;
  void write(const char *data, int len);
  void write_finish();

  void write8(uint8_t x);
  void write16(uint16_t x);
  void write32(uint32_t x);
  void write64(uint64_t x);

  void writestring(const char *str);

  void WriteDescriptor();
  void WriteStorageIDs();

  void GetStorageInfo(uint32_t storage);

  uint32_t GetNumObjects(uint32_t storage, uint32_t parent);

  void GetObjectHandles(uint32_t storage, uint32_t parent);

  void GetObjectInfo(uint32_t handle);
  void GetObject(uint32_t object_id);
  uint32_t GetPartialObject(uint32_t object_id, uint32_t offset,
                            uint32_t NumBytes);

  void read(char *data, uint32_t size);

  uint32_t ReadMTPHeader();

  uint8_t read8();
  uint16_t read16();
  uint32_t read32();
  int readstring(char *buffer, uint16_t buffer_size);
  int readDateTimeString(uint32_t *pdt);

  //  void read_until_short_packet() ;

  uint32_t SendObjectInfo(uint32_t storage, uint32_t parent, int &object_id);
  void check_memcpy(uint8_t *pdest, const uint8_t *psrc, size_t size, const uint8_t *pb, size_t pb_size);
  bool SendObject();
  bool SendObjectWithYield();

  void GetDevicePropValue(uint32_t prop);
  void GetDevicePropDesc(uint32_t prop);
  void getObjectPropsSupported(uint32_t p1);

  void getObjectPropDesc(uint32_t p1, uint32_t p2);
  void getObjectPropValue(uint32_t p1, uint32_t p2);

  uint32_t setObjectPropValue(uint32_t p1, uint32_t p2);
  uint32_t formatStore(uint32_t storage, uint32_t p2, bool post_process);
  
  static MTPD *g_pmtpd_interval;
  static void _interval_timer_handler();
  static IntervalTimer g_intervaltimer;
  void processIntervalTimer();

  uint32_t deleteObject(uint32_t p1);
  uint32_t copyObject(uint32_t p1, uint32_t p2, uint32_t p3/*, int &object_id*/);
  uint32_t moveObject(uint32_t p1, uint32_t p2, uint32_t p3);
  void openSession(uint32_t id);

  uint32_t TID;
#if USE_EVENTS == 1
  int send_Event(uint16_t eventCode, uint32_t p1);
  int send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2);
  int send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2, uint32_t p3);
#endif

public:
  void loop(void);
  void test(void);
  operator bool() { return usb_configuration && (sessionID_ != 0); }

  void addSendObjectBuffer(
      char *pb,
      uint32_t cb); // you can extend the send object buffer by this buffer

 inline uint32_t Store2Storage(uint32_t store) {
    return ((store + 1) << 16) | storage_->storeMinorIndex(store);
  }
  static inline uint32_t Storage2Store(uint32_t storage) {
    return (storage >> 16) - 1;
  }

#if USE_EVENTS == 1
  int send_Event(uint16_t eventCode);
  int send_addObjectEvent(uint32_t p1);
  int send_removeObjectEvent(uint32_t p1);
  int send_StorageInfoChangedEvent(uint32_t p1);
  int send_DeviceResetEvent(void);
  int send_StoreAddedEvent(uint32_t store);
  int send_StoreRemovedEvent(uint32_t store);

  // higer level version of sending events
  // unclear if should pass in pfs or store?
  bool send_addObjectEvent(uint32_t store, const char *pathname);
  bool send_removeObjectEvent(uint32_t store, const char *pathname);
  void printContainer(const void *container, const char *msg = nullptr);
#endif
  // Support for SendObject, holding parameters from SendObjectInfo.
  int object_id_;
  uint32_t dtCreated_;
  uint32_t dtModified_;
  uint32_t dtFormatStart_ = 0;
  static const uint32_t MAX_FORMAT_TIME_ = 2750; // give a little time. 
  bool storage_ids_sent_ = false;

};

#endif
