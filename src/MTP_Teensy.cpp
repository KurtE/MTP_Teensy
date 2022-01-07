// MTP.cpp - Teensy MTP Responder library
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

#if defined(USB_MTPDISK) || defined(USB_MTPDISK_SERIAL)

#include "MTP_Teensy.h"
#undef USB_DESC_LIST_DEFINE
#include "usb_desc.h"

#if defined(__IMXRT1062__)
// following only while usb_mtp is not included in cores
#include "usb_mtp.h"
#endif

#include "usb_names.h"
extern struct usb_string_descriptor_struct usb_string_serial_number;

// Define some of the static members
Stream *MTPD::printStream_ = &Serial;

#if defined(__IMXRT1062__)
DMAMEM uint8_t MTPD::disk_buffer_[DISK_BUFFER_SIZE] __attribute__((aligned(32)));
#endif

#define DEBUG 2
#if DEBUG > 0
#define printf(...) printStream_->printf(__VA_ARGS__)
#else
#define printf(...)
#endif
#if DEBUG > 2
#define DBGPRINTF(...) printf_debug(__VA_ARGS__)
extern "C" {
void printf_debug(const char *format, ...);
}
#else
#define DBGPRINTF(...)
#endif

// temporary include dump library.
//#include <MemoryHexDump.h>

/***************************************************************************************************/
// Container Types
#define MTP_CONTAINER_TYPE_UNDEFINED 0
#define MTP_CONTAINER_TYPE_COMMAND 1
#define MTP_CONTAINER_TYPE_DATA 2
#define MTP_CONTAINER_TYPE_RESPONSE 3
#define MTP_CONTAINER_TYPE_EVENT 4

// Container Offsets
#define MTP_CONTAINER_LENGTH_OFFSET 0
#define MTP_CONTAINER_TYPE_OFFSET 4
#define MTP_CONTAINER_CODE_OFFSET 6
#define MTP_CONTAINER_TRANSACTION_ID_OFFSET 8
#define MTP_CONTAINER_PARAMETER_OFFSET 12
#define MTP_CONTAINER_HEADER_SIZE 12

// Define global(static) members
uint32_t MTPD::sessionID_ = 0;

// MTP Operation Codes
#define MTP_OPERATION_GET_DEVICE_INFO 0x1001
#define MTP_OPERATION_OPEN_SESSION 0x1002
#define MTP_OPERATION_CLOSE_SESSION 0x1003
#define MTP_OPERATION_GET_STORAGE_IDS 0x1004
#define MTP_OPERATION_GET_STORAGE_INFO 0x1005
#define MTP_OPERATION_GET_NUM_OBJECTS 0x1006
#define MTP_OPERATION_GET_OBJECT_HANDLES 0x1007
#define MTP_OPERATION_GET_OBJECT_INFO 0x1008
#define MTP_OPERATION_GET_OBJECT 0x1009
#define MTP_OPERATION_GET_THUMB 0x100A
#define MTP_OPERATION_DELETE_OBJECT 0x100B
#define MTP_OPERATION_SEND_OBJECT_INFO 0x100C
#define MTP_OPERATION_SEND_OBJECT 0x100D
#define MTP_OPERATION_INITIATE_CAPTURE 0x100E
#define MTP_OPERATION_FORMAT_STORE 0x100F
#define MTP_OPERATION_RESET_DEVICE 0x1010
#define MTP_OPERATION_SELF_TEST 0x1011
#define MTP_OPERATION_SET_OBJECT_PROTECTION 0x1012
#define MTP_OPERATION_POWER_DOWN 0x1013
#define MTP_OPERATION_GET_DEVICE_PROP_DESC 0x1014
#define MTP_OPERATION_GET_DEVICE_PROP_VALUE 0x1015
#define MTP_OPERATION_SET_DEVICE_PROP_VALUE 0x1016
#define MTP_OPERATION_RESET_DEVICE_PROP_VALUE 0x1017
#define MTP_OPERATION_TERMINATE_OPEN_CAPTURE 0x1018
#define MTP_OPERATION_MOVE_OBJECT 0x1019
#define MTP_OPERATION_COPY_OBJECT 0x101A
#define MTP_OPERATION_GET_PARTIAL_OBJECT 0x101B
#define MTP_OPERATION_INITIATE_OPEN_CAPTURE 0x101C
#define MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED 0x9801
#define MTP_OPERATION_GET_OBJECT_PROP_DESC 0x9802
#define MTP_OPERATION_GET_OBJECT_PROP_VALUE 0x9803
#define MTP_OPERATION_SET_OBJECT_PROP_VALUE 0x9804
#define MTP_OPERATION_GET_OBJECT_PROP_LIST 0x9805
#define MTP_OPERATION_SET_OBJECT_PROP_LIST 0x9806
#define MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC 0x9807
#define MTP_OPERATION_SEND_OBJECT_PROP_LIST 0x9808
#define MTP_OPERATION_GET_OBJECT_REFERENCES 0x9810
#define MTP_OPERATION_SET_OBJECT_REFERENCES 0x9811
#define MTP_OPERATION_SKIP 0x9820

const unsigned short supported_op[] = {
    MTP_OPERATION_GET_DEVICE_INFO,  // 0x1001
    MTP_OPERATION_OPEN_SESSION,     // 0x1002
    MTP_OPERATION_CLOSE_SESSION,    // 0x1003
    MTP_OPERATION_GET_STORAGE_IDS,  // 0x1004
    MTP_OPERATION_GET_STORAGE_INFO, // 0x1005
    // MTP_OPERATION_GET_NUM_OBJECTS                        ,//0x1006
    MTP_OPERATION_GET_OBJECT_HANDLES, // 0x1007
    MTP_OPERATION_GET_OBJECT_INFO,    // 0x1008
    MTP_OPERATION_GET_OBJECT,         // 0x1009
    // MTP_OPERATION_GET_THUMB                              ,//0x100A
    MTP_OPERATION_DELETE_OBJECT,         // 0x100B
    MTP_OPERATION_SEND_OBJECT_INFO,      // 0x100C
    MTP_OPERATION_SEND_OBJECT,           // 0x100D
    MTP_OPERATION_FORMAT_STORE,          // 0x100F
    MTP_OPERATION_GET_DEVICE_PROP_DESC,  // 0x1014
    MTP_OPERATION_GET_DEVICE_PROP_VALUE, // 0x1015
    // MTP_OPERATION_SET_DEVICE_PROP_VALUE                  ,//0x1016
    // MTP_OPERATION_RESET_DEVICE_PROP_VALUE                ,//0x1017
    MTP_OPERATION_MOVE_OBJECT,        // 0x1019
    MTP_OPERATION_COPY_OBJECT,        // 0x101A
    MTP_OPERATION_GET_PARTIAL_OBJECT, // 0x101B

    MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED, // 0x9801
    MTP_OPERATION_GET_OBJECT_PROP_DESC,       // 0x9802
    MTP_OPERATION_GET_OBJECT_PROP_VALUE,      // 0x9803
    MTP_OPERATION_SET_OBJECT_PROP_VALUE       // 0x9804
    // MTP_OPERATION_GET_OBJECT_PROP_LIST                   ,//0x9805
    // MTP_OPERATION_GET_OBJECT_REFERENCES                  ,//0x9810
    // MTP_OPERATION_SET_OBJECT_REFERENCES                  ,//0x9811

    // MTP_OPERATION_GET_PARTIAL_OBJECT_64                  ,//0x95C1
    // MTP_OPERATION_SEND_PARTIAL_OBJECT                    ,//0x95C2
    // MTP_OPERATION_TRUNCATE_OBJECT                        ,//0x95C3
    // MTP_OPERATION_BEGIN_EDIT_OBJECT                      ,//0x95C4
    // MTP_OPERATION_END_EDIT_OBJECT                         //0x95C5
};

const int supported_op_size = sizeof(supported_op);
const int supported_op_num = supported_op_size / sizeof(supported_op[0]);

#define MTP_PROPERTY_STORAGE_ID 0xDC01
#define MTP_PROPERTY_OBJECT_FORMAT 0xDC02
#define MTP_PROPERTY_PROTECTION_STATUS 0xDC03
#define MTP_PROPERTY_OBJECT_SIZE 0xDC04
#define MTP_PROPERTY_OBJECT_FILE_NAME 0xDC07
#define MTP_PROPERTY_DATE_CREATED 0xDC08
#define MTP_PROPERTY_DATE_MODIFIED 0xDC09
#define MTP_PROPERTY_PARENT_OBJECT 0xDC0B
#define MTP_PROPERTY_PERSISTENT_UID 0xDC41
#define MTP_PROPERTY_NAME 0xDC44

const uint16_t propertyList[] = {
    MTP_PROPERTY_STORAGE_ID,        // 0xDC01
    MTP_PROPERTY_OBJECT_FORMAT,     // 0xDC02
    MTP_PROPERTY_PROTECTION_STATUS, // 0xDC03
    MTP_PROPERTY_OBJECT_SIZE,       // 0xDC04
    MTP_PROPERTY_OBJECT_FILE_NAME,  // 0xDC07
    MTP_PROPERTY_DATE_CREATED, // 0xDC08
    MTP_PROPERTY_DATE_MODIFIED, // 0xDC09
    MTP_PROPERTY_PARENT_OBJECT,  // 0xDC0B
    MTP_PROPERTY_PERSISTENT_UID, // 0xDC41
    MTP_PROPERTY_NAME            // 0xDC44
};

uint32_t propertyListNum = sizeof(propertyList) / sizeof(propertyList[0]);

#define MTP_EVENT_UNDEFINED 0x4000
#define MTP_EVENT_CANCEL_TRANSACTION 0x4001
#define MTP_EVENT_OBJECT_ADDED 0x4002
#define MTP_EVENT_OBJECT_REMOVED 0x4003
#define MTP_EVENT_STORE_ADDED 0x4004
#define MTP_EVENT_STORE_REMOVED 0x4005
#define MTP_EVENT_DEVICE_PROP_CHANGED 0x4006
#define MTP_EVENT_OBJECT_INFO_CHANGED 0x4007
#define MTP_EVENT_DEVICE_INFO_CHANGED 0x4008
#define MTP_EVENT_REQUEST_OBJECT_TRANSFER 0x4009
#define MTP_EVENT_STORE_FULL 0x400A
#define MTP_EVENT_DEVICE_RESET 0x400B
#define MTP_EVENT_STORAGE_INFO_CHANGED 0x400C
#define MTP_EVENT_CAPTURE_COMPLETE 0x400D
#define MTP_EVENT_UNREPORTED_STATUS 0x400E
#define MTP_EVENT_OBJECT_PROP_CHANGED 0xC801
#define MTP_EVENT_OBJECT_PROP_DESC_CHANGED 0xC802
#define MTP_EVENT_OBJECT_REFERENCES_CHANGED 0xC803

