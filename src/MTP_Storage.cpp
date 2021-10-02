// Storage.cpp - Teensy MTP Responder library
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
// Nov 2020 adapted to SdFat-beta / SD combo

#include "core_pins.h"
#include "usb_dev.h"
#include "usb_serial.h"

#include "MTP_Teensy.h"
#include "MTP_Storage.h"

#define DEBUG 0

#if DEBUG > 0
#define USE_DBG_MACROS 1
#else
#define USE_DBG_MACROS 0
#endif

#define DBG_FILE "Storage.cpp"

#if USE_DBG_MACROS == 1
static void dbgPrint(uint16_t line) {
  MTPD::PrintStream()->print(F("DBG_FAIL: "));
  MTPD::PrintStream()->print(F(DBG_FILE));
  MTPD::PrintStream()->write('.');
  MTPD::PrintStream()->println(line);
}

#define DBG_PRINT_IF(b)                                                        \
  if (b) {                                                                     \
    MTPD::PrintStream()->print(F(__FILE__));                                   \
    MTPD::PrintStream()->println(__LINE__);                                    \
  }
#define DBG_HALT_IF(b)                                                         \
  if (b) {                                                                     \
    MTPD::PrintStream()->print(F("DBG_HALT "));                                \
    MTPD::PrintStream()->print(F(__FILE__));                                   \
    MTPD::PrintStream()->println(__LINE__);                                    \
    while (true) {                                                             \
    }                                                                          \
  }
#define DBG_FAIL_MACRO dbgPrint(__LINE__);
#else // USE_DBG_MACROS
#define DBG_FAIL_MACRO
#define DBG_PRINT_IF(b)
#define DBG_HALT_IF(b)
#endif // USE_DBG_MACROS

#define sd_isOpen(x) (x)
#define sd_getName(x, y, n) strlcpy(y, x.name(), n)

#define indexFile "/mtpindex.dat"
// TODO:
//   support serialflash
//   partial object fetch/receive
//   events (notify usb host when local storage changes) (But, this seems too
//   difficult)

// These should probably be weak.
void mtp_yield() {}
void mtp_lock_storage(bool lock) {}

bool MTPStorage_SD::readonly(uint32_t store) { return false; }
bool MTPStorage_SD::has_directories(uint32_t store) { return true; }

// default implementions...
uint64_t MTPStorageInterfaceCB::totalSizeCB(MTPStorage_SD *mtpstorage,
                                            uint32_t store,
                                            uint32_t user_token) {
  return mtpstorage->sd_totalSize(store);
}
uint64_t MTPStorageInterfaceCB::usedSizeCB(MTPStorage_SD *mtpstorage,
                                           uint32_t store,
                                           uint32_t user_token) {
  return mtpstorage->sd_usedSize(store);
}

uint64_t MTPStorage_SD::totalSize(uint32_t store) {
  MTPStorageInterfaceCB *callbacks = sd_getCallback(store);
  if (callbacks) {
    uint32_t user_token = sd_getUserToken(store);
    return callbacks->totalSizeCB(this, store, user_token);
  }
  return sd_totalSize(store);
}

uint8_t MTPStorage_SD::formatStore(uint32_t store, uint32_t p2,
                                   bool post_process) {
  MTPStorageInterfaceCB *callbacks = sd_getCallback(store);
  if (callbacks) {
    uint32_t user_token = sd_getUserToken(store);
    return callbacks->formatStore(this, store, user_token, p2, post_process);
  }
  return MTPStorageInterfaceCB::FORMAT_NOT_SUPPORTED;
}

uint64_t MTPStorage_SD::usedSize(uint32_t store) {
  MTPStorageInterfaceCB *callback = sd_getCallback(store);
  if (callback) {
    uint32_t user_token = sd_getUserToken(store);
    return callback->usedSizeCB(this, store, user_token);
  }
  return sd_usedSize(store);
}

void MTPStorage_SD::setIndexFile(File *index_file) {
  CloseIndex(); // close off any
  if (index_file) {
    index_ = *index_file;
    user_index_file_ = true;
  } else {
    user_index_file_ = false;
  }
}

bool MTPStorage_SD::setIndexStore(uint32_t storage) {
  if (storage >= sd_getFSCount())
    return false; // out of range
  CloseIndex();
  index_file_storage_ = storage;
  user_index_file_ = false;
  return true;
}

void MTPStorage_SD::CloseIndex() {
  mtp_lock_storage(true);
  if (user_index_file_) {
    // maybe truncate the file
    index_.seek(0, SeekSet);
    index_.truncate();
  } else if (sd_isOpen(index_))
    index_.close();

  mtp_lock_storage(false);
  index_generated = false;
  index_entries_ = 0;
}

void MTPStorage::CloseIndex()
{
	mtp_lock_storage(true);
	if (user_index_file_) {
		// maybe truncate the file
		index_.seek(0, SeekSet);
		index_.truncate();
	} else if (sd_isOpen(index_)) {
		index_.close();
	}
	mtp_lock_storage(false);
	index_generated = false;
	index_entries_ = 0;
}


void MTPStorage_SD::OpenIndex() {
  if (sd_isOpen(index_))
    return; // only once
  mtp_lock_storage(true);
  index_ = sd_open(index_file_storage_, indexFile, FILE_WRITE_BEGIN);
  if (!index_)
    MTPD::PrintStream()->println("cannot open Index file");
  mtp_lock_storage(false);
  user_index_file_ = false; // opened up default file so make sure off
}

void MTPStorage::OpenIndex()
{
	if (index_) return; // only once
	mtp_lock_storage(true);
	index_ = open(index_file_storage_, indexFile, FILE_WRITE_BEGIN);
	if (!index_) MTPD::PrintStream()->println("cannot open Index file");
	mtp_lock_storage(false);
	user_index_file_ = false; // opened up default file so make sure off
}

void MTPStorage_SD::ResetIndex() {
  if (!sd_isOpen(index_))
    return;
  CloseIndex();
  //    OpenIndex();

  all_scanned_ = false;
  open_file_ = 0xFFFFFFFEUL;
}

void MTPStorage::ResetIndex()
{
	if (!index_) return;
	CloseIndex();
	all_scanned_ = false;
	open_file_ = 0xFFFFFFFEUL;
}


void MTPStorage_SD::WriteIndexRecord(uint32_t i, const Record &r) {
  OpenIndex();
  mtp_lock_storage(true);
  index_.seek((sizeof(r) * i));
  index_.write((char *)&r, sizeof(r));
  mtp_lock_storage(false);
}

void MTPStorage::WriteIndexRecord(uint32_t i, const Record &r)
{
	OpenIndex();
	mtp_lock_storage(true);
	index_.seek((sizeof(r) * i));
	index_.write((char *)&r, sizeof(r));
	mtp_lock_storage(false);
}

uint32_t MTPStorage_SD::AppendIndexRecord(const Record &r) {
  uint32_t new_record = index_entries_++;
  WriteIndexRecord(new_record, r);
  return new_record;
}

uint32_t MTPStorage::AppendIndexRecord(const Record &r)
{
	uint32_t new_record = index_entries_++;
	WriteIndexRecord(new_record, r);
	return new_record;
}

// TODO(hubbe): Cache a few records for speed.
Record MTPStorage_SD::ReadIndexRecord(uint32_t i) {
  Record ret;
  memset(&ret, 0, sizeof(ret));
  if (i > index_entries_) {
    memset(&ret, 0, sizeof(ret));
    return ret;
  }
  OpenIndex();
  mtp_lock_storage(true);
  index_.seek(sizeof(ret) * i);
  index_.read((char *)&ret, sizeof(ret));
  mtp_lock_storage(false);

  return ret;
}

// TODO(hubbe): Cache a few records for speed.
Record MTPStorage::ReadIndexRecord(uint32_t i)
{
	Record ret;
	//memset(&ret, 0, sizeof(ret));
	if (i > index_entries_) {
		memset(&ret, 0, sizeof(ret));
		return ret;
	}
	OpenIndex();
	mtp_lock_storage(true);
	index_.seek(sizeof(ret) * i);
	index_.read((char *)&ret, sizeof(ret));
	mtp_lock_storage(false);
	return ret;
}