const uint16_t supported_events[] = {
    //    MTP_EVENT_UNDEFINED                         ,//0x4000
    MTP_EVENT_CANCEL_TRANSACTION, // 0x4001
    MTP_EVENT_OBJECT_ADDED,       // 0x4002
    MTP_EVENT_OBJECT_REMOVED,     // 0x4003
    MTP_EVENT_STORE_ADDED,        // 0x4004
    MTP_EVENT_STORE_REMOVED,      // 0x4005
    //    MTP_EVENT_DEVICE_PROP_CHANGED               ,//0x4006
    //    MTP_EVENT_OBJECT_INFO_CHANGED               ,//0x4007
    //    MTP_EVENT_DEVICE_INFO_CHANGED               ,//0x4008
    //    MTP_EVENT_REQUEST_OBJECT_TRANSFER           ,//0x4009
    //    MTP_EVENT_STORE_FULL                        ,//0x400A
    MTP_EVENT_DEVICE_RESET,         // 0x400B
    MTP_EVENT_STORAGE_INFO_CHANGED, // 0x400C
    //    MTP_EVENT_CAPTURE_COMPLETE                  ,//0x400D
    MTP_EVENT_UNREPORTED_STATUS,   // 0x400E
    MTP_EVENT_OBJECT_PROP_CHANGED, // 0xC801
    //    MTP_EVENT_OBJECT_PROP_DESC_CHANGED          ,//0xC802
    //    MTP_EVENT_OBJECT_REFERENCES_CHANGED          //0xC803
};

const int supported_event_num =
    sizeof(supported_events) / sizeof(supported_events[0]);

// Responses

#define MTP_RESPONSE_UNDEFINED 0x2000
#define MTP_RESPONSE_OK 0x2001
#define MTP_RESPONSE_GENERAL_ERROR 0x2002
#define MTP_RESPONSE_SESSION_NOT_OPEN 0x2003
#define MTP_RESPONSE_INVALID_TRANSACTION_ID 0x2004
#define MTP_RESPONSE_OPERATION_NOT_SUPPORTED 0x2005
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED 0x2006
#define MTP_RESPONSE_INCOMPLETE_TRANSFER 0x2007
#define MTP_RESPONSE_INVALID_STORAGE_ID 0x2008
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE 0x2009
#define MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED 0x200A
#define MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE 0x200B
#define MTP_RESPONSE_STORAGE_FULL 0x200C
#define MTP_RESPONSE_OBJECT_WRITE_PROTECTED 0x200D
#define MTP_RESPONSE_STORE_READ_ONLY 0x200E
#define MTP_RESPONSE_ACCESS_DENIED 0x200F
#define MTP_RESPONSE_NO_THUMBNAIL_PRESENT 0x2010
#define MTP_RESPONSE_SELF_TEST_FAILED 0x2011
#define MTP_RESPONSE_PARTIAL_DELETION 0x2012
#define MTP_RESPONSE_STORE_NOT_AVAILABLE 0x2013
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED 0x2014
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO 0x2015
#define MTP_RESPONSE_INVALID_CODE_FORMAT 0x2016
#define MTP_RESPONSE_UNKNOWN_VENDOR_CODE 0x2017
#define MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED 0x2018
#define MTP_RESPONSE_DEVICE_BUSY 0x2019
#define MTP_RESPONSE_INVALID_PARENT_OBJECT 0x201A
#define MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT 0x201B
#define MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE 0x201C
#define MTP_RESPONSE_INVALID_PARAMETER 0x201D
#define MTP_RESPONSE_SESSION_ALREADY_OPEN 0x201E
#define MTP_RESPONSE_TRANSACTION_CANCELLED 0x201F
#define MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED 0x2020
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE 0xA801
#define MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT 0xA802
#define MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE 0xA803
#define MTP_RESPONSE_INVALID_OBJECT_REFERENCE 0xA804
#define MTP_RESPONSE_GROUP_NOT_SUPPORTED 0xA805
#define MTP_RESPONSE_INVALID_DATASET 0xA806
#define MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED 0xA807
#define MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED 0xA808
#define MTP_RESPONSE_OBJECT_TOO_LARGE 0xA809
#define MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED 0xA80A

// MTP Responder.
/*
  struct MTPHeader {
    uint32_t len;  // 0
    uint16_t type; // 4
    uint16_t op;   // 6
    uint32_t transaction_id; // 8
  };

  struct MTPContainer {
    uint32_t len;  // 0
    uint16_t type; // 4
    uint16_t op;   // 6
    uint32_t transaction_id; // 8
    uint32_t params[5];    // 12
  };
*/
int MTPD::begin() {

  // lets set up to check for MTP messages and tell
  // other side we are busy...  Maybe should be function:
  g_pmtpd_interval = this;
  printf("\n\n*** Start Interval Timer ***\n");
  g_intervaltimer.begin(&_interval_timer_handler,
                        50000); // try maybe 20 times per second...

#if defined(__IMXRT1062__)
  // get our actual transfer sizes
  mtp_rx_size_ = usb_mtp_rxSize();
  mtp_tx_size_ = usb_mtp_txSize();
#endif

  return usb_init_events();
}

//time_t MTPD::getTeensyTime() { return Teensy3Clock.get(); }

void MTPD::writestring(const char *str) {
  if (*str) {
    write8(strlen(str) + 1);
    while (*str) {
      write16(*str);
      ++str;
    }
    write16(0);
  } else {
    write8(0);
  }
}


static uint32_t writestringlen(const char *str) {
  if (!str) return 1;
  return strlen(str)*2 + 2 + 1;
}



uint32_t MTPD::GetDeviceInfo(struct MTPContainer &cmd) {
  char buf[20];
  dtostrf((float)(TEENSYDUINO / 100.0f), 3, 2, buf);
  strlcat(buf, " / MTP " MTP_VERS, sizeof(buf));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  char sernum[10];
  for (size_t i = 0; i < 10; i++)
    sernum[i] = usb_string_serial_number.wString[i];
#pragma GCC diagnostic pop
  // DeviceInfo Dataset, MTP 1.1 spec, page 40
  uint32_t size = 2 + 4 + 2 + writestringlen("microsoft.com: 1.0;") + 2
                  + 4 + sizeof(supported_op)
                  + 4 + sizeof(supported_events)
                  + 4 + 2 + 4 + 4 + 2*2
                  + writestringlen(MTP_MANUF)
                  + writestringlen(MTP_MODEL)
                  + writestringlen(buf)
                  + writestringlen(sernum);
  printf("GetDeviceInfo size=%u\n", size);
  cmd.len = size + 12;
  cmd.type = MTP_CONTAINER_TYPE_DATA;
  write(&cmd, 12);

  write16(100); // MTP version
  write32(6);   // MTP extension
                //    write32(0xFFFFFFFFUL);    // MTP extension
  write16(100); // MTP version
  writestring("microsoft.com: 1.0;");
  write16(1); // functional mode

  // Supported operations (array of uint16)
  write32(supported_op_num);
  write(supported_op, sizeof(supported_op));

  // Events (array of uint16)
  write32(supported_event_num);
  write(supported_events, sizeof(supported_events));

  write32(1);      // Device properties (array of uint16)
  write16(0xd402); // Device friendly name

  write32(0); // Capture formats (array of uint16)

  write32(2);      // Playback formats (array of uint16)
  write16(0x3000); // Undefined format
  write16(0x3001); // Folders (associations)

  writestring(MTP_MANUF); // Manufacturer
  writestring(MTP_MODEL); // Model
  writestring(buf);       // version
  writestring(sernum);    // serial number
  write_finish();
  return MTP_RESPONSE_OK;
}


void MTPD::WriteStorageIDs() {

  uint32_t num = storage_->get_FSCount();

  // Quick and dirty, we maybe allow some storages to be removed, lets loop
  // through
  // and see if there are any...
  uint32_t num_valid = 0;
  for (uint32_t ii = 0; ii < num; ii++) {
    if (storage_->get_FSName(ii))
      num_valid++; // storage id
  }

  write32(num_valid); // number of storages (disks)
  for (uint32_t ii = 0; ii < num; ii++) {
    if (storage_->get_FSName(ii))
      write32(Store2Storage(ii)); // storage id
  }
  storage_ids_sent_ = true;
}

void MTPD::GetStorageInfo(uint32_t storage) {
  uint32_t store = Storage2Store(storage);
  write16(storage_->readonly(store) ? 0x0001
                                    : 0x0004); // storage type (removable RAM)
  write16(storage_->has_directories(store)
              ? 0x0002
              : 0x0001); // filesystem type (generic hierarchical)
  write16(0x0000);       // access capability (read-write)

//  elapsedMillis em;
  uint64_t ntotal = storage_->totalSize(store);

  write64(ntotal); // max capacity
  // Quick test to see if not getting the used size on large disks helps us get
  // them displayed...
  //    if (ntotal < 3000000000UL) {
  uint64_t nused = storage_->usedSize(store);
  write64((ntotal - nused)); // free space (100M)
  //    } else write64(ntotal/2);  // free space - how about glass half empty or
  //    full
  //
//  printf("GetStorageInfo dt:%u tot:%lu, used: %lu\n", (uint32_t)em, ntotal, nused);
  write32(0xFFFFFFFFUL); // free space (objects)
  const char *name = storage_->get_FSName(store);
  writestring(name); // storage descriptor
  // const char *volumeID = storage_->get_volumeID(store);
  static const char _volumeID[] = "";

  writestring(_volumeID); // volume identifier

  printf("%d %d name:%s\n", storage, store, name);
}

uint32_t MTPD::GetNumObjects(uint32_t storage, uint32_t parent) {
  uint32_t store = Storage2Store(storage);
  storage_->StartGetObjectHandles(store, parent);
  int num = 0;
  while (storage_->GetNextObjectHandle(store))
    num++;
  return num;
}

void MTPD::GetObjectHandles(uint32_t storage, uint32_t parent) {
  uint32_t store = Storage2Store(storage);
  if (write_get_length_) {
    write_length_ = GetNumObjects(storage, parent);
    write_length_++;
    write_length_ *= 4;
  } else {
    write32(GetNumObjects(storage, parent));
    int handle;
    storage_->StartGetObjectHandles(store, parent);
    while ((handle = storage_->GetNextObjectHandle(store)))
      write32(handle);
  }
}

void MTPD::GetObjectInfo(uint32_t handle) {
  char filename[MAX_FILENAME_LEN];
  uint32_t size, parent;
  uint16_t store;
  storage_->GetObjectInfo(handle, filename, &size, &parent, &store);

  uint32_t storage = Store2Storage(store);
  write32(storage);                                // storage
  write16(size == 0xFFFFFFFFUL ? 0x3001 : 0x0000); // format
  write16(0);                                      // protection
  write32(size);                                   // size
  write16(0);                                      // thumb format
  write32(0);                                      // thumb size
  write32(0);                                      // thumb width
  write32(0);                                      // thumb height
  write32(0);                                      // pix width
  write32(0);                                      // pix height
  write32(0);                                      // bit depth
  write32(parent);                                 // parent
  write16(size == 0xFFFFFFFFUL ? 1 : 0);           // association type
  write32(0);                                      // association description
  write32(0);                                      // sequence number
  writestring(filename);

  uint32_t dt;
  DateTimeFields dtf;

  if (storage_->getCreateTime(handle, dt)) {
    // going to use the buffer name to output
    breakTime(dt, dtf);
    snprintf(filename, MAX_FILENAME_LEN, "%04u%02u%02uT%02u%02u%02u",
             dtf.year + 1900, dtf.mon + 1, dtf.mday, dtf.hour, dtf.min,
             dtf.sec);
    writestring(filename);
  } else {
    writestring(""); // date created
  }

  if (storage_->getModifyTime(handle, dt)) {
    // going to use the buffer name to output
    breakTime(dt, dtf);
    snprintf(filename, MAX_FILENAME_LEN, "%04u%02u%02uT%02u%02u%02u",
             dtf.year + 1900, dtf.mon + 1, dtf.mday, dtf.hour, dtf.min,
             dtf.sec);
    writestring(filename);
  } else {
    writestring(""); // date modified
  }

  writestring(""); // keywords
}