uint16_t
MTPStorage_SD::ConstructFilename(int i, char *out,
                                 int len) // construct filename rexursively
{
  Record tmp = ReadIndexRecord(i);

  if (tmp.parent == 0xFFFFFFFFUL) // flags the root object
  {
    strcpy(out, "/");
    return tmp.store;
  } else {
    ConstructFilename(tmp.parent, out, len);
    if (out[strlen(out) - 1] != '/')
      strlcat(out, "/", len);
    strlcat(out, tmp.name, len);
    return tmp.store;
  }
}

// construct filename rexursively
uint16_t MTPStorage::ConstructFilename(int i, char *out, int len)
{
	Record tmp = ReadIndexRecord(i);
	if (tmp.parent == 0xFFFFFFFFUL) { // flags the root object
		strcpy(out, "/");
		return tmp.store;
	} else {
		ConstructFilename(tmp.parent, out, len);
		if (out[strlen(out) - 1] != '/') {
			strlcat(out, "/", len);
		}
		strlcat(out, tmp.name, len);
		return tmp.store;
	}
}


void MTPStorage_SD::OpenFileByIndex(uint32_t i, uint32_t mode) {
  bool file_is_open = sd_isOpen(file_);  // check to see if file is open
  if (file_is_open && (open_file_ == i) && (mode_ == mode))
    return;
  char filename[MAX_FILENAME_LEN];
  uint16_t store = ConstructFilename(i, filename, MAX_FILENAME_LEN);

  mtp_lock_storage(true);
  if (file_is_open)
    file_.close();
  file_ = sd_open(store, filename, mode);
  if (!sd_isOpen(file_)) {
    MTPD::PrintStream()->printf(
        "OpenFileByIndex failed to open (%u):%s mode: %u\n", i, filename, mode);
    open_file_ = 0xFFFFFFFEUL;
  } else {
    open_file_ = i;
    mode_ = mode;
  }
  mtp_lock_storage(false);
}

void MTPStorage::OpenFileByIndex(uint32_t i, uint32_t mode)
{
	bool file_is_open = sd_isOpen(file_);  // check to see if file is open
	if (file_is_open && (open_file_ == i) && (mode_ == mode)) {
		return;
	}
	char filename[MAX_FILENAME_LEN];
	uint16_t store = ConstructFilename(i, filename, MAX_FILENAME_LEN);
	mtp_lock_storage(true);
	if (file_is_open) {
		file_.close();
	}
	file_ = open(store, filename, mode);
	if (!sd_isOpen(file_)) {
		MTPD::PrintStream()->printf(
			"OpenFileByIndex failed to open (%u):%s mode: %u\n", i, filename, mode);
		open_file_ = 0xFFFFFFFEUL;
	} else {
		open_file_ = i;
		mode_ = mode;
	}
	mtp_lock_storage(false);
}


// MTP object handles should not change or be re-used during a session.
// This would be easy if we could just have a list of all files in memory.
// Since our RAM is limited, we'll keep the index in a file instead.
void MTPStorage_SD::GenerateIndex(uint32_t store) {
  if (index_generated)
    return;
  index_generated = true;
  // first remove old index file
  mtp_lock_storage(true);
  if (user_index_file_) {
    // maybe truncate the file
    index_.seek(0, SeekSet);
    index_.truncate();
  } else
    sd_remove(index_file_storage_, indexFile);
  mtp_lock_storage(false);

  num_storage = sd_getFSCount();

  index_entries_ = 0;
  Record r;
  for (int ii = 0; ii < num_storage; ii++) {
    r.store = ii;            //
    r.parent = 0xFFFFFFFFUL; //
    r.sibling = 0;
    r.child = 0;
    r.isdir = true;
    r.scanned = false;
#ifdef MTP_SUPPORT_MODIFY_DATE
    r.dtModify = 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
    r.dtCreate = 0;
#endif
    strcpy(r.name, "/");
    AppendIndexRecord(r);
  }
}

// MTP object handles should not change or be re-used during a session.
// This would be easy if we could just have a list of all files in memory.
// Since our RAM is limited, we'll keep the index in a file instead.
void MTPStorage::GenerateIndex(uint32_t store)
{
	if (index_generated) return;
	index_generated = true;
	// first remove old index file
	mtp_lock_storage(true);
	if (user_index_file_) {
		// maybe truncate the file
		index_.seek(0, SeekSet);
		index_.truncate();
	} else {
		remove(index_file_storage_, indexFile);
	}
	mtp_lock_storage(false);
	num_storage = getFSCount();
	index_entries_ = 0;
	Record r;
	for (int ii = 0; ii < num_storage; ii++) {
		r.store = ii;
		r.parent = 0xFFFFFFFFUL;
		r.sibling = 0;
		r.child = 0;
		r.isdir = true;
		r.scanned = false;
		r.dtModify = 0;
		r.dtCreate = 0;
		strcpy(r.name, "/");
		AppendIndexRecord(r);
	}
}




void MTPStorage_SD::ScanDir(uint32_t store, uint32_t i) {
  if (i == 0xFFFFFFFFUL)
    i = store;

  Record record = ReadIndexRecord(i);
  if (record.isdir && !record.scanned) {
    OpenFileByIndex(i);
    if (!sd_isOpen(file_))
      return;

    int sibling = 0;
    while (true) {
      mtp_lock_storage(true);
      child_ = file_.openNextFile();
      mtp_lock_storage(false);
      if (!sd_isOpen(child_))
        break;

      Record r;
      r.store = record.store;
      r.parent = i;
      r.sibling = sibling;
      r.isdir = child_.isDirectory();
      r.child = r.isdir ? 0 : (uint32_t)child_.size();
      r.scanned = false;
      sd_getName(child_, r.name, MAX_FILENAME_LEN);

#if defined(MTP_SUPPORT_MODIFY_DATE) || defined(MTP_SUPPORT_CREATE_DATE)
      DateTimeFields dtf;
#ifdef MTP_SUPPORT_MODIFY_DATE
      r.dtModify = child_.getModifyTime(dtf) ? makeTime(dtf) : 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
      r.dtCreate = child_.getCreateTime(dtf) ? makeTime(dtf) : 0;
#endif
#endif
      sibling = AppendIndexRecord(r);
      child_.close();
    }

    record.scanned = true;
    record.child = sibling;
    WriteIndexRecord(i, record);
  }
}

void MTPStorage::ScanDir(uint32_t store, uint32_t i)
{
	if (i == 0xFFFFFFFFUL) i = store;
	Record record = ReadIndexRecord(i);
	if (record.isdir && !record.scanned) {
		OpenFileByIndex(i);
		if (!sd_isOpen(file_)) return;
		int sibling = 0;
		while (true) {
			mtp_lock_storage(true);
			child_ = file_.openNextFile();
			mtp_lock_storage(false);
			if (!sd_isOpen(child_)) break;
			Record r;
			r.store = record.store;
			r.parent = i;
			r.sibling = sibling;
			r.isdir = child_.isDirectory();
			r.child = r.isdir ? 0 : (uint32_t)child_.size();
			r.scanned = false;
			strlcpy(r.name, child_.name(), MAX_FILENAME_LEN);
			DateTimeFields dtf;
			r.dtModify = child_.getModifyTime(dtf) ? makeTime(dtf) : 0;
			r.dtCreate = child_.getCreateTime(dtf) ? makeTime(dtf) : 0;
			sibling = AppendIndexRecord(r);
			child_.close();
		}
		record.scanned = true;
		record.child = sibling;
		WriteIndexRecord(i, record);
	}
}

void MTPStorage_SD::ScanAll(uint32_t store) {
  if (all_scanned_)
    return;
  all_scanned_ = true;

  GenerateIndex(store);
  for (uint32_t i = 0; i < index_entries_; i++)
    ScanDir(store, i);
}

void MTPStorage::ScanAll(uint32_t store)
{
	if (all_scanned_) return;
	all_scanned_ = true;
	GenerateIndex(store);
	for (uint32_t i = 0; i < index_entries_; i++) {
		ScanDir(store, i);
	}
}


void MTPStorage_SD::StartGetObjectHandles(uint32_t store, uint32_t parent) {
  GenerateIndex(store);
  if (parent) {
    if (parent == 0xFFFFFFFFUL)
      parent = store; // As per initizalization

    ScanDir(store, parent);
    follow_sibling_ = true;
    // Root folder?
    next_ = ReadIndexRecord(parent).child;
  } else {
    ScanAll(store);
    follow_sibling_ = false;
    next_ = 1;
  }
}

void MTPStorage::StartGetObjectHandles(uint32_t store, uint32_t parent)
{
	GenerateIndex(store);
	if (parent) {
		if (parent == 0xFFFFFFFFUL) {
			parent = store; // As per initizalization
		}
		ScanDir(store, parent);
		follow_sibling_ = true;
		// Root folder?
		next_ = ReadIndexRecord(parent).child;
	} else {
		ScanAll(store);
		follow_sibling_ = false;
		next_ = 1;
	}
}

uint32_t MTPStorage_SD::GetNextObjectHandle(uint32_t store) {
  while (true) {
    if (next_ == 0)
      return 0;

    int ret = next_;
    Record r = ReadIndexRecord(ret);
    if (follow_sibling_) {
      next_ = r.sibling;
    } else {
      next_++;
      if (next_ >= index_entries_)
        next_ = 0;
    }
    if (r.name[0])
      return ret;
  }
}

uint32_t MTPStorage::GetNextObjectHandle(uint32_t store)
{
	while (true) {
		if (next_ == 0) return 0;
		int ret = next_;
		Record r = ReadIndexRecord(ret);
		if (follow_sibling_) {
			next_ = r.sibling;
		} else {
			next_++;
			if (next_ >= index_entries_) next_ = 0;
		}
		if (r.name[0]) return ret;
	}
}

void MTPStorage_SD::GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
                                  uint32_t *parent, uint16_t *store) {
  Record r = ReadIndexRecord(handle);
  strcpy(name, r.name);
  *parent = r.parent;
  *size = r.isdir ? 0xFFFFFFFFUL : r.child;
  *store = r.store;
}

void MTPStorage::GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
                               uint32_t *parent, uint16_t *store)
{
	Record r = ReadIndexRecord(handle);
	strcpy(name, r.name);
	*parent = r.parent;
	*size = r.isdir ? 0xFFFFFFFFUL : r.child;
	*store = r.store;
}

uint32_t MTPStorage_SD::GetSize(uint32_t handle) {
  return ReadIndexRecord(handle).child;
}

uint32_t MTPStorage::GetSize(uint32_t handle) {
	return ReadIndexRecord(handle).child;
}

#ifdef MTP_SUPPORT_MODIFY_DATE
bool MTPStorage_SD::getModifyTime(uint32_t handle, uint32_t &dt) {
  Record r = ReadIndexRecord(handle);
  dt = r.dtModify;
  return (r.dtModify) ? true : false;
}

bool MTPStorage::getModifyTime(uint32_t handle, uint32_t &dt)
{
	Record r = ReadIndexRecord(handle);
	dt = r.dtModify;
	return (r.dtModify) ? true : false;
}
#endif

#ifdef MTP_SUPPORT_CREATE_DATE
bool MTPStorage_SD::getCreateTime(uint32_t handle, uint32_t &dt) {
  Record r = ReadIndexRecord(handle);
  dt = r.dtCreate;
  return (r.dtCreate) ? true : false;
}

bool MTPStorage::getCreateTime(uint32_t handle, uint32_t &dt)
{
  Record r = ReadIndexRecord(handle);
  dt = r.dtCreate;
  return (r.dtCreate) ? true : false;
}
#endif

static inline uint16_t MTPFS_DATE(uint16_t year, uint8_t month, uint8_t day) {
  year -= 1980;
  return year > 127 || month > 12 || day > 31 ? 0
                                              : year << 9 | month << 5 | day;
}
static inline uint16_t MTPFS_YEAR(uint16_t fatDate) {
  return 1980 + (fatDate >> 9);
}
static inline uint8_t MTPFS_MONTH(uint16_t fatDate) {
  return (fatDate >> 5) & 0XF;
}
static inline uint8_t MTPFS_DAY(uint16_t fatDate) { return fatDate & 0X1F; }
static inline uint16_t MTPFS_TIME(uint8_t hour, uint8_t minute,
                                  uint8_t second) {
  return hour > 23 || minute > 59 || second > 59
             ? 0
             : hour << 11 | minute << 5 | second >> 1;
}
static inline uint8_t MTPFS_HOUR(uint16_t fatTime) { return fatTime >> 11; }
static inline uint8_t MTPFS_MINUTE(uint16_t fatTime) {
  return (fatTime >> 5) & 0X3F;
}
static inline uint8_t MTPFS_SECOND(uint16_t fatTime) {
  return 2 * (fatTime & 0X1F);
}

bool MTPStorage_SD::updateDateTimeStamps(uint32_t handle, uint32_t dtCreated,
                                         uint32_t dtModified) {
#if defined(MTP_SUPPORT_MODIFY_DATE) || defined(MTP_SUPPORT_CREATE_DATE)
  Record r = ReadIndexRecord(handle);
  DateTimeFields dtf;
  OpenFileByIndex(handle, FILE_READ);
  if (!sd_isOpen(file_)) {
    MTPD::PrintStream()->printf(
        "MTPStorage_SD::updateDateTimeStamps failed to open file\n");
    return false;
  }
  mtp_lock_storage(true);

#if defined(MTP_SUPPORT_MODIFY_DATE)
  r.dtModify = dtModified;
  breakTime(dtModified, dtf);
  file_.setModifyTime(dtf);
#endif
#if defined(MTP_SUPPORT_CREATE_DATE)
  r.dtCreate = dtCreated;
  breakTime(dtCreated, dtf);
  file_.setCreateTime(dtf);
#endif

  WriteIndexRecord(handle, r);
  file_.close();
  mtp_lock_storage(false);
#endif
  return true;
}

bool MTPStorage::updateDateTimeStamps(uint32_t handle, uint32_t dtCreated, uint32_t dtModified)
{
	Record r = ReadIndexRecord(handle);
	DateTimeFields dtf;
	OpenFileByIndex(handle, FILE_READ);
	if (!file_) {
		MTPD::PrintStream()->printf(
			"MTPStorage::updateDateTimeStamps failed to open file\n");
		return false;
	}
	mtp_lock_storage(true);
	r.dtModify = dtModified;
	breakTime(dtModified, dtf);
	file_.setModifyTime(dtf);
	r.dtCreate = dtCreated;
	breakTime(dtCreated, dtf);
	file_.setCreateTime(dtf);
	WriteIndexRecord(handle, r);
	file_.close();
	mtp_lock_storage(false);
	return true;
}

void MTPStorage_SD::read(uint32_t handle, uint32_t pos, char *out,
                         uint32_t bytes) {
  OpenFileByIndex(handle);
  mtp_lock_storage(true);
  file_.seek(pos);
  file_.read(out, bytes);
  mtp_lock_storage(false);
}

void MTPStorage::read(uint32_t handle, uint32_t pos, char *out, uint32_t bytes)
{
	OpenFileByIndex(handle);
	mtp_lock_storage(true);
	file_.seek(pos);
	file_.read(out, bytes);
	mtp_lock_storage(false);
}

void MTPStorage_SD::removeFile(uint32_t store, char *file) {
  char tname[MAX_FILENAME_LEN];

  File f1 = sd_open(store, file, 0);
  if (f1.isDirectory()) {
    File f2;
    while (f2 = f1.openNextFile()) {
      sprintf(tname, "%s/%s", file, f2.name());
      if (f2.isDirectory())
        removeFile(store, tname);
      else
        sd_remove(store, tname);
    }
    sd_rmdir(store, file);
  } else {
    sd_remove(store, file);
  }
}

void MTPStorage::removeFile(uint32_t store, const char *file)
{
	char tname[MAX_FILENAME_LEN];

	File f1 = open(store, file, 0);
	if (f1.isDirectory()) {
		File f2;
		while (f2 = f1.openNextFile()) {
			sprintf(tname, "%s/%s", file, f2.name());
			if (f2.isDirectory()) {
				removeFile(store, tname);
			} else {
				remove(store, tname);
			}
		}
		rmdir(store, file);
	} else {
		remove(store, file);
	}
}