uint32_t MTPD::ReadMTPHeader() {
  MTPHeader header;
  read(&header, sizeof(MTPHeader));
  // check that the type is data
  if (header.type == 2)
    return header.len - 12;
  else
    return 0;
}

uint8_t MTPD::read8() {
  uint8_t ret;
  read(&ret, sizeof(ret));
  return ret;
}
uint16_t MTPD::read16() {
  uint16_t ret;
  read(&ret, sizeof(ret));
  return ret;
}
uint32_t MTPD::read32() {
  uint32_t ret;
  read(&ret, sizeof(ret));
  return ret;
}

int MTPD::readstring(char *buffer, uint16_t buffer_size) {
  int len = read8();
  char * buffer_end = buffer + buffer_size -1;
  if (!buffer) {
    read(NULL, len * 2);
  } else {
    for (int i = 0; i < len; i++) {
      int16_t c2 = read16();
      // try not to overwrite memory...
      if (buffer <= buffer_end) *(buffer++) = c2;
    }
    if (buffer > buffer_end) *buffer_end = 0; // make sure null terminated.
  }
  return len * 2 + 1;
}

int MTPD::readDateTimeString(uint32_t *pdt) {
  char dtb[20]; // let it take care of the conversions.
  //                            01234567890123456
  // format of expected String: YYYYMMDDThhmmss.s
  int cb = readstring(dtb, sizeof(dtb));
  if (cb > 1) {
    DateTimeFields dtf;
    printf("Read DateTime string: %s\n", dtb);
    // Quick and dirty!
    uint16_t year = ((dtb[0] - '0') * 1000) + ((dtb[1] - '0') * 100) +
                    ((dtb[2] - '0') * 10) + (dtb[3] - '0');
    dtf.year = year - 1900;                               // range 70-206
    dtf.mon = ((dtb[4] - '0') * 10) + (dtb[5] - '0') - 1; // zero based not 1
    dtf.mday = ((dtb[6] - '0') * 10) + (dtb[7] - '0');
    dtf.wday = 0; // hopefully not needed...
    dtf.hour = ((dtb[9] - '0') * 10) + (dtb[10] - '0');
    dtf.min = ((dtb[11] - '0') * 10) + (dtb[12] - '0');
    dtf.sec = ((dtb[13] - '0') * 10) + (dtb[14] - '0');
    *pdt = makeTime(dtf);
    printf(">> date/Time: %x %u/%u/%u %u:%u:%u\n", *pdt, dtf.mon + 1, dtf.mday,
           year, dtf.hour, dtf.min, dtf.sec);
  }
  return cb;
}

void MTPD::GetDevicePropValue(uint32_t prop) {
  switch (prop) {
  case 0xd402: // friendly name
    // This is the name we'll actually see in the windows explorer.
    // Should probably be configurable.
    writestring(MTP_NAME);
    break;
  }
}

void MTPD::GetDevicePropDesc(uint32_t prop) {
  switch (prop) {
  case 0xd402: // friendly name
    write16(prop);
    write16(0xFFFF); // string type
    write8(0);       // read-only
    GetDevicePropValue(prop);
    GetDevicePropValue(prop);
    write8(0); // no form
  }
}

void MTPD::getObjectPropsSupported(uint32_t p1) {
  write32(propertyListNum);
  for (uint32_t ii = 0; ii < propertyListNum; ii++)
    write16(propertyList[ii]);
}