bool MTPStorage_SD::DeleteObject(uint32_t object) {
  if (object == 0xFFFFFFFFUL)
    return true; // don't do anything if trying to delete a root directory see
                 // below

  // first create full filename
  char filename[MAX_FILENAME_LEN];
  ConstructFilename(object, filename, MAX_FILENAME_LEN);

  Record r = ReadIndexRecord(object);

  // remove file from storage (assume it is always working)
  mtp_lock_storage(true);
  removeFile(r.store, filename);
  mtp_lock_storage(false);

  // mark object as deleted
  r.name[0] = 0;
  WriteIndexRecord(object, r);

  // update index file
  Record t = ReadIndexRecord(r.parent);
  if (t.child ==
      object) { // we are the jungest, simply relink parent to older sibling
    t.child = r.sibling;
    WriteIndexRecord(r.parent, t);
  } else { // link junger to older sibling
    // find junger sibling
    uint32_t is = t.child;
    Record x = ReadIndexRecord(is);
    while ((x.sibling != object)) {
      is = x.sibling;
      x = ReadIndexRecord(is);
    }
    // is points now to junder sibling
    x.sibling = r.sibling;
    WriteIndexRecord(is, x);
  }
  return 1;
}

bool MTPStorage::DeleteObject(uint32_t object)
{
	// don't do anything if trying to delete a root directory see below
	if (object == 0xFFFFFFFFUL) return true;

	// first create full filename
	char filename[MAX_FILENAME_LEN];
	ConstructFilename(object, filename, MAX_FILENAME_LEN);

	Record r = ReadIndexRecord(object);

	// remove file from storage (assume it is always working)
	mtp_lock_storage(true);
	removeFile(r.store, filename);
	mtp_lock_storage(false);

	// mark object as deleted
	r.name[0] = 0;
	WriteIndexRecord(object, r);

	// update index file
	Record t = ReadIndexRecord(r.parent);
	if (t.child == object) { // we are the jungest, simply relink parent to older sibling
		t.child = r.sibling;
		WriteIndexRecord(r.parent, t);
	} else { // link junger to older sibling
		// find junger sibling
		uint32_t is = t.child;
		Record x = ReadIndexRecord(is);
		while ((x.sibling != object)) {
			is = x.sibling;
			x = ReadIndexRecord(is);
		}
		// is points now to junder sibling
		x.sibling = r.sibling;
		WriteIndexRecord(is, x);
	}
	return 1;
}


uint32_t MTPStorage_SD::Create(uint32_t store, uint32_t parent, bool folder,
                               const char *filename) {
  uint32_t ret;
  if (parent == 0xFFFFFFFFUL)
    parent = store;
  Record p = ReadIndexRecord(parent);
  Record r;
  strlcpy(r.name, filename, MAX_FILENAME_LEN);
  r.store = p.store;
  r.parent = parent;
  r.child = 0;
  r.sibling = p.child;
  r.isdir = folder;
  // New folder is empty, scanned = true.
  r.scanned = 1;
  ret = p.child = AppendIndexRecord(r);
#ifdef MTP_SUPPORT_MODIFY_DATE
  r.dtModify = 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
  r.dtCreate = 0;
#endif
  WriteIndexRecord(parent, p);
  if (folder) {
    char filename[MAX_FILENAME_LEN];
    ConstructFilename(ret, filename, MAX_FILENAME_LEN);
    mtp_lock_storage(true);
    sd_mkdir(store, filename);
    mtp_lock_storage(false);
#if defined(MTP_SUPPORT_MODIFY_DATE) || defined(MTP_SUPPORT_CREATE_DATE)
    OpenFileByIndex(ret, FILE_READ);
    if (!sd_isOpen(file_))
      MTPD::PrintStream()->printf(
          "MTPStorage_SD::Create %s failed to open folder\n", filename);
    else {
#if defined(MTP_SUPPORT_MODIFY_DATE) || defined(MTP_SUPPORT_CREATE_DATE)
      DateTimeFields dtf;
#ifdef MTP_SUPPORT_MODIFY_DATE
      r.dtModify = file_.getModifyTime(dtf) ? makeTime(dtf) : 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
      r.dtModify = file_.getCreateTime(dtf) ? makeTime(dtf) : 0;
#endif
#endif
      // does not do any good if we don't save the data!
      WriteIndexRecord(ret, r);
      file_.close();
      open_file_ = 0xFFFFFFFEUL;
    }
#endif

  } else {
    OpenFileByIndex(ret, FILE_WRITE_BEGIN);
    // lets check to see if we opened the file or not...
    if (!sd_isOpen(file_)) {
      MTPD::PrintStream()->printf(
          "MTPStorage_SD::Create %s failed to create file\n", filename);
      DeleteObject(ret);  // note this will mark that new item as deleted...
      ret = 0xFFFFFFFFUL; // return an error code...
    } else {
#if defined(MTP_SUPPORT_MODIFY_DATE) || defined(MTP_SUPPORT_CREATE_DATE)
      DateTimeFields dtf;
#ifdef MTP_SUPPORT_MODIFY_DATE
      r.dtModify = file_.getModifyTime(dtf) ? makeTime(dtf) : 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
      r.dtModify = file_.getCreateTime(dtf) ? makeTime(dtf) : 0;
#endif
      // does not do any good if we don't save the data!
      WriteIndexRecord(ret, r);
#endif
    }
  }
#if DEBUG > 1
  MTPD::PrintStream()->print("Create ");
  MTPD::PrintStream()->print(ret);
  MTPD::PrintStream()->print(" ");
  MTPD::PrintStream()->print(store);
  MTPD::PrintStream()->print(" ");
  MTPD::PrintStream()->print(parent);
  MTPD::PrintStream()->print(" ");
#ifdef MTP_SUPPORT_CREATE_DATE
  MTPD::PrintStream()->print(folder);
  MTPD::PrintStream()->print(" ");
#endif
  MTPD::PrintStream()->print(r.createDate);
  MTPD::PrintStream()->print(" ");
#ifdef MTP_SUPPORT_MODIFY_DATE
  MTPD::PrintStream()->print(r.modifyDate);
  MTPD::PrintStream()->print(" ");
#endif
  MTPD::PrintStream()->println(filename);
#endif
  return ret;
}

uint32_t MTPStorage::Create(uint32_t store, uint32_t parent, bool folder, const char *filename)
{
	uint32_t ret;
	if (parent == 0xFFFFFFFFUL) parent = store;
	Record p = ReadIndexRecord(parent);
	Record r;
	strlcpy(r.name, filename, MAX_FILENAME_LEN);
	r.store = p.store;
	r.parent = parent;
	r.child = 0;
	r.sibling = p.child;
	r.isdir = folder;
	// New folder is empty, scanned = true.
	r.scanned = 1;
	ret = p.child = AppendIndexRecord(r);
	r.dtModify = 0;
	r.dtCreate = 0;
	WriteIndexRecord(parent, p);
	if (folder) {
		char filename[MAX_FILENAME_LEN];
		ConstructFilename(ret, filename, MAX_FILENAME_LEN);
		mtp_lock_storage(true);
		mkdir(store, filename);
		mtp_lock_storage(false);
		OpenFileByIndex(ret, FILE_READ);
		if (!file_) {
			MTPD::PrintStream()->printf(
				"MTPStorage::Create %s failed to open folder\n", filename);
		} else {
			DateTimeFields dtf;
			r.dtModify = file_.getModifyTime(dtf) ? makeTime(dtf) : 0;
			r.dtCreate = file_.getCreateTime(dtf) ? makeTime(dtf) : 0;
			// does not do any good if we don't save the data!
			WriteIndexRecord(ret, r);
			file_.close();
			open_file_ = 0xFFFFFFFEUL;
		}
	} else {
		OpenFileByIndex(ret, FILE_WRITE_BEGIN);
		// lets check to see if we opened the file or not...
		if (!file_) {
			MTPD::PrintStream()->printf(
				"MTPStorage::Create %s failed to create file\n", filename);
			DeleteObject(ret);  // note this will mark that new item as deleted...
			ret = 0xFFFFFFFFUL; // return an error code...
		} else {
			DateTimeFields dtf;
			r.dtModify = file_.getModifyTime(dtf) ? makeTime(dtf) : 0;
			r.dtCreate = file_.getCreateTime(dtf) ? makeTime(dtf) : 0;
			// does not do any good if we don't save the data!
			WriteIndexRecord(ret, r);
		}
	}
#if DEBUG > 1
	MTPD::PrintStream()->print("Create ");
	MTPD::PrintStream()->print(ret);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(store);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(parent);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(folder);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(r.createDate);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(r.modifyDate);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->println(filename);
#endif
	return ret;
}



size_t MTPStorage_SD::write(const char *data, uint32_t bytes) {
  mtp_lock_storage(true);
  size_t ret = file_.write(data, bytes);
  mtp_lock_storage(false);
  return ret;
}

size_t MTPStorage::write(const char *data, uint32_t bytes)
{
	mtp_lock_storage(true);
	size_t ret = file_.write(data, bytes);
	mtp_lock_storage(false);
	return ret;
}

void MTPStorage_SD::close() {
  mtp_lock_storage(true);
  uint32_t size = (uint32_t)file_.size();
  file_.close();
  mtp_lock_storage(false);
  //
  // update record with file size
  Record r = ReadIndexRecord(open_file_);
  r.child = size;
  WriteIndexRecord(open_file_, r);
  open_file_ = 0xFFFFFFFEUL;
}

void MTPStorage::close()
{
	mtp_lock_storage(true);
	uint32_t size = (uint32_t)file_.size();
	file_.close();
	mtp_lock_storage(false);
	// update record with file size
	Record r = ReadIndexRecord(open_file_);
	r.child = size;
	WriteIndexRecord(open_file_, r);
	open_file_ = 0xFFFFFFFEUL;
}

bool MTPStorage_SD::rename(uint32_t handle, const char *name) {
  char oldName[MAX_FILENAME_LEN];
  char newName[MAX_FILENAME_LEN];
  char temp[MAX_FILENAME_LEN];

  uint16_t store = ConstructFilename(handle, oldName, MAX_FILENAME_LEN);
  MTPD::PrintStream()->println(oldName);

  Record p1 = ReadIndexRecord(handle);
  strlcpy(temp, p1.name, MAX_FILENAME_LEN);
  strlcpy(p1.name, name, MAX_FILENAME_LEN);

  WriteIndexRecord(handle, p1);
  ConstructFilename(handle, newName, MAX_FILENAME_LEN);
  MTPD::PrintStream()->println(newName);

  if (sd_rename(store, oldName, newName))
    return true;

  // rename failed; undo index update
  strlcpy(p1.name, temp, MAX_FILENAME_LEN);
  WriteIndexRecord(handle, p1);
  return false;
}

bool MTPStorage::rename(uint32_t handle, const char *name)
{
	char oldName[MAX_FILENAME_LEN];
	char newName[MAX_FILENAME_LEN];
	char temp[MAX_FILENAME_LEN];

	uint16_t store = ConstructFilename(handle, oldName, MAX_FILENAME_LEN);
	MTPD::PrintStream()->println(oldName);

	Record p1 = ReadIndexRecord(handle);
	strlcpy(temp, p1.name, MAX_FILENAME_LEN);
	strlcpy(p1.name, name, MAX_FILENAME_LEN);

	WriteIndexRecord(handle, p1);
	ConstructFilename(handle, newName, MAX_FILENAME_LEN);
	MTPD::PrintStream()->println(newName);

	if (rename(store, oldName, newName)) return true;

	// rename failed; undo index update
	strlcpy(p1.name, temp, MAX_FILENAME_LEN);
	WriteIndexRecord(handle, p1);
	return false;
}

void MTPStorage_SD::dumpIndexList(void) {
  for (uint32_t ii = 0; ii < index_entries_; ii++) {
    Record p = ReadIndexRecord(ii);
    MTPD::PrintStream()->printf("%d: %d %d %d %d %d %u %u %s\n", ii, p.store,
                                p.isdir, p.parent, p.sibling, p.child,
                                p.dtCreate, p.dtModify, p.name);
  }
}

void MTPStorage::dumpIndexList(void)
{
	for (uint32_t ii = 0; ii < index_entries_; ii++) {
		Record p = ReadIndexRecord(ii);
		MTPD::PrintStream()->printf("%d: %d %d %d %d %d %u %u %s\n",
			ii, p.store, p.isdir, p.parent, p.sibling, p.child,
			p.dtCreate, p.dtModify, p.name);
	}
}

void MTPStorage_SD::printRecord(int h, Record *p) {
  MTPD::PrintStream()->printf("%d: %d %d %d %d %d\n", h, p->store, p->isdir,
                              p->parent, p->sibling, p->child);
}

void MTPStorage::printRecord(int h, Record *p)
{
	MTPD::PrintStream()->printf("%d: %d %d %d %d %d\n", h, p->store, p->isdir,
                              p->parent, p->sibling, p->child);
}

void MTPStorage_SD::printRecordIncludeName(int h, Record *p) {
  MTPD::PrintStream()->printf("%d: %d %d %d %d %d %d %s\n", h, p->store,
                              p->isdir, p->scanned, p->parent, p->sibling,
                              p->child, p->name);
}

void MTPStorage::printRecordIncludeName(int h, Record *p)
{
	MTPD::PrintStream()->printf("%d: %d %d %d %d %d %d %s\n", h, p->store,
                              p->isdir, p->scanned, p->parent, p->sibling,
                              p->child, p->name);
}

/*
 * //index list management for moving object around
 * p1 is record of handle
 * p2 is record of new dir
 * p3 is record of old dir
 *
 *  // remove from old direcory
 * if p3.child == handle  / handle is last in old dir
 *      p3.child = p1.sibling   / simply relink old dir
 *      save p3
 * else
 *      px record of p3.child
 *      while( px.sibling != handle ) update px = record of px.sibling
 *      px.sibling = p1.sibling
 *      save px
 *
 *  // add to new directory
 * p1.parent = new
 * p1.sibling = p2.child
 * p2.child = handle
 * save p1
 * save p2
 *
*/