void MTPD::getObjectPropDesc(uint32_t p1, uint32_t p2) {
  switch (p1) {
  case MTP_PROPERTY_STORAGE_ID: // 0xDC01:
    write16(0xDC01);
    write16(0x006);
    write8(0); // get
    write32(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_OBJECT_FORMAT: // 0xDC02:
    write16(0xDC02);
    write16(0x004);
    write8(0); // get
    write16(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_PROTECTION_STATUS: // 0xDC03:
    write16(0xDC03);
    write16(0x004);
    write8(0); // get
    write16(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_OBJECT_SIZE: // 0xDC04:
    write16(0xDC04);
    write16(0x008);
    write8(0); // get
    write64(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_OBJECT_FILE_NAME: // 0xDC07:
    write16(0xDC07);
    write16(0xFFFF);
    write8(1); // get/set
    write8(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_DATE_CREATED: // 0xDC08:
    write16(0xDC08);
    write16(0xFFFF);
    write8(1); // get
    write8(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_DATE_MODIFIED: // 0xDC09:
    write16(0xDC09);
    write16(0xFFFF);
    write8(1); // may be both get set?
    write8(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_PARENT_OBJECT: // 0xDC0B:
    write16(0xDC0B);
    write16(6);
    write8(0); // get
    write32(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_PERSISTENT_UID: // 0xDC41:
    write16(0xDC41);
    write16(0x0A);
    write8(0); // get
    write64(0);
    write64(0);
    write32(0);
    write8(0);
    break;
  case MTP_PROPERTY_NAME: // 0xDC44:
    write16(0xDC44);
    write16(0xFFFF);
    write8(0); // get
    write8(0);
    write32(0);
    write8(0);
    break;
  default:
    break;
  }
}

void MTPD::getObjectPropValue(uint32_t p1, uint32_t p2) {
  char name[MAX_FILENAME_LEN];
  uint32_t dir;
  uint32_t size;
  uint32_t parent;
  uint16_t store;
  storage_->GetObjectInfo(p1, name, &size, &parent, &store);
  dir = size == 0xFFFFFFFFUL;
  uint32_t storage = Store2Storage(store);
  switch (p2) {
  case MTP_PROPERTY_STORAGE_ID: // 0xDC01:
    write32(storage);
    break;
  case MTP_PROPERTY_OBJECT_FORMAT: // 0xDC02:
    write16(dir ? 0x3001 : 0x3000);
    break;
  case MTP_PROPERTY_PROTECTION_STATUS: // 0xDC03:
    write16(0);
    break;
  case MTP_PROPERTY_OBJECT_SIZE: // 0xDC04:
    write32(size);
    write32(0);
    break;
  case MTP_PROPERTY_OBJECT_FILE_NAME: // 0xDC07:
    writestring(name);
    break;
  case MTP_PROPERTY_DATE_CREATED: // 0xDC08:
    // String is like: YYYYMMDDThhmmss.s
    uint32_t dt;
    DateTimeFields dtf;

    if (storage_->getCreateTime(p1, dt)) {

      // going to use the buffer name to output
      breakTime(dt, dtf);
      snprintf(name, MAX_FILENAME_LEN, "%04u%02u%02uT%02u%02u%02u",
               dtf.year + 1900, dtf.mon + 1, dtf.mday, dtf.hour, dtf.min,
               dtf.sec);
      writestring(name);
      printf("Create (%x)Date/time:%s\n", dt, name);
      break;
    } else {
      printf("Create failed (%x)Date/time\n", dt);
    }
    writestring("");
    break;
  case MTP_PROPERTY_DATE_MODIFIED: // 0xDC09:
  {
    // String is like: YYYYMMDDThhmmss.s
    uint32_t dt;
    DateTimeFields dtf;

    if (storage_->getModifyTime(p1, dt)) {
      // going to use the buffer name to output
      breakTime(dt, dtf);
      snprintf(name, MAX_FILENAME_LEN, "%04u%02u%02uT%02u%02u%02u",
               dtf.year + 1900, dtf.mon + 1, dtf.mday, dtf.hour, dtf.min,
               dtf.sec);
      writestring(name);
      printf("Modify (%x)Date/time:%s\n", dt, name);
      break;
    } else {
      printf("modify failed (%x)Date/time\n", dt);
    }
  }
    writestring("");
    break;
  case MTP_PROPERTY_PARENT_OBJECT: // 0xDC0B:
    write32((store == parent) ? 0 : parent);
    break;
  case MTP_PROPERTY_PERSISTENT_UID: // 0xDC41:
    write32(p1);
    write32(parent);
    write32(storage);
    write32(0);
    break;
  case MTP_PROPERTY_NAME: // 0xDC44:
    writestring(name);
    break;
  default:
    break;
  }
}

uint32_t MTPD::deleteObject(uint32_t handle) {
  if (!storage_->DeleteObject(handle)) {
    return 0x2012; // partial deletion
  }
  return 0x2001;
}

uint32_t MTPD::moveObject(uint32_t handle, uint32_t newStorage,
                          uint32_t newHandle) {
  uint32_t store1 = Storage2Store(newStorage);
  if (newHandle == 0) newHandle = store1;

  if (storage_->move(handle, store1, newHandle))
    return 0x2001;
  else
    return 0x2005;
}

uint32_t MTPD::copyObject(uint32_t handle, uint32_t newStorage,
                          uint32_t newHandle) {
  uint32_t store1 = Storage2Store(newStorage);
  if (newHandle == 0) newHandle = store1;

  return storage_->copy(handle, store1, newHandle);
}

void MTPD::openSession(uint32_t id) {
  sessionID_ = id;
  storage_->ResetIndex();
}













#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)

bool MTPD::receive_bulk(uint32_t timeout) { // T3
  elapsedMillis msec = 0;
  while (msec <= timeout) {
    usb_packet_t *packet = usb_rx(MTP_RX_ENDPOINT);
    if (packet) {
      receive_buffer.len = packet->len;
      receive_buffer.index = 0;
      receive_buffer.size = sizeof(packet->buf);
      receive_buffer.data = packet->buf;
      receive_buffer.usb = packet;
      return true;
    }
  }
  return false;
}

void MTPD::free_received_bulk() { // T3
  if (receive_buffer.usb) {
    usb_free((usb_packet_t *)receive_buffer.usb);
  }
  receive_buffer.len = 0;
  receive_buffer.data = NULL;
  receive_buffer.usb = NULL;
}

void MTPD::allocate_transmit_bulk() { // T3
  while (1) {
    usb_packet_t *packet = usb_malloc();
    if (packet) {
      transmit_buffer.len = 0;
      transmit_buffer.index = 0;
      transmit_buffer.size = sizeof(packet->buf);
      transmit_buffer.data = packet->buf;
      transmit_buffer.usb = packet;
      return;
    }
    mtp_yield();
  }
}

int MTPD::transmit_bulk() { // T3
  usb_packet_t *packet = (usb_packet_t *)transmit_buffer.usb;
  int len = transmit_buffer.len;
  packet->len = len;
  usb_tx(MTP_TX_ENDPOINT, packet);
  transmit_buffer.len = 0;
  transmit_buffer.index = 0;
  transmit_buffer.data = NULL;
  transmit_buffer.usb = NULL;
  return len;
}

//  usb_packet_t *data_buffer_ = NULL;

void MTPD::receive_buffer_wait() { // T3 only
  while (!data_buffer_) {
    data_buffer_ = usb_rx(MTP_RX_ENDPOINT);
    if (!data_buffer_)
      mtp_yield();
  }
}

bool MTPD::receive_buffer_timeout(uint32_t to) { // T3 only
  elapsedMillis em = 0;
  while (!data_buffer_) {
    data_buffer_ = usb_rx(MTP_RX_ENDPOINT);
    if (!data_buffer_) {
      mtp_yield();
      if (em > to) {
        printf("receive_buffer_timeout Timeout");
        return false;
      }
    }
  }
  return true;
}

void MTPD::get_buffer() { // T3 only
  while (!data_buffer_) {
    data_buffer_ = usb_malloc();
    if (!data_buffer_)
      mtp_yield();
  }
}

// TODO: core library not yet implementing cancel on Teensy 3.x
static uint8_t usb_mtp_status = 0x01;


#elif defined(__IMXRT1062__)

bool MTPD::receive_bulk(uint32_t timeout) { // T4
  if (usb_mtp_status != 0x01) return false;
  receive_buffer.index = 0;
  receive_buffer.size = MTP_RX_SIZE;
  receive_buffer.usb = NULL;
  receive_buffer.len = usb_mtp_recv(rx_data_buffer, timeout);
  if (receive_buffer.len > 0) {
    // FIXME: need way to receive ZLP
    receive_buffer.data = rx_data_buffer;
    return true;
  } else {
    receive_buffer.data = NULL;
    return false;
  }
}

void MTPD::free_received_bulk() { // T4
  receive_buffer.len = 0;
  receive_buffer.data = NULL;
}

void MTPD::allocate_transmit_bulk() { // T4
  transmit_buffer.len = 0;
  transmit_buffer.index = 0;
  transmit_buffer.size = usb_mtp_txSize();
  transmit_buffer.data = tx_data_buffer;
  transmit_buffer.usb = NULL;
}

int MTPD::transmit_bulk() { // T4
  int r = 0;
  if (usb_mtp_status == 0x01) {
    usb_mtp_send(transmit_buffer.data, transmit_buffer.len, 50);
  }
  transmit_buffer.len = 0;
  transmit_buffer.index = 0;
  transmit_buffer.data = NULL;
  return r;
}

int MTPD::pull_packet(uint8_t *data_buffer) { // T4 only
  while (!usb_mtp_available())
    ;
  return usb_mtp_recv(data_buffer, 60);
}

int MTPD::push_packet(uint8_t *data_buffer, uint32_t len) { // T4 only
  int count_sent;
  uint8_t loop_count = 0;
  while ((count_sent = usb_mtp_send(data_buffer, len, 60)) <= 0) {
    printf("push_packet: l:%u ret:%d loop:%u\n", len, count_sent, loop_count++);
    if (loop_count == 5) return 0;
  }
  return 1;
}


#endif // __IMXRT1062__





void MTPD::read(void *ptr, uint32_t size) {
  char *data = (char *)ptr;
  while (size > 0) {
    if (receive_buffer.data == NULL) {
      if (!receive_bulk(100)) {
        memset(data, 0, size);
        return;
      }
    }
    // TODO: what happens if read spans multiple packets?  Do any cases exist?
    uint32_t to_copy = receive_buffer.len - receive_buffer.index;
    if (to_copy > size) to_copy = size;
    if (data) {
      memcpy(data, receive_buffer.data + receive_buffer.index, to_copy);
      data += to_copy;
    }
    size -= to_copy;
    receive_buffer.index += to_copy;
    if (receive_buffer.index >= receive_buffer.len) {
      free_received_bulk();
    }
  }
}



void MTPD::write(const void *ptr, int len) {
  if (len < 0) return;
  if (write_get_length_) {
    write_length_ += len;
    return;
  }
  const char *data = (const char *)ptr;
  while (len > 0) {
    if (transmit_buffer.data == NULL) allocate_transmit_bulk();
    unsigned int avail = transmit_buffer.size - transmit_buffer.len;
    unsigned int to_copy = len;
    if (to_copy > avail) to_copy = avail;
    memcpy(transmit_buffer.data + transmit_buffer.len, data, to_copy);
    data += to_copy;
    len -= to_copy;
    transmit_buffer.len += to_copy;
    if (transmit_buffer.len >= transmit_buffer.size) {
      transmit_bulk();
    }
  }
}

void MTPD::write_finish() {
  if (transmit_buffer.data == NULL) {
    if (write_length_ == 0) return;
    printf("send a ZLP\n");
    allocate_transmit_bulk();
  }
  transmit_bulk();
}



#define TRANSMIT(FUN)                                                          \
  do {                                                                         \
    write_length_ = 0;                                                         \
    write_get_length_ = true;                                                  \
    FUN;                                                                       \
    write_get_length_ = false;                                                 \
    MTPHeader header;                                                          \
    header.len = write_length_ + 12;                                           \
    header.type = 2;                                                           \
    header.op = container.op;                                                  \
    header.transaction_id = container.transaction_id;                          \
    write(&header, 12);                                                        \
    FUN;                                                                       \
    write_finish();                                                            \
  } while (0)





uint32_t MTPD::GetObject(struct MTPContainer &cmd) {
  const int object_id = cmd.params[0];
  uint32_t size = storage_->GetSize(object_id);
  //printf("GetObject, size=%u\n", size);
  cmd.len = size + 12;
  cmd.type = MTP_CONTAINER_TYPE_DATA;
  write(&cmd, 12);
  uint32_t pos = 0;
  while (pos < size) {
    if (usb_mtp_status != 0x01) {
      //printf("GetObject, abort\n");
      return 0;
    }
    if (transmit_buffer.data == NULL) allocate_transmit_bulk();
    uint32_t avail = transmit_buffer.size - transmit_buffer.len;
    uint32_t to_copy = size - pos;
    if (to_copy > avail) to_copy = avail;
    //printf("GetObject, read=%u, pos=%u\n", to_copy, pos);
    // Read directly from storage into usb buffer.
    storage_->read(object_id, pos,
                   (char *)(transmit_buffer.data + transmit_buffer.len), to_copy);
    pos += to_copy;
    transmit_buffer.len += to_copy;
    if (transmit_buffer.len >= transmit_buffer.size) {
      transmit_bulk();
    }
  }
  write_finish();
  //printf("GetObject, done\n");
  return MTP_RESPONSE_OK;
}





uint32_t MTPD::GetPartialObject(struct MTPContainer &cmd) {
  uint32_t object_id = cmd.params[0];
  uint32_t offset = cmd.params[1];
  uint32_t NumBytes = cmd.params[2];
  uint32_t size = storage_->GetSize(object_id);
  size -= offset;
  if (NumBytes < size) {
    size = NumBytes;
  }
  cmd.len = size + 12;
  cmd.type = MTP_CONTAINER_TYPE_DATA;
  write(&cmd, 12);
  uint32_t pos = offset; // into data
  while (pos < size) {
    if (usb_mtp_status != 0x01) {
      //printf("GetPartialObject, abort\n");
      return 0;
    }
    if (transmit_buffer.data == NULL) allocate_transmit_bulk();
    uint32_t avail = transmit_buffer.size - transmit_buffer.len;
    uint32_t to_copy = size - pos;
    if (to_copy > avail) to_copy = avail;
    storage_->read(object_id, pos,
                   (char *)(transmit_buffer.data + transmit_buffer.len), to_copy);
    pos += to_copy;
    transmit_buffer.len += to_copy;
    if (transmit_buffer.len >= transmit_buffer.size) {
      transmit_bulk();
    }
  }
  write_finish();
  cmd.params[0] = size;
  return MTP_RESPONSE_OK + (1<<28);
}






#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)

uint32_t MTPD::SendObjectInfo(uint32_t storage, uint32_t parent, int &object_id) { // T3
  uint32_t len = ReadMTPHeader();
  char filename[MAX_FILENAME_LEN];
  dtCreated_ = 0;
  dtModified_ = 0;

  printf("SendObjectInfo: %u %u : ", storage, parent);
  uint32_t store = Storage2Store(storage);

  printf("%x ", read32());
  len -= 4; // storage
  uint16_t oformat = read16();
  len -= 2; // format
  printf("%x ", oformat);
  bool dir = oformat == 0x3001;
  printf("%x ", read16());
  len -= 2; // protection
  uint32_t file_size = read32();
  len -= 4;                 // size
  printf("%x ", file_size); // size
  printf("%x ", read16());
  len -= 2; // thumb format
  printf("%x ", read32());
  len -= 4; // thumb size
  printf("%x ", read32());
  len -= 4; // thumb width
  printf("%x ", read32());
  len -= 4; // thumb height
  printf("%x ", read32());
  len -= 4; // pix width
  printf("%x ", read32());
  len -= 4; // pix height
  printf("%x ", read32());
  len -= 4; // bit depth
  printf("%x ", read32());
  len -= 4; // parent
  printf("%x ", read16());
  len -= 2; // association type
  printf("%x ", read32());
  len -= 4; // association description
  printf("%x ", read32());
  len -= 4; // sequence number

  readstring(filename, sizeof(filename));
  len -= (2 * (strlen(filename) + 1) + 1);
  printf(": %s\n", filename);

  // Next is DateCreated followed by DateModified
  if (len) {
    len -= readDateTimeString(&dtCreated_);
    printf("Created: %x\n", dtCreated_);
  }
  if (len) {
    len -= readDateTimeString(&dtModified_);
    printf("Modified: %x\n", dtModified_);
  }


  // ignore rest of ObjectInfo
  while (len >= 4) {
    read32();
    len -= 4;
  }
  while (len) {
    read8();
    len--;
  }

  // Lets see if we have enough room to store this file:
  uint32_t free_space = storage_->totalSize(store) - storage_->usedSize(store);
  if (file_size > free_space) {
    printf("Size of object:%u is > free space: %u\n", file_size, free_space);
    return MTP_RESPONSE_STORAGE_FULL;
  }

  object_id_ = object_id = storage_->Create(store, parent, dir, filename);
  if ((uint32_t)object_id == 0xFFFFFFFFUL)
    return MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED;

  return MTP_RESPONSE_OK;
}

#elif defined(__IMXRT1062__)

uint32_t MTPD::SendObjectInfo(uint32_t storage, uint32_t parent, int &object_id) { // T4
  pull_packet(rx_data_buffer);
  read(0, 0); // resync read
  printContainer(rx_data_buffer);
  printf("SendObjectInfo: %u %u %x: ", storage, parent, (uint32_t)rx_data_buffer);
  uint32_t store = Storage2Store(storage);

  int len = ReadMTPHeader();
  char filename[MAX_FILENAME_LEN];
  dtCreated_ = 0;
  dtModified_ = 0;

  printf("Len:%d ST:%x ", len, read32());
  len -= 4; // storage
  uint16_t oformat = read16();
  len -= 2; // format
  printf("F:%x ", oformat);
  bool dir = oformat == 0x3001;
  printf("%x ", read16());
  len -= 2; // protection
  uint32_t file_size = read32();
  len -= 4;                 // size
  printf("SZ:%u ", file_size); // size
  printf("%x ", read16());
  len -= 2; // thumb format
  printf("%x ", read32());
  len -= 4; // thumb size
  printf("%x ", read32());
  len -= 4; // thumb width
  printf("%x ", read32());
  len -= 4; // thumb height
  printf("%x ", read32());
  len -= 4; // pix width
  printf("%x ", read32());
  len -= 4; // pix height
  printf("%x ", read32());
  len -= 4; // bit depth
  printf("%x ", read32());
  len -= 4; // parent
  printf("%x ", read16());
  len -= 2; // association type
  printf("%x ", read32());
  len -= 4; // association description
  printf("%x ", read32());
  len -= 4; // sequence number

  len -= readstring(filename, sizeof(filename));
  printf(": %s\n", filename);

  // Next is DateCreated followed by DateModified
  if (len) {
    len -= readDateTimeString(&dtCreated_);
    printf("Created: %x\n", dtCreated_);
  }
  if (len) {
    len -= readDateTimeString(&dtModified_);
    printf("Modified: %x\n", dtModified_);
  }

  // ignore rest of ObjectInfo
  while (len >= 4) {
    read32();
    len -= 4;
  }
  while (len) {
    read8();
    len--;
  }

  // Lets see if we have enough room to store this file:
  uint32_t free_space = storage_->totalSize(store) - storage_->usedSize(store);
  if (file_size > free_space) {
    printf("Size of object:%u is > free space: %u\n", file_size, free_space);
    return MTP_RESPONSE_STORAGE_FULL;
  }

  object_id_ = object_id = storage_->Create(store, parent, dir, filename);
  if ((uint32_t)object_id == 0xFFFFFFFFUL)
    return MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED;

  if (dir) {
    // lets see if we should update the date and time stamps.
    // if it is dirctory, then sendObject will not be called, so do it now.
    if (!storage_->updateDateTimeStamps(object_id_, dtCreated_, dtModified_)) {
      // BUGBUG: failed to update, maybe FS needs little time to settle in
      // before trying this.
      for (uint8_t i = 0; i < 10; i++) {
        printf("!!!(%d) Try delay and call update time stamps again\n", i);
        delay(25);
        if (storage_->updateDateTimeStamps(object_id_, dtCreated_, dtModified_))
          break;
      }
    }
    storage_->close();
  }

  return MTP_RESPONSE_OK;
}

#endif // __IMXRT1062__




#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)

bool MTPD::SendObject() { // T3
  uint32_t len = ReadMTPHeader();
  printf("MTPD::SendObject %u\n", len);
//  uint32_t expected_read_count = 1 + (len-52+63)/64;
//  uint32_t read_count = 0;
  while (len) {
    if (!receive_buffer_timeout(250)) break;
    //read_count++;
    uint32_t to_copy = data_buffer_->len - data_buffer_->index;
    to_copy = min(to_copy, len);
    //elapsedMicros emw = 0;
    bool write_status = storage_->write((char *)(data_buffer_->buf + data_buffer_->index),
                         to_copy);
    //printf("    %u %u %u %u\n", len, to_copy, (uint32_t)emw, write_status);

    if (!write_status) return false;
    data_buffer_->index += to_copy;
    len -= to_copy;
    if (data_buffer_->index == data_buffer_->len) {
      usb_free(data_buffer_);
      data_buffer_ = NULL;
    }
  }
  // lets see if we should update the date and time stamps.
  //printf(" len:%u loop count:%u Expected: %u\n", len, read_count, expected_read_count);
  storage_->updateDateTimeStamps(object_id_, dtCreated_, dtModified_);
  storage_->close();
  return true;
}


#elif defined(__IMXRT1062__)


bool MTPD::SendObject() { // T4
  pull_packet(rx_data_buffer);
  read(0, 0);
  // printContainer(rx_data_buffer);
  uint32_t len = ReadMTPHeader();
  printf("MTPD::SendObject: len:%u\n", len);
  disk_pos = 0;
  elapsedMicros em_total = 0;

  uint32_t sum_read_em = 0;
  uint32_t c_read_em = 0;
  uint32_t read_em_max = 0;

  uint32_t sum_write_em = 0;
  uint32_t c_write_em = 0;
  uint32_t write_em_max = 0;
  uint32_t write_em_max_index = 0;
  uint32_t c_write_em_GE_10 = 0;
  uint32_t c_write_em_GE_20 = 0;
  uint32_t c_write_em_GE_30 = 0;
  uint32_t c_write_em_GE_40 = 0;
  uint32_t c_write_em_GE_50 = 0;
  uint32_t c_write_em_GE_100 = 0;

  uint32_t last_n_write_times[32];

  // first copy in the rest of the first packet into the diskbuffer.
  int cb_recv = MTP_RX_SIZE - sizeof(MTPHeader);
  if (cb_recv > (int)len) cb_recv = len;
  check_memcpy(disk_buffer_, rx_data_buffer + sizeof(MTPHeader), cb_recv, disk_buffer_, DISK_BUFFER_SIZE);
  c_read_em = 1;
  len -= cb_recv;
  disk_pos = cb_recv;

  while ((int)len > 0) {
    // check mtp status
    if (usb_mtp_status != 0x1) {
      printf("\nMTPD::SendObject *** MTP status %x ***\n", usb_mtp_status);
      break;
    }
    // read in next sector
    elapsedMillis emRead = 0;
    cb_recv = usb_mtp_recv(rx_data_buffer, SENDOBJECT_READ_TIMEOUT_MS);
    delayMicroseconds(50);
    if (cb_recv > 0) { // read a packet of data
      uint32_t em = emRead;
      sum_read_em += em;
      c_read_em++;
      if (em > read_em_max)
        read_em_max = em;
    } else {
      if (cb_recv == 0) printf("\nMTPD::SendObject *** USB Read 0 bytes ***\n");
      else printf("\nMTPD::SendObject *** USB Read Timeout ***\n");
      break; //
    }

    // Now see how much of this that will fit into output buffer
    uint32_t bytes = cb_recv; // how many data in usb-packet
    bytes = min(bytes, len);              // loimit at end
    uint32_t to_copy = min(bytes, DISK_BUFFER_SIZE - disk_pos); // how many data to copy to disk buffer
    //  printf("len, bytes,to_copy:    %u %u %u\n", len, bytes, to_copy);
    check_memcpy(disk_buffer_ + disk_pos, rx_data_buffer, to_copy, disk_buffer_, DISK_BUFFER_SIZE);
    disk_pos += to_copy;
    bytes -= to_copy;
    len -= to_copy;
    // printf("a %d %d %d %d %d\n", len,disk_pos,bytes,index,to_copy);
    //
    if (disk_pos == DISK_BUFFER_SIZE) {
      elapsedMillis emWrite = 0;
      if (storage_->write((const char *)disk_buffer_, DISK_BUFFER_SIZE) <
          DISK_BUFFER_SIZE)
        return false;
      uint32_t em = emWrite;
      last_n_write_times[c_write_em & 0x1f] = em;
      sum_write_em += em;
      c_write_em++;
      if (em > write_em_max) {
        write_em_max = em;
        write_em_max_index = c_write_em;
      }
      if (em >= 100) c_write_em_GE_100++;
      else if (em >= 50) c_write_em_GE_50++;
      else if (em >= 40) c_write_em_GE_40++;
      else if (em >= 30) c_write_em_GE_30++;
      else if (em >= 20) c_write_em_GE_20++;
      else if (em >= 10) c_write_em_GE_10++;

      disk_pos = 0;

      if (bytes) // we have still data in transfer buffer, copy to initial disk_buffer_
      {
        check_memcpy(disk_buffer_, rx_data_buffer + to_copy, bytes, disk_buffer_, DISK_BUFFER_SIZE);
        disk_pos += bytes;
        len -= bytes;
      }
      // printf("b %d %d %d %d %d\n", len,disk_pos,bytes,index,to_copy);
    }
  }

  // BUGBUG - should we leave the file if we did not receive all of it?

  //  printf("len %d diskpos: %u\n", len, disk_pos);
  if (disk_pos) {
    elapsedMillis emWrite = 0;
    if (storage_->write((const char *)disk_buffer_, disk_pos) < disk_pos)
      return false;
    uint32_t em = emWrite;
    last_n_write_times[c_write_em & 0x1f] = em;
    sum_write_em += em;
    c_write_em++;
    if (em > write_em_max) {
      write_em_max = em;
        write_em_max_index = c_write_em;
      }
      if (em >= 100) c_write_em_GE_100++;
      else if (em >= 50) c_write_em_GE_50++;
      else if (em >= 40) c_write_em_GE_40++;
      else if (em >= 30) c_write_em_GE_30++;
      else if (em >= 20) c_write_em_GE_20++;
      else if (em >= 10) c_write_em_GE_10++;
    disk_pos = 0;
  }

  // lets see if we should update the date and time stamps.
  storage_->updateDateTimeStamps(object_id_, dtCreated_, dtModified_);

  storage_->close();

  if (c_read_em)
    printf(" # USB Packets: %u total: %u avg ms: %u max: %u\n", c_read_em,
           sum_read_em, sum_read_em / c_read_em, read_em_max);
  if (c_write_em)
    printf(" # Write: %u total:%u avg ms: %u max: %u(%u)\n", c_write_em,
           sum_write_em, sum_write_em / c_write_em, write_em_max, write_em_max_index);
    printf("  >> >=100ms: %u 50:%u 40:%u 30:%u 20:%u 10:%u\n", c_write_em_GE_100,
        c_write_em_GE_50, c_write_em_GE_40, c_write_em_GE_30, c_write_em_GE_20, c_write_em_GE_10);
    printf("  >> Last write times\n ");
    uint8_t dump_count = min(32u, c_write_em);
    uint8_t index = c_write_em;
    while (dump_count--){
      index = (index - 1) & 0x1f;
      printf(" %u",last_n_write_times[index] );
    }
  printf("\n");
  printf(">>>Total Time: %u\n", (uint32_t)em_total);
  Serial.flush();

  return (len == 0);
}

void MTPD::check_memcpy(uint8_t *pdest, const uint8_t *psrc, size_t size, const uint8_t *pb, size_t pb_size) { // T4
  static uint32_t call_count = 0;
  call_count++;
  if (((uint32_t)pdest < (uint32_t)pb) || (((uint32_t)pdest + size) > ((uint32_t)pb + pb_size)) ) {
    printf("\n####### Check_memcpy ******(%u): %u %u %u (%u %u)\n", call_count, (uint32_t)pdest, (uint32_t)psrc, size, (uint32_t)pb, pb_size);
  }
  memcpy(pdest, psrc, size);
}

#endif // __IMXRT1062__




#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)

uint32_t MTPD::setObjectPropValue(uint32_t p1, uint32_t p2) { // T3
  receive_buffer_wait();
  if (p2 == 0xDC07) {
    char filename[MAX_FILENAME_LEN];
    ReadMTPHeader();
    readstring(filename, sizeof(filename));

    storage_->rename(p1, filename);

    return 0x2001;
  } else
    return 0x2005;
}

#elif defined(__IMXRT1062__)

uint32_t MTPD::setObjectPropValue(uint32_t handle, uint32_t p2) { // T4 only
  pull_packet(rx_data_buffer);
  read(0, 0);
  // printContainer(rx_data_buffer);

  if (p2 == 0xDC07) {
    char filename[MAX_FILENAME_LEN];
    ReadMTPHeader();
    readstring(filename, sizeof(filename));
    if (storage_->rename(handle, filename))
      return 0x2001;
    else
      return 0x2005;
  } else
    return 0x2005;
}


#endif // __IMXRT1062__



uint32_t MTPD::formatStore(struct MTPContainer &cmd) {
  printf("formatStore begin\n");
  const uint32_t store = Storage2Store(cmd.params[0]);
  const uint32_t format = cmd.params[1];
  g_pmtpd_interval = this;
  dtFormatStart_ = millis();  // remember when format started
  g_intervaltimer.begin(&_interval_timer_handler, 50000); // 20 Hz
  elapsedMillis msec = 0;
  uint8_t success = storage_->formatStore(store, format);
  if (g_pmtpd_interval) g_intervaltimer.end();
  printf("formatStore success=%u, format took %u ms\n", success, (uint32_t)msec);
  if (success) {
    storage_->ResetIndex(); // maybe should add a less of sledge hammer here.
    // send_DeviceResetEvent();
    return MTP_RESPONSE_OK;
  } else {
    return MTP_RESPONSE_OPERATION_NOT_SUPPORTED;
  }
}



MTPD *MTPD::g_pmtpd_interval = nullptr;
IntervalTimer MTPD::g_intervaltimer;

void MTPD::_interval_timer_handler() {
  if (g_pmtpd_interval)
    g_pmtpd_interval->processIntervalTimer();
}

void MTPD::processIntervalTimer() { // T3
  if (receive_bulk(0)) {
    if (receive_buffer.len >= 12 && receive_buffer.len <= 32) {
      struct MTPContainer container;
      memset(&container, 0, sizeof(container));
      memcpy(&container, receive_buffer.data, receive_buffer.len);
      free_received_bulk();
      printContainer(&container, "timer:"); // to switch on set debug to 1 at beginning of file

      const int op = container.op;
      const int p1 = container.params[0];
      TID = container.transaction_id;

      uint32_t return_code = 0x2001; // 0x2001=OK

      if (container.type == 1) { // command
        switch (op) {
        case MTP_OPERATION_GET_DEVICE_INFO: // GetDescription 0x1001
          return_code = GetDeviceInfo(container);
          break;
        case MTP_OPERATION_OPEN_SESSION: // open session 0x1002
          openSession(p1);
          break;
        case MTP_OPERATION_GET_DEVICE_PROP_DESC: // 1014
          TRANSMIT(GetDevicePropDesc(p1));
          break;
        default:
          return_code = MTP_RESPONSE_DEVICE_BUSY; // busy 0x2019
          break;
        }
      } else {
        // TODO: should this send 0x2005 MTP_RESPONSE_OPERATION_NOT_SUPPORTED ??
        return_code = MTP_RESPONSE_UNDEFINED; // undefined 0x2000
      }
      container.type = 3;
      container.len = 12;
      container.op = return_code;
#if DEBUG > 1
      printContainer(&container);
#endif
      allocate_transmit_bulk();
      memcpy(transmit_buffer.data, &container, container.len);
      transmit_buffer.len = container.len;
      transmit_bulk();
    } else {
      printf("ERROR: intervaltimer received command with %u bytes\n", receive_buffer.len);
      free_received_bulk();
    }
  }
}



void MTPD::loop(void) {
  if (g_pmtpd_interval) {
    g_pmtpd_interval = nullptr; // clear out timer.
    g_intervaltimer.end();      // try maybe 20 times per second...
    printf("*** end Interval Timer ***\n");
  }
  if (receive_bulk(0)) {
    if (receive_buffer.len >= 12 && receive_buffer.len <= 32) {
      // This container holds the operation code received from host
      // Commands which transmit a 12 byte header as the first part
      // of their data phase will reuse this container, overwriting
      // the len & type fields, but keeping op and transaction_id.
      // Then this container is again reused to transmit the final
      // response code, keeping the original transaction_id, but
      // the other 3 header fields are based on "return_code".  If
      // the response requires parameters, they are written into
      // this container's parameter list.
      struct MTPContainer container;
      memset(&container, 0, sizeof(container));
      memcpy(&container, receive_buffer.data, receive_buffer.len);
      free_received_bulk();
      printContainer(&container, "loop:");

      int p1 = container.params[0];
      int p2 = container.params[1];
      int p3 = container.params[2];
      TID = container.transaction_id;

      // The low 16 bits of return_code have the response code
      // operation field.  The top 4 bits indicate the number
      // of parameters to transmit with the response code.
      int return_code = 0x2001; // OK use as default value

      if (container.type == MTP_CONTAINER_TYPE_COMMAND) {
        switch (container.op) {
        case 0x1001: // GetDeviceInfo
          return_code = GetDeviceInfo(container);
          break;

        case 0x1002: // OpenSession
          openSession(p1);
          break;

        case 0x1003: // CloseSession
          printf("MTPD::CloseSession\n");
          sessionID_ = 0; //
          break;

        case 0x1004: // GetStorageIDs
          TRANSMIT(WriteStorageIDs());
          break;

        case 0x1005: // GetStorageInfo
          TRANSMIT(GetStorageInfo(p1));
          break;

        case 0x1006: // GetNumObjects
          if (p2) {
            return_code = 0x2014; // spec by format unsupported
          } else {
            container.params[0] = GetNumObjects(p1, p3);
            return_code = 0x2001 | (1<<28);
          }
          break;

        case 0x1007: // GetObjectHandles
          if (p2) {
            return_code = 0x2014; // spec by format unsupported
          } else {
            TRANSMIT(GetObjectHandles(p1, p3));
          }
          break;

        case 0x1008: // GetObjectInfo
          TRANSMIT(GetObjectInfo(p1));
          break;

        case 0x1009: // GetObject
          return_code = GetObject(container);
          break;

        case 0x100B: // DeleteObject
          if (p2) {
            return_code = 0x2014; // spec by format unsupported
          } else {
            if (!storage_->DeleteObject(p1)) {
              return_code = 0x2012; // partial deletion
            }
          }
          break;

        case 0x100C:                        // SendObjectInfo
          return_code = SendObjectInfo(p1,  // storage
                                       p2,  // parent
                                       p3); // returned object id;
          container.params[1] = p2;
          container.params[2] = p3;
          return_code |= (3 << 28);
          break;

        case 0x100D: // SendObject
          if (!SendObject()) {
            return_code = MTP_RESPONSE_INCOMPLETE_TRANSFER;  // what happens if we don't send response...
            //send_Event(
            //    MTP_EVENT_CANCEL_TRANSACTION); // try sending an event to cancel?
          } else {
              printf("SendObject() returned true\n"); Serial.flush();
          }
          break;

        case 0x100F: // FormatStore
          return_code = formatStore(container);
          break;

        case 0x1014: // GetDevicePropDesc
          TRANSMIT(GetDevicePropDesc(p1));
          break;

        case 0x1015: // GetDevicePropvalue
          TRANSMIT(GetDevicePropValue(p1));
          break;

        case 0x1010: // Reset
          return_code = 0x2005;
          break;

        case 0x1019: // MoveObject
          return_code = moveObject(p1, p2, p3);
          break;

        case 0x101A: // CopyObject
          return_code = copyObject(p1, p2, p3);
          if (!return_code) {
            return_code = 0x2005;
          } else {
            container.params[0] = return_code;
            uint8_t error_code = storage_->getLastError();
            switch (error_code) {
              default:
                return_code = 0x2001;
                break;
              case MTPStorage::RMDIR_FAIL:
              case MTPStorage::WRITE_ERROR:
              case MTPStorage::DEST_OPEN_FAIL:
                return_code = MTP_RESPONSE_STORAGE_FULL;
                break;
            }
            return_code |= (1<<28);
          }
          break;

        case 0x101B: // GetPartialObject
          return_code = GetPartialObject(container);
          break;

        case 0x9801: // getObjectPropsSupported
          TRANSMIT(getObjectPropsSupported(p1));
          break;

        case 0x9802: // getObjectPropDesc
          TRANSMIT(getObjectPropDesc(p1, p2));
          break;

        case 0x9803: // getObjectPropertyValue
          TRANSMIT(getObjectPropValue(p1, p2));
          break;

        case 0x9804: // setObjectPropertyValue
          return_code = setObjectPropValue(p1, p2);
          break;

        default:
          return_code = 0x2005; // operation not supported
          break;
        }
      } else {
        return_code = 0x2005; // we should only get cmds
        printContainer(&container, "!!! unexpected/unknown message:");
      }
      if (return_code && usb_mtp_status == 0x01) {
        container.len = 12 + (return_code >> 28) * 4; // top 4 bits is number of parameters
        container.type = MTP_CONTAINER_TYPE_RESPONSE;
        container.op = (return_code & 0xFFFF);        // low 16 bits is op response code
        // container.transaction_id reused from original received command
        #if DEBUG > 1
        printContainer(&container); // to switch on set debug to 2 at beginning of file
        #endif
        write(&container, container.len);
        write_finish();
      }
    } else {
      printf("ERROR: loop received command with %u bytes\n", receive_buffer.len);
      free_received_bulk();
    }
  }

  // check here to mske sure the USB status is reset
  if (usb_mtp_status != 0x01) {
    printf("mtpd::Loop usb_mtp_status %x != 0x1 reset\n", usb_mtp_status);
    usb_mtp_status = 0x01;
  }

  // See if Storage needs to do anything
  storage_->loop();
}








#if USE_EVENTS == 1

#if defined(__MK20DX128__) || defined(__MK20DX256__) ||                        \
    defined(__MK64FX512__) || defined(__MK66FX1M0__)

#include "usb_mtp.h"
extern "C" {
usb_packet_t *tx_event_packet = NULL;

int usb_init_events(void) { // T3
  //     tx_event_packet = usb_malloc();
  //     if(tx_event_packet) return 1; else return 0;
  return 1;
}

#define EVENT_TX_PACKET_LIMIT 4

int usb_mtp_sendEvent(const void *buffer, uint32_t len, uint32_t timeout) { // T3
//  digitalWriteFast(4, HIGH);
  usb_packet_t *event_packet;
  // printf("usb_mtp_sendEvent: called %x %x\n", (uint32_t)buffer, len);
  struct MTPContainer {
    uint32_t len;            // 0
    uint16_t type;           // 4
    uint16_t op;             // 6
    uint32_t transaction_id; // 8
    uint32_t params[5];      // 12
  } __attribute__((__may_alias__));

  //const MTPContainer *pe = (const MTPContainer *)buffer;
  // printf("  op:%x len:%d type:%d tid:%d Params:  ", pe->op, pe->len,
  // pe->type, pe->transaction_id);
  //if(pe->len>12) printf(" %x", pe->params[0]);
  //if(pe->len>16) printf(" %x", pe->params[1]);
  //if(pe->len>20) printf(" %x", pe->params[2]);
  //printf("\n");

  if (!usb_configuration)
    return -1;
  elapsedMillis em = 0;
  while (1) {
    if (!usb_configuration) {
//      digitalWriteFast(4, LOW);
      return -1;
    }
    if (usb_tx_packet_count(MTP_EVENT_ENDPOINT) < EVENT_TX_PACKET_LIMIT) {
      event_packet = usb_malloc();
      if (event_packet)
        break;
    }
    if (em > timeout) {
//    digitalWriteFast(4, LOW);
      return -1;
    }
    yield();
  }

  memcpy(event_packet->buf, buffer, len);
  event_packet->len = len;
  usb_tx(MTP_EVENT_ENDPOINT, event_packet);
//  digitalWriteFast(4, LOW);
  return len;
}
}

#elif defined(__IMXRT1062__)
// keep this here until cores is upgraded

#include "usb_mtp.h"
extern "C" {
static transfer_t tx_event_transfer[1] __attribute__((used, aligned(32)));
static uint8_t tx_event_buffer[MTP_EVENT_SIZE]
    __attribute__((used, aligned(32)));

int usb_init_events(void) { // T4
  // usb_config_tx(MTP_EVENT_ENDPOINT, MTP_EVENT_SIZE, 0, txEvent_event);
  //
  // usb_config_rx(MTP_EVENT_ENDPOINT, MTP_EVENT_SIZE, 0, rxEvent_event);
  // usb_prepare_transfer(rx_event_transfer + 0, rx_event_buffer,
  // MTP_EVENT_SIZE, 0);
  // usb_receive(MTP_EVENT_ENDPOINT, rx_event_transfer + 0);
  return 1;
}

static int usb_mtp_wait(transfer_t *xfer, uint32_t timeout) { // T4
  uint32_t wait_begin_at = systick_millis_count;
  while (1) {
    if (!usb_configuration)
      return -1; // usb not enumerated by host
    uint32_t status = usb_transfer_status(xfer);
    if (!(status & 0x80))
      break; // transfer descriptor ready
    if (systick_millis_count - wait_begin_at > timeout)
      return 0;
    yield();
  }
  return 1;
}

int usb_mtp_recvEvent(void *buffer, uint32_t len, uint32_t timeout) { // T4
#if 0
      int ret= usb_mtp_wait(rx_event_transfer, timeout); if(ret<=0) return ret;

      memcpy(buffer, rx_event_buffer, len);
      memset(rx_event_transfer, 0, sizeof(rx_event_transfer));

      NVIC_DISABLE_IRQ(IRQ_USB1);
      usb_prepare_transfer(rx_event_transfer + 0, rx_event_buffer, MTP_EVENT_SIZE, 0);
      usb_receive(MTP_EVENT_ENDPOINT, rx_event_transfer + 0);
      NVIC_ENABLE_IRQ(IRQ_USB1);
#endif
  return MTP_EVENT_SIZE;
}

int usb_mtp_sendEvent(const void *buffer, uint32_t len, uint32_t timeout) { // T4
  transfer_t *xfer = tx_event_transfer;
  int ret = usb_mtp_wait(xfer, timeout);
  if (ret <= 0)
    return ret;

  uint8_t *eventdata = tx_event_buffer;
  memcpy(eventdata, buffer, len);
  usb_prepare_transfer(xfer, eventdata, len, 0);
  usb_transmit(MTP_EVENT_ENDPOINT, xfer);
  return len;
}
}  // extern "C"

#endif // __IMXRT1062__



const uint32_t EVENT_TIMEOUT = 60;

int MTPD::send_Event(uint16_t eventCode) {
  printf("*MTPD::send_Event(%x)\n", eventCode);
  MTPContainer event;
  event.len = 12;
  event.op = eventCode;
  event.type = MTP_CONTAINER_TYPE_EVENT;
  event.transaction_id = TID;
  event.params[0] = 0;
  event.params[1] = 0;
  event.params[2] = 0;
  return usb_mtp_sendEvent((const void *)&event, event.len, EVENT_TIMEOUT);
}
int MTPD::send_Event(uint16_t eventCode, uint32_t p1) {
  printf("*MTPD::send_Event(%x) %x\n", eventCode, p1);
  MTPContainer event;
  event.len = 16;
  event.op = eventCode;
  event.type = MTP_CONTAINER_TYPE_EVENT;
  event.transaction_id = TID;
  event.params[0] = p1;
  event.params[1] = 0;
  event.params[2] = 0;
  return usb_mtp_sendEvent((const void *)&event, event.len, EVENT_TIMEOUT);
}
int MTPD::send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2) {
  printf("*MTPD::send_Event(%x) %x %x\n", eventCode, p1, p2);
  MTPContainer event;
  event.len = 20;
  event.op = eventCode;
  event.type = MTP_CONTAINER_TYPE_EVENT;
  event.transaction_id = TID;
  event.params[0] = p1;
  event.params[1] = p2;
  event.params[2] = 0;
  return usb_mtp_sendEvent((const void *)&event, event.len, EVENT_TIMEOUT);
}
int MTPD::send_Event(uint16_t eventCode, uint32_t p1, uint32_t p2,
                     uint32_t p3) {
  MTPContainer event;
  event.len = 24;
  event.op = eventCode;
  event.type = MTP_CONTAINER_TYPE_EVENT;
  event.transaction_id = TID;
  event.params[0] = p1;
  event.params[1] = p2;
  event.params[2] = p3;
  return usb_mtp_sendEvent((const void *)&event, event.len, EVENT_TIMEOUT);
}

int MTPD::send_DeviceResetEvent(void) {
  storage_ids_sent_ = false;  // clear it for now
  return send_Event(MTP_EVENT_DEVICE_RESET);
}
// following WIP
int MTPD::send_StorageInfoChangedEvent(uint32_t p1) {
  return send_Event(MTP_EVENT_STORAGE_INFO_CHANGED, Store2Storage(p1));
}

// following not tested
int MTPD::send_addObjectEvent(uint32_t p1) {
  return send_Event(MTP_EVENT_OBJECT_ADDED, p1);
}
int MTPD::send_removeObjectEvent(uint32_t p1) {
  return send_Event(MTP_EVENT_OBJECT_REMOVED, p1);
}

int MTPD::send_StoreAddedEvent(uint32_t store) {
  if (!storage_ids_sent_) return 0; // Don't need to send.

  return send_Event(MTP_EVENT_STORE_ADDED, Store2Storage(store));
}

int MTPD::send_StoreRemovedEvent(uint32_t store) {
  if (!storage_ids_sent_) return 0; // Don't need to send.
  return send_Event(MTP_EVENT_STORE_REMOVED, Store2Storage(store));
}

bool MTPD::send_addObjectEvent(uint32_t store, const char *pathname) {
  bool node_added = false;
  uint32_t handle =
      storage_->MapFileNameToIndex(store, pathname, true, &node_added);
  printf("notifyFileCreated: %x:%x maps to handle: %x\n", store, pathname,
         handle);
  if (handle != 0xFFFFFFFFUL) {
    send_addObjectEvent(handle);
    return true;
  }
  return false;
}

bool MTPD::send_removeObjectEvent(uint32_t store, const char *pathname) {
  uint32_t handle =
      storage_->MapFileNameToIndex(store, pathname, false, nullptr);
  printf("notifyFileRemoved: %x:%x maps to handle: %x\n", store, pathname,
         handle);
  if (handle != 0xFFFFFFFFUL) {
    send_removeObjectEvent(handle);
    return true;
  }
  return false;
}

#endif // USE_EVENTS




void MTPD::printContainer(const void *container, const char *msg) {
  const struct MTPContainer *c = (const struct MTPContainer *)container;
#ifndef MTP_VERBOSE_PRINT_CONTAINER
  printf("%x %d %d %d: ", c->op, c->len, c->type, c->transaction_id);
  if (c->len > 12) {
    printf(" %x", c->params[0]);
  }
  if (c->len > 16) {
    printf(" %x", c->params[1]);
  }
  if (c->len > 20) {
    printf(" %x", c->params[2]);
  }
  printf("\n");
#else // MTP_VERBOSE_PRINT_CONTAINER
  int print_property_name = -1; // no
  if (msg) {
    printf("%s", msg);
    DBGPRINTF("%s", msg);
  }
  printf("%u ", millis());
  switch (c->type) {
  default:
    printf(" UNKWN: %x\n", c->type);
    DBGPRINTF("UNKWN: %x l:%d\n", c->op, c->len);
    //MemoryHexDump(*printStream_, (void*)c, 512, true);
    printf(" UNKWN: %x\n", c->type);  // print it again...
    break;
  case MTP_CONTAINER_TYPE_COMMAND:
    printf(F("CMD: "));
    DBGPRINTF("CMD: %x l:%d\n", c->op, c->len);
    break;
  case MTP_CONTAINER_TYPE_DATA:
    printf(F("DATA:"));
    DBGPRINTF("DATA: %x l:%d\n", c->op, c->len);
    break;
  case MTP_CONTAINER_TYPE_RESPONSE:
    printf(F("RESP:"));
    DBGPRINTF("RESP: %x l:%d\n", c->op, c->len);
    break;
  case MTP_CONTAINER_TYPE_EVENT:
    printf(F("EVENT: "));
    DBGPRINTF("EVENT: %x\n", c->op);
    break;
  }
  printf(F("%x"), c->op);
  switch (c->op) {
  case MTP_OPERATION_GET_DEVICE_INFO:
    printf(F("(GET_DEVICE_INFO)"));
    break;
  case MTP_OPERATION_OPEN_SESSION:
    printf(F("(OPEN_SESSION)"));
    break;
  case MTP_OPERATION_CLOSE_SESSION:
    printf(F("(CLOSE_SESSION)"));
    break;
  case MTP_OPERATION_GET_STORAGE_IDS:
    printf(F("(GET_STORAGE_IDS)"));
    break;
  case MTP_OPERATION_GET_STORAGE_INFO:
    printf(F("(GET_STORAGE_INFO)"));
    break;
  case MTP_OPERATION_GET_NUM_OBJECTS:
    printf(F("(GET_NUM_OBJECTS)"));
    break;
  case MTP_OPERATION_GET_OBJECT_HANDLES:
    printf(F("(GET_OBJECT_HANDLES)"));
    break;
  case MTP_OPERATION_GET_OBJECT_INFO:
    printf(F("(GET_OBJECT_INFO)"));
    break;
  case MTP_OPERATION_GET_OBJECT:
    printf(F("(GET_OBJECT)"));
    break;
  case MTP_OPERATION_GET_THUMB:
    printf(F("(GET_THUMB)"));
    break;
  case MTP_OPERATION_DELETE_OBJECT:
    printf(F("(DELETE_OBJECT)"));
    break;
  case MTP_OPERATION_SEND_OBJECT_INFO:
    printf(F("(SEND_OBJECT_INFO)"));
    break;
  case MTP_OPERATION_SEND_OBJECT:
    printf(F("(SEND_OBJECT)"));
    break;
  case MTP_OPERATION_INITIATE_CAPTURE:
    printf(F("(INITIATE_CAPTURE)"));
    break;
  case MTP_OPERATION_FORMAT_STORE:
    printf(F("(FORMAT_STORE)"));
    break;
  case MTP_OPERATION_RESET_DEVICE:
    printf(F("(RESET_DEVICE)"));
    break;
  case MTP_OPERATION_SELF_TEST:
    printf(F("(SELF_TEST)"));
    break;
  case MTP_OPERATION_SET_OBJECT_PROTECTION:
    printf(F("(SET_OBJECT_PROTECTION)"));
    break;
  case MTP_OPERATION_POWER_DOWN:
    printf(F("(POWER_DOWN)"));
    break;
  case MTP_OPERATION_GET_DEVICE_PROP_DESC:
    printf(F("(GET_DEVICE_PROP_DESC)"));
    break;
  case MTP_OPERATION_GET_DEVICE_PROP_VALUE:
    printf(F("(GET_DEVICE_PROP_VALUE)"));
    break;
  case MTP_OPERATION_SET_DEVICE_PROP_VALUE:
    printf(F("(SET_DEVICE_PROP_VALUE)"));
    break;
  case MTP_OPERATION_RESET_DEVICE_PROP_VALUE:
    printf(F("(RESET_DEVICE_PROP_VALUE)"));
    break;
  case MTP_OPERATION_TERMINATE_OPEN_CAPTURE:
    printf(F("(TERMINATE_OPEN_CAPTURE)"));
    break;
  case MTP_OPERATION_MOVE_OBJECT:
    printf(F("(MOVE_OBJECT)"));
    break;
  case MTP_OPERATION_COPY_OBJECT:
    printf(F("(COPY_OBJECT)"));
    break;
  case MTP_OPERATION_GET_PARTIAL_OBJECT:
    printf(F("(GET_PARTIAL_OBJECT)"));
    break;
  case MTP_OPERATION_INITIATE_OPEN_CAPTURE:
    printf(F("(INITIATE_OPEN_CAPTURE)"));
    break;
  case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
    printf(F("(GET_OBJECT_PROPS_SUPPORTED)"));
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_DESC:
    printf(F("(GET_OBJECT_PROP_DESC)"));
    print_property_name = 0;
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
    printf(F("(GET_OBJECT_PROP_VALUE)"));
    print_property_name = 1;
    break;
  case MTP_OPERATION_SET_OBJECT_PROP_VALUE:
    printf(F("(SET_OBJECT_PROP_VALUE)"));
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_LIST:
    printf(F("(GET_OBJECT_PROP_LIST)"));
    break;
  case MTP_OPERATION_SET_OBJECT_PROP_LIST:
    printf(F("(SET_OBJECT_PROP_LIST)"));
    break;
  case MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC:
    printf(F("(GET_INTERDEPENDENT_PROP_DESC)"));
    break;
  case MTP_OPERATION_SEND_OBJECT_PROP_LIST:
    printf(F("(SEND_OBJECT_PROP_LIST)"));
    break;
  case MTP_OPERATION_GET_OBJECT_REFERENCES:
    printf(F("(GET_OBJECT_REFERENCES)"));
    break;
  case MTP_OPERATION_SET_OBJECT_REFERENCES:
    printf(F("(SET_OBJECT_REFERENCES)"));
    break;
  case MTP_OPERATION_SKIP:
    printf(F("(SKIP)"));
    break;
  // RESPONSES
  case MTP_RESPONSE_UNDEFINED:
    printf(F("(RSP:UNDEFINED)"));
    break;
  case MTP_RESPONSE_OK:
    printf(F("(RSP:OK)"));
    break;
  case MTP_RESPONSE_GENERAL_ERROR:
    printf(F("(RSP:GENERAL_ERROR)"));
    break;
  case MTP_RESPONSE_SESSION_NOT_OPEN:
    printf(F("(RSP:SESSION_NOT_OPEN)"));
    break;
  case MTP_RESPONSE_INVALID_TRANSACTION_ID:
    printf(F("(RSP:INVALID_TRANSACTION_ID)"));
    break;
  case MTP_RESPONSE_OPERATION_NOT_SUPPORTED:
    printf(F("(RSP:OPERATION_NOT_SUPPORTED)"));
    break;
  case MTP_RESPONSE_PARAMETER_NOT_SUPPORTED:
    printf(F("(RSP:PARAMETER_NOT_SUPPORTED)"));
    break;
  case MTP_RESPONSE_INCOMPLETE_TRANSFER:
    printf(F("(RSP:INCOMPLETE_TRANSFER)"));
    break;
  case MTP_RESPONSE_INVALID_STORAGE_ID:
    printf(F("(RSP:INVALID_STORAGE_ID)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_HANDLE:
    printf(F("(RSP:INVALID_OBJECT_HANDLE)"));
    break;
  case MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED:
    printf(F("(RSP:DEVICE_PROP_NOT_SUPPORTED)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE:
    printf(F("(RSP:INVALID_OBJECT_FORMAT_CODE)"));
    break;
  case MTP_RESPONSE_STORAGE_FULL:
    printf(F("(RSP:STORAGE_FULL)"));
    break;
  case MTP_RESPONSE_OBJECT_WRITE_PROTECTED:
    printf(F("(RSP:OBJECT_WRITE_PROTECTED)"));
    break;
  case MTP_RESPONSE_STORE_READ_ONLY:
    printf(F("(RSP:STORE_READ_ONLY)"));
    break;
  case MTP_RESPONSE_ACCESS_DENIED:
    printf(F("(RSP:ACCESS_DENIED)"));
    break;
  case MTP_RESPONSE_NO_THUMBNAIL_PRESENT:
    printf(F("(RSP:NO_THUMBNAIL_PRESENT)"));
    break;
  case MTP_RESPONSE_SELF_TEST_FAILED:
    printf(F("(RSP:SELF_TEST_FAILED)"));
    break;
  case MTP_RESPONSE_PARTIAL_DELETION:
    printf(F("(RSP:PARTIAL_DELETION)"));
    break;
  case MTP_RESPONSE_STORE_NOT_AVAILABLE:
    printf(F("(RSP:STORE_NOT_AVAILABLE)"));
    break;
  case MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED:
    printf(F("(RSP:SPECIFICATION_BY_FORMAT_UNSUPPORTED)"));
    break;
  case MTP_RESPONSE_NO_VALID_OBJECT_INFO:
    printf(F("(RSP:NO_VALID_OBJECT_INFO)"));
    break;
  case MTP_RESPONSE_INVALID_CODE_FORMAT:
    printf(F("(RSP:INVALID_CODE_FORMAT)"));
    break;
  case MTP_RESPONSE_UNKNOWN_VENDOR_CODE:
    printf(F("(RSP:UNKNOWN_VENDOR_CODE)"));
    break;
  case MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED:
    printf(F("(RSP:CAPTURE_ALREADY_TERMINATED)"));
    break;
  case MTP_RESPONSE_DEVICE_BUSY:
    printf(F("(RSP:DEVICE_BUSY)"));
    break;
  case MTP_RESPONSE_INVALID_PARENT_OBJECT:
    printf(F("(RSP:INVALID_PARENT_OBJECT)"));
    break;
  case MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT:
    printf(F("(RSP:INVALID_DEVICE_PROP_FORMAT)"));
    break;
  case MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE:
    printf(F("(RSP:INVALID_DEVICE_PROP_VALUE)"));
    break;
  case MTP_RESPONSE_INVALID_PARAMETER:
    printf(F("(RSP:INVALID_PARAMETER)"));
    break;
  case MTP_RESPONSE_SESSION_ALREADY_OPEN:
    printf(F("(RSP:SESSION_ALREADY_OPEN)"));
    break;
  case MTP_RESPONSE_TRANSACTION_CANCELLED:
    printf(F("(RSP:TRANSACTION_CANCELLED)"));
    break;
  case MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED:
    printf(F("(RSP:SPECIFICATION_OF_DESTINATION_UNSUPPORTED)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_PROP_CODE:
    printf(F("(RSP:INVALID_OBJECT_PROP_CODE)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT:
    printf(F("(RSP:INVALID_OBJECT_PROP_FORMAT)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE:
    printf(F("(RSP:INVALID_OBJECT_PROP_VALUE)"));
    break;
  case MTP_RESPONSE_INVALID_OBJECT_REFERENCE:
    printf(F("(RSP:INVALID_OBJECT_REFERENCE)"));
    break;
  case MTP_RESPONSE_GROUP_NOT_SUPPORTED:
    printf(F("(RSP:GROUP_NOT_SUPPORTED)"));
    break;
  case MTP_RESPONSE_INVALID_DATASET:
    printf(F("(RSP:INVALID_DATASET)"));
    break;
  case MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED:
    printf(F("(RSP:SPECIFICATION_BY_GROUP_UNSUPPORTED)"));
    break;
  case MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED:
    printf(F("(RSP:SPECIFICATION_BY_DEPTH_UNSUPPORTED)"));
    break;
  case MTP_RESPONSE_OBJECT_TOO_LARGE:
    printf(F("(RSP:OBJECT_TOO_LARGE)"));
    break;
  case MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED:
    printf(F("(RSP:OBJECT_PROP_NOT_SUPPORTED)"));
    break;
  case MTP_EVENT_UNDEFINED:
    printf(F("(EVT:UNDEFINED)"));
    break;
  case MTP_EVENT_CANCEL_TRANSACTION:
    printf(F("(EVT:CANCEL_TRANSACTION)"));
    break;
  case MTP_EVENT_OBJECT_ADDED:
    printf(F("(EVT:OBJECT_ADDED)"));
    break;
  case MTP_EVENT_OBJECT_REMOVED:
    printf(F("(EVT:OBJECT_REMOVED)"));
    break;
  case MTP_EVENT_STORE_ADDED:
    printf(F("(EVT:STORE_ADDED)"));
    break;
  case MTP_EVENT_STORE_REMOVED:
    printf(F("(EVT:STORE_REMOVED)"));
    break;
  case MTP_EVENT_DEVICE_PROP_CHANGED:
    printf(F("(EVT:DEVICE_PROP_CHANGED)"));
    break;
  case MTP_EVENT_OBJECT_INFO_CHANGED:
    printf(F("(EVT:OBJECT_INFO_CHANGED)"));
    break;
  case MTP_EVENT_DEVICE_INFO_CHANGED:
    printf(F("(EVT:DEVICE_INFO_CHANGED)"));
    break;
  case MTP_EVENT_REQUEST_OBJECT_TRANSFER:
    printf(F("(EVT:REQUEST_OBJECT_TRANSFER)"));
    break;
  case MTP_EVENT_STORE_FULL:
    printf(F("(EVT:STORE_FULL)"));
    break;
  case MTP_EVENT_DEVICE_RESET:
    printf(F("(EVT:DEVICE_RESET)"));
    break;
  case MTP_EVENT_STORAGE_INFO_CHANGED:
    printf(F("(EVT:STORAGE_INFO_CHANGED)"));
    break;
  case MTP_EVENT_CAPTURE_COMPLETE:
    printf(F("(EVT:CAPTURE_COMPLETE)"));
    break;
  case MTP_EVENT_UNREPORTED_STATUS:
    printf(F("(EVT:UNREPORTED_STATUS)"));
    break;
  case MTP_EVENT_OBJECT_PROP_CHANGED:
    printf(F("(EVT:OBJECT_PROP_CHANGED)"));
    break;
  case MTP_EVENT_OBJECT_PROP_DESC_CHANGED:
    printf(F("(EVT:OBJECT_PROP_DESC_CHANGED)"));
    break;
  case MTP_EVENT_OBJECT_REFERENCES_CHANGED:
    printf(F("(EVT:OBJECT_REFERENCES_CHANGED)"));
    break;
  }
  printf("l: %d", c->len);
  printf(F(" T:%x"), c->transaction_id);
  if (c->len >= 16)
    printf(F(" : %x"), c->params[0]);
  if (c->len >= 20)
    printf(F(" %x"), c->params[1]);
  if (c->len >= 24)
    printf(F(" %x"), c->params[2]);
  if (c->len >= 28)
    printf(F(" %x"), c->params[3]);
  if (c->len >= 32)
    printf(F(" %x"), c->params[4]);
  if (print_property_name >= 0) {
    switch (c->params[print_property_name]) {
    case MTP_PROPERTY_STORAGE_ID:
      printf(" (STORAGE_ID)");
      break;
    case MTP_PROPERTY_OBJECT_FORMAT:
      printf(" (FORMAT)");
      break;
    case MTP_PROPERTY_PROTECTION_STATUS:
      printf(" (PROTECTION)");
      break;
    case MTP_PROPERTY_OBJECT_SIZE:
      printf(" (SIZE)");
      break;
    case MTP_PROPERTY_OBJECT_FILE_NAME:
      printf(" (OBJECT NAME)");
      break;
    case MTP_PROPERTY_DATE_CREATED:
      printf(" (CREATED)");
      break;
    case MTP_PROPERTY_DATE_MODIFIED:
      printf(" (MODIFIED)");
      break;
    case MTP_PROPERTY_PARENT_OBJECT:
      printf(" (PARENT)");
      break;
    case MTP_PROPERTY_PERSISTENT_UID:
      printf(" (PERSISTENT_UID)");
      break;
    case MTP_PROPERTY_NAME:
      printf(" (NAME)");
      break;
    }
  }
  printf("\n");
}
#endif // MTP_VERBOSE_PRINT_CONTAINER

#endif // USB_MTPDISK