bool MTPStorage_SD::move(uint32_t handle, uint32_t newStore,
                         uint32_t newParent) {
#if DEBUG > 1
  MTPD::PrintStream()->printf("%d -> %d %d\n", handle, newStore, newParent);
#endif
  if (newParent == 0xFFFFFFFFUL)
    newParent = newStore; // storage runs from 1, while record.store runs from 0

  Record p1 = ReadIndexRecord(handle);
  Record p2 = ReadIndexRecord(newParent);
  Record p3 = ReadIndexRecord(p1.parent);

  if (p1.isdir) {
    if (!p1.scanned) {
      ScanDir(p1.store, handle); // in case scan directory
      WriteIndexRecord(handle, p1);
    }
  }

  Record p1o = p1;
  Record p2o = p2;
  Record p3o = p3;

  char oldName[MAX_FILENAME_LEN];
  ConstructFilename(handle, oldName, MAX_FILENAME_LEN);

#if DEBUG > 1
  MTPD::PrintStream()->print(p1.store);
  MTPD::PrintStream()->print(": ");
  MTPD::PrintStream()->println(oldName);
  dumpIndexList();
#endif

  uint32_t jx = -1;
  Record pxo;

  // remove index from old parent
  Record px;
  if (p3.child == handle) {
    p3.child = p1.sibling;
    WriteIndexRecord(p1.parent, p3);
  } else {
    jx = p3.child;
    px = ReadIndexRecord(jx);
    pxo = px;
    while (handle != px.sibling) {
      jx = px.sibling;
      px = ReadIndexRecord(jx);
      pxo = px;
    }
    px.sibling = p1.sibling;
    WriteIndexRecord(jx, px);
  }

  // add to new parent
  p1.parent = newParent;
  p1.store = p2.store;
  p1.sibling = p2.child;
  p2.child = handle;
  WriteIndexRecord(handle, p1);
  WriteIndexRecord(newParent, p2);

  // now working on disk storage
  char newName[MAX_FILENAME_LEN];
  ConstructFilename(handle, newName, MAX_FILENAME_LEN);

#if DEBUG > 1
  MTPD::PrintStream()->print(p1.store);
  MTPD::PrintStream()->print(": ");
  MTPD::PrintStream()->println(newName);
  dumpIndexList();
#endif

  if (p1o.store ==
      p2o.store) { // do a simple rename (works for files and directories)
    if (sd_rename(p1o.store, oldName, newName))
      return true;
    else {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else if (!p1o.isdir) {
    if (sd_copy(p1o.store, oldName, p2o.store, newName)) {
      sd_remove(p2o.store, oldName);
      return true;
    } else {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else { // move directory cross mtp-disks
    if (sd_moveDir(p1o.store, oldName, p2o.store, newName))
      return true;
    else {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }

fail:
  // undo changes in index list
  if (jx < 0)
    WriteIndexRecord(p1.parent, p3o);
  else
    WriteIndexRecord(jx, pxo);
  WriteIndexRecord(handle, p1o);
  WriteIndexRecord(newParent, p2o);
  return false;
}


bool MTPStorage::move(uint32_t handle, uint32_t newStore, uint32_t newParent)
{
#if DEBUG > 1
	MTPD::PrintStream()->printf("MTPStorage::move %d -> %d %d\n", handle, newStore, newParent);
#endif
	if (newParent == 0xFFFFFFFFUL) {
		newParent = newStore; // storage runs from 1, while record.store runs from 0
	}
	Record p1 = ReadIndexRecord(handle);
	Record p2 = ReadIndexRecord(newParent);
	Record p3 = ReadIndexRecord(p1.parent);

	if (p1.isdir) {
		if (!p1.scanned) {
			ScanDir(p1.store, handle); // in case scan directory
			WriteIndexRecord(handle, p1);
		}
	}

	Record p1o = p1;
	Record p2o = p2;
	Record p3o = p3;

	char oldName[MAX_FILENAME_LEN];
	ConstructFilename(handle, oldName, MAX_FILENAME_LEN);

#if DEBUG > 1
	MTPD::PrintStream()->print(p1.store);
	MTPD::PrintStream()->print(": ");
	MTPD::PrintStream()->println(oldName);
	dumpIndexList();
#endif

	uint32_t jx = -1;
	Record pxo;

	// remove index from old parent
	Record px;
	if (p3.child == handle) {
		p3.child = p1.sibling;
		WriteIndexRecord(p1.parent, p3);
	} else {
		jx = p3.child;
		px = ReadIndexRecord(jx);
		pxo = px;
		while (handle != px.sibling) {
			jx = px.sibling;
			px = ReadIndexRecord(jx);
			pxo = px;
		}
		px.sibling = p1.sibling;
		WriteIndexRecord(jx, px);
	}

	// add to new parent
	p1.parent = newParent;
	p1.store = p2.store;
	p1.sibling = p2.child;
	p2.child = handle;
	WriteIndexRecord(handle, p1);
	WriteIndexRecord(newParent, p2);

	// now working on disk storage
	char newName[MAX_FILENAME_LEN];
	ConstructFilename(handle, newName, MAX_FILENAME_LEN);

#if DEBUG > 1
	MTPD::PrintStream()->print(p1.store);
	MTPD::PrintStream()->print(": ");
	MTPD::PrintStream()->println(newName);
	dumpIndexList();
#endif

	if (p1o.store == p2o.store) { // do a simple rename (works for files and directories)
		if (rename(p1o.store, oldName, newName)) {
			return true;
		} else {
			DBG_FAIL_MACRO;
			goto fail;
		}
	} else if (!p1o.isdir) {
		if (copy(p1o.store, oldName, p2o.store, newName)) {
			remove(p2o.store, oldName);
			return true;
		} else {
			DBG_FAIL_MACRO;
			goto fail;
		}
	} else { // move directory cross mtp-disks
		if (moveDir(p1o.store, oldName, p2o.store, newName)) {
			return true;
		} else {
			DBG_FAIL_MACRO;
			goto fail;
		}
	}
fail:
	// undo changes in index list
	if (jx < 0) {
		WriteIndexRecord(p1.parent, p3o);
	} else {
		WriteIndexRecord(jx, pxo);
	}
	WriteIndexRecord(handle, p1o);
	WriteIndexRecord(newParent, p2o);
	return false;
}

uint32_t MTPStorage_SD::copy(uint32_t handle, uint32_t newStore,
                             uint32_t newParent) {
  if (newParent == 0xFFFFFFFFUL)
    newParent = newStore;

  Record p1 = ReadIndexRecord(handle);
  Record p2 = ReadIndexRecord(newParent);

  uint32_t newHandle;
  if (p1.isdir) {
    ScanDir(p1.store + 1, handle);
    newHandle = Create(p2.store, newParent, p1.isdir, p1.name);
    CopyFiles(handle, p2.store, newHandle);
  } else {
    Record r;
    strlcpy(r.name, p1.name, MAX_FILENAME_LEN);
    r.store = p2.store;
    r.parent = newParent;
    r.child = 0;
    r.sibling = p2.child;
    r.isdir = 0;
    r.scanned = 0;
    newHandle = p2.child = AppendIndexRecord(r);
    WriteIndexRecord(newParent, p2);

    char oldfilename[MAX_FILENAME_LEN];
    char newfilename[MAX_FILENAME_LEN];
    uint32_t store0 = ConstructFilename(handle, oldfilename, MAX_FILENAME_LEN);
    uint32_t store1 =
        ConstructFilename(newHandle, newfilename, MAX_FILENAME_LEN);

    sd_copy(store0, oldfilename, store1, newfilename);
  }

  return newHandle;
}

uint32_t MTPStorage::copy(uint32_t handle, uint32_t newStore, uint32_t newParent)
{
	if (newParent == 0xFFFFFFFFUL) newParent = newStore;
	Record p1 = ReadIndexRecord(handle);
	Record p2 = ReadIndexRecord(newParent);
	uint32_t newHandle;
	if (p1.isdir) {
		ScanDir(p1.store + 1, handle);
		newHandle = Create(p2.store, newParent, p1.isdir, p1.name);
		CopyFiles(handle, p2.store, newHandle);
	} else {
		Record r;
		strlcpy(r.name, p1.name, MAX_FILENAME_LEN);
		r.store = p2.store;
		r.parent = newParent;
		r.child = 0;
		r.sibling = p2.child;
		r.isdir = 0;
		r.scanned = 0;
		newHandle = p2.child = AppendIndexRecord(r);
		WriteIndexRecord(newParent, p2);
		char oldfilename[MAX_FILENAME_LEN];
		char newfilename[MAX_FILENAME_LEN];
		uint32_t store0 = ConstructFilename(handle, oldfilename, MAX_FILENAME_LEN);
		uint32_t store1 = ConstructFilename(newHandle, newfilename, MAX_FILENAME_LEN);
		copy(store0, oldfilename, store1, newfilename);
	}
	return newHandle;
}

bool MTPStorage_SD::CopyFiles(uint32_t handle, uint32_t store,
                              uint32_t newHandle) { // assume handle and
                                                    // newHandle point to
                                                    // existing directories
  if (newHandle == 0xFFFFFFFFUL)
    newHandle = store;
#if DEBUG > 1
  MTPD::PrintStream()->printf("%d -> %d\n", handle, newHandle);
#endif

  Record p1 = ReadIndexRecord(handle);
  Record p2 = ReadIndexRecord(newHandle);
  uint32_t ix = p1.child;
  uint32_t iy = 0;
  while (ix) { // get child
    Record px = ReadIndexRecord(ix);
    Record py = px;
    py.store = p2.store;
    py.parent = newHandle;
    py.sibling = iy;
    iy = AppendIndexRecord(py);

    char oldfilename[MAX_FILENAME_LEN];
    char newfilename[MAX_FILENAME_LEN];
    ConstructFilename(ix, oldfilename, MAX_FILENAME_LEN);
    ConstructFilename(iy, newfilename, MAX_FILENAME_LEN);

    if (py.isdir) {
      sd_mkdir(py.store, newfilename);

      ScanDir(p1.store, ix);
      CopyFiles(ix, p2.store, iy);
    } else {
      sd_copy(p1.store, oldfilename, py.store, newfilename);
    }
    ix = px.sibling;
  }
  p2.child = iy;
  WriteIndexRecord(newHandle, p2);
  return true;
}


// assume handle and newHandle point to existing directories
bool MTPStorage::CopyFiles(uint32_t handle, uint32_t store, uint32_t newHandle)
{
	if (newHandle == 0xFFFFFFFFUL) {
		newHandle = store;
	}
#if DEBUG > 1
	MTPD::PrintStream()->printf("%d -> %d\n", handle, newHandle);
#endif
	Record p1 = ReadIndexRecord(handle);
	Record p2 = ReadIndexRecord(newHandle);
	uint32_t ix = p1.child;
	uint32_t iy = 0;
	while (ix) { // get child
		Record px = ReadIndexRecord(ix);
		Record py = px;
		py.store = p2.store;
		py.parent = newHandle;
		py.sibling = iy;
		iy = AppendIndexRecord(py);
		char oldfilename[MAX_FILENAME_LEN];
		char newfilename[MAX_FILENAME_LEN];
		ConstructFilename(ix, oldfilename, MAX_FILENAME_LEN);
		ConstructFilename(iy, newfilename, MAX_FILENAME_LEN);
		if (py.isdir) {
			mkdir(py.store, newfilename);
			ScanDir(p1.store, ix);
			CopyFiles(ix, p2.store, iy);
		} else {
			copy(p1.store, oldfilename, py.store, newfilename);
		}
		ix = px.sibling;
	}
	p2.child = iy;
	WriteIndexRecord(newHandle, p2);
	return true;
}



/************************************** mSD_Base
 * *******************************/
uint32_t mSD_Base::sd_addFilesystem(FS &fs, const char *name,
                                    MTPStorageInterfaceCB *callback,
                                    uint32_t user_token) {
  if (fsCount < MTPD_MAX_FILESYSTEMS) {
    sd_name[fsCount] = name;
    sdx[fsCount] = &fs;
    callbacks[fsCount] = callback;
    user_tokens[fsCount] = user_token;
    MTPD::PrintStream()->printf("sd_addFilesystem: %d %x %s %x %x\n", fsCount,
                                (uint32_t)&fs, name, (uint32_t)callback,
                                user_token);
    return fsCount++;
  }
  return 0xFFFFFFFFUL; // no room left
}

uint32_t MTPStorage::addFilesystem(FS &disk, const char *diskname)
{
	if (fsCount < MTPD_MAX_FILESYSTEMS) {
		name[fsCount] = diskname;
		fs[fsCount] = &disk;
		MTPD::PrintStream()->printf("addFilesystem: %d %s\n", fsCount, diskname);
		return fsCount++;
	}
	return 0xFFFFFFFFUL; // no room left
}

bool mSD_Base::sd_removeFilesystem(uint32_t store) {
  if ((store < (uint32_t)fsCount) && (sd_name[store])) {
    sd_name[store] = nullptr;
    sdx[store] = nullptr;
    return true;
  }
  return false;
  ;
}

bool MTPStorage::removeFilesystem(uint32_t store)
{
	if ((store < fsCount) && (name[store])) {
		name[store] = nullptr;
		fs[store] = nullptr;
		return true;
	}
	return false;
}

bool mSD_Base::sd_copy(uint32_t store0, char *oldfilename, uint32_t store1,
                       char *newfilename) {
  const int nbuf = 2048;
  char buffer[nbuf];
  int nd = -1;

#if DEBUG > 1
  MTPD::PrintStream()->print("From ");
  MTPD::PrintStream()->print(store0);
  MTPD::PrintStream()->print(": ");
  MTPD::PrintStream()->println(oldfilename);
  MTPD::PrintStream()->print("To   ");
  MTPD::PrintStream()->print(store1);
  MTPD::PrintStream()->print(": ");
  MTPD::PrintStream()->println(newfilename);
#endif

  File f1 = sd_open(store0, oldfilename, FILE_READ);
  if (!f1) {
    DBG_FAIL_MACRO;
    return false;
  }
  File f2 = sd_open(store1, newfilename, FILE_WRITE_BEGIN);
  if (!f2) {
    f1.close();
    {
      DBG_FAIL_MACRO;
      return false;
    }
  }

  while (f1.available() > 0) {
    nd = f1.read(buffer, nbuf);
    if (nd < 0)
      break; // read error
    f2.write(buffer, nd);
    if (nd < nbuf)
      break; // end of file
  }
  // close all files
  f1.close();
  f2.close();
  if (nd < 0) {
    DBG_FAIL_MACRO;
    return false;
  }
  return true;
}

bool MTPStorage::copy(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename)
{
	const int nbuf = 2048;
	char buffer[nbuf];
	int nd = -1;

#if DEBUG > 1
	MTPD::PrintStream()->print("MTPStorage::copy - From ");
	MTPD::PrintStream()->print(store0);
	MTPD::PrintStream()->print(": ");
	MTPD::PrintStream()->println(oldfilename);
	MTPD::PrintStream()->print("To   ");
	MTPD::PrintStream()->print(store1);
	MTPD::PrintStream()->print(": ");
	MTPD::PrintStream()->println(newfilename);
#endif

	File f1 = open(store0, oldfilename, FILE_READ);
	if (!f1) {
		DBG_FAIL_MACRO;
		return false;
	}
	File f2 = open(store1, newfilename, FILE_WRITE_BEGIN);
	if (!f2) {
		f1.close();
		DBG_FAIL_MACRO;
		return false;
	}
	while (f1.available() > 0) {
		nd = f1.read(buffer, nbuf);
		if (nd < 0) break; // read error
		f2.write(buffer, nd);
		if (nd < nbuf) break; // end of file
	}
	// close all files
	f1.close();
	f2.close();
	if (nd < 0) {
		DBG_FAIL_MACRO;
		return false;
	}
	return true;
}

bool mSD_Base::sd_moveDir(
    uint32_t store0, char *oldfilename, uint32_t store1,
    char *newfilename) { // old and new are directory paths

  char tmp0Name[MAX_FILENAME_LEN];
  char tmp1Name[MAX_FILENAME_LEN];

  if (!sd_mkdir(store1, newfilename)) {
    DBG_FAIL_MACRO;
    return false;
  }

  File f1 = sd_open(store0, oldfilename, FILE_READ);
  if (!f1) {
    DBG_FAIL_MACRO;
    return false;
  }
  {
    while (1) {
      strlcpy(tmp0Name, oldfilename, MAX_FILENAME_LEN);
      if (tmp0Name[strlen(tmp0Name) - 1] != '/')
        strlcat(tmp0Name, "/", MAX_FILENAME_LEN);

      strlcpy(tmp1Name, newfilename, MAX_FILENAME_LEN);
      if (tmp1Name[strlen(tmp1Name) - 1] != '/')
        strlcat(tmp1Name, "/", MAX_FILENAME_LEN);

      File f2 = f1.openNextFile();
      if (!f2)
        break;
      { // generate filenames
        strlcat(tmp0Name, f2.name(), MAX_FILENAME_LEN);
        strlcat(tmp1Name, f2.name(), MAX_FILENAME_LEN);

        if (f2.isDirectory()) {
          if (!sd_moveDir(store0, tmp0Name, store1, tmp1Name)) {
            DBG_FAIL_MACRO;
            return false;
          }
        } else {
          if (!sd_copy(store0, tmp0Name, store1, tmp1Name)) {
            DBG_FAIL_MACRO;
            return false;
          }
          if (!sd_remove(store0, tmp0Name)) {
            DBG_FAIL_MACRO;
            return false;
          }
        }
      }
    }
  }
  return sd_rmdir(store0, oldfilename);
}

// old and new are directory paths
bool MTPStorage::moveDir(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename)
{
	char tmp0Name[MAX_FILENAME_LEN];
	char tmp1Name[MAX_FILENAME_LEN];

	if (!mkdir(store1, newfilename)) {
		DBG_FAIL_MACRO;
		return false;
	}
	File f1 = open(store0, oldfilename, FILE_READ);
	if (!f1) {
		DBG_FAIL_MACRO;
		return false;
	}
	while (1) {
		strlcpy(tmp0Name, oldfilename, MAX_FILENAME_LEN);
		if (tmp0Name[strlen(tmp0Name) - 1] != '/') {
			strlcat(tmp0Name, "/", MAX_FILENAME_LEN);
		}
		strlcpy(tmp1Name, newfilename, MAX_FILENAME_LEN);
		if (tmp1Name[strlen(tmp1Name) - 1] != '/') {
			strlcat(tmp1Name, "/", MAX_FILENAME_LEN);
		}
		File f2 = f1.openNextFile();
		if (!f2) break; {
			// generate filenames
			strlcat(tmp0Name, f2.name(), MAX_FILENAME_LEN);
			strlcat(tmp1Name, f2.name(), MAX_FILENAME_LEN);
			if (f2.isDirectory()) {
				if (!moveDir(store0, tmp0Name, store1, tmp1Name)) {
					DBG_FAIL_MACRO;
					return false;
				}
			} else {
				if (!copy(store0, tmp0Name, store1, tmp1Name)) {
					DBG_FAIL_MACRO;
					return false;
				}
				if (!remove(store0, tmp0Name)) {
					DBG_FAIL_MACRO;
					return false;
				}
			}
		}
	}
	return rmdir(store0, oldfilename);
}





uint32_t MTPStorage_SD::MapFileNameToIndex(uint32_t storage,
                                           const char *pathname,
                                           bool addLastNode, bool *node_added) {
  const char *path_parser = pathname;
  MTPD::PrintStream()->printf(
      "MTPStorage_SD::MapFileNameToIndex %u %s add:%d\n", storage, pathname,
      addLastNode);
  // We will only walk as far as we have enumerated
  if (node_added)
    *node_added = false;
  if (!index_generated || (path_parser == nullptr) || (*path_parser == '\0'))
    return 0xFFFFFFFFUL; // no index
  char filename[MAX_FILENAME_LEN];
  Record record = ReadIndexRecord(storage);
  uint32_t index;

  printRecordIncludeName(storage, &record);
  for (;;) {
    if (!record.isdir || !record.scanned)
      return 0xFFFFFFFFUL; // This storage has not been scanned.
    // Copy the nex section of file name
    if (*path_parser == '/')
      path_parser++; // advance from the previous /
    char *psz = filename;
    while (*path_parser && (*path_parser != '/')) {
      *psz++ = *path_parser++;
    }
    *psz = '\0'; // terminate the string.

    // Now lets see if we can find this item in the record list.
    MTPD::PrintStream()->printf("Looking for: %s\n", filename);
    index = record.child;
    while (index) {
      record = ReadIndexRecord(index);
      printRecordIncludeName(index, &record);
      if (strcmp(filename, record.name) == 0)
        break; // found a match
      index = record.sibling;
    }

    if (index) {
      // found a match. return it.
      if (*path_parser == '\0') {
        MTPD::PrintStream()->printf("Found Node: %d\n", index);
        return index;
      }

    } else {
      // item not found
      MTPD::PrintStream()->println("Node Not found");

      if ((*path_parser != '\0') || !addLastNode)
        return 0xFFFFFFFFUL; // not found nor added

      // need to add item
      uint32_t parent = record.parent;
      record = ReadIndexRecord(parent);
      Record r; // could probably reuse the other one, but...
      ///////////
      // Question is should more data be passed in like is this a directory and
      // size
      // or should we ask for it now, and/or should we mark it that we have not
      // grabbed that
      // info and wait for someone to ask for it?

      strlcpy(r.name, filename, MAX_FILENAME_LEN);
      r.store = storage;
      r.parent = parent;
      r.child = 0;
      r.scanned = false;

      mtp_lock_storage(true);
      if (sd_isOpen(file_))
        file_.close();
      file_ = sd_open(storage, pathname, FILE_READ);
      mtp_lock_storage(false);

      if (sd_isOpen(file_)) {
        r.isdir = file_.isDirectory();
        if (!r.isdir)
          r.child = (uint32_t)file_.size();
        mtp_lock_storage(true);
        file_.close();
        mtp_lock_storage(false);
      } else {
        r.isdir = false;
      }
      r.sibling = record.child;
      // New folder is empty, scanned = true.
      index = record.child = AppendIndexRecord(r);
      WriteIndexRecord(parent, record);

      MTPD::PrintStream()->printf("New node created: %d\n", index);
      record = ReadIndexRecord(index);
      printRecordIncludeName(index, &record);
      if (node_added)
        *node_added = true;
      return index;
    }
  }
  //
  return 0xFFFFFFFFUL;
}



uint32_t MTPStorage::MapFileNameToIndex(uint32_t storage, const char *pathname,
                                        bool addLastNode, bool *node_added)
{
	const char *path_parser = pathname;
	MTPD::PrintStream()->printf(
		"MTPStorage_SD::MapFileNameToIndex %u %s add:%d\n",
		storage, pathname, addLastNode);
	// We will only walk as far as we have enumerated
	if (node_added) *node_added = false;
	if (!index_generated || (path_parser == nullptr) || (*path_parser == '\0')) {
		return 0xFFFFFFFFUL; // no index
	}
	char filename[MAX_FILENAME_LEN];
	Record record = ReadIndexRecord(storage);
	uint32_t index;

	printRecordIncludeName(storage, &record);
	for (;;) {
		if (!record.isdir || !record.scanned) {
			return 0xFFFFFFFFUL; // This storage has not been scanned.
		}
		// Copy the nex section of file name
		if (*path_parser == '/') {
			path_parser++; // advance from the previous /
		}
		char *psz = filename;
		while (*path_parser && (*path_parser != '/')) {
			*psz++ = *path_parser++;
		}
		*psz = '\0'; // terminate the string.

		// Now lets see if we can find this item in the record list.
		MTPD::PrintStream()->printf("Looking for: %s\n", filename);
		index = record.child;
		while (index) {
			record = ReadIndexRecord(index);
			printRecordIncludeName(index, &record);
			if (strcmp(filename, record.name) == 0) break; // found a match
			index = record.sibling;
		}

		if (index) {
			// found a match. return it.
			if (*path_parser == '\0') {
				MTPD::PrintStream()->printf("Found Node: %d\n", index);
				return index;
			}

		} else {
			// item not found
			MTPD::PrintStream()->println("Node Not found");

			if ((*path_parser != '\0') || !addLastNode) {
				return 0xFFFFFFFFUL; // not found nor added
			}
			// need to add item
			uint32_t parent = record.parent;
			record = ReadIndexRecord(parent);
			Record r; // could probably reuse the other one, but...
			///////////
			// Question is should more data be passed in like is this a
			// directory and size or should we ask for it now, and/or
			// should we mark it that we have not grabbed that info and
			// wait for someone to ask for it?

			strlcpy(r.name, filename, MAX_FILENAME_LEN);
			r.store = storage;
			r.parent = parent;
			r.child = 0;
			r.scanned = false;

			mtp_lock_storage(true);
			if (sd_isOpen(file_)) file_.close();
			file_ = open(storage, pathname, FILE_READ);
			mtp_lock_storage(false);

			if (sd_isOpen(file_)) {
				r.isdir = file_.isDirectory();
				if (!r.isdir) {
					r.child = (uint32_t)file_.size();
				}
				mtp_lock_storage(true);
				file_.close();
				mtp_lock_storage(false);
			} else {
				r.isdir = false;
			}
			r.sibling = record.child;
			// New folder is empty, scanned = true.
			index = record.child = AppendIndexRecord(r);
			WriteIndexRecord(parent, record);

			MTPD::PrintStream()->printf("New node created: %d\n", index);
			record = ReadIndexRecord(index);
			printRecordIncludeName(index, &record);
			if (node_added) *node_added = true;
			return index;
		}
	}
	return 0xFFFFFFFFUL;
}


// Simple method to call any callback functions who want to know
// when the loop function is called to do some minor background work
void MTPStorage_SD::loop() {
  int fs_count = sd_getFSCount();
  for (int store = 0; store < fs_count; store++) {
    FS *pfs = sd_getStoreFS(store);
    MTPStorageInterfaceCB *callback = sd_getCallback(store);

    if (pfs && callback) {
      // Have them return something to maybe remove calls
      callback->loop(this, store, sd_getUserToken(store));
    }
  }
}
