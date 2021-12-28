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

#if DEBUG > 1
#define DBGPrintf(...) MTPD::PrintStream()->printf(__VA_ARGS__)
#else
#define DBGPrintf(...)
#endif



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


void MTPStorage::CloseIndex()
{
	mtp_lock_storage(true);
	if (user_index_file_) {
		// maybe truncate the file
		index_.seek(0, SeekSet);
		index_.truncate();
	} else if (index_) {
		index_.close();
	}
	mtp_lock_storage(false);
	index_generated = false;
	index_entries_ = 0;
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


void MTPStorage::ResetIndex()
{
	if (!index_) return;
	CloseIndex();
	all_scanned_ = false;
	open_file_ = 0xFFFFFFFEUL;
}


bool MTPStorage::WriteIndexRecord(uint32_t i, const Record &r)
{
	OpenIndex();
	mtp_lock_storage(true);
	bool write_succeeded = true;
	if (i < MTPD_MAX_FILESYSTEMS) {
		// all we need is the first child pointer and if it was scanned
		store_first_child_[i] = r.child;
		store_scanned_[i] = r.scanned;
	} else {
		index_.seek((i - MTPD_MAX_FILESYSTEMS) * sizeof(r));		
		size_t bytes_written = index_.write((char *)&r, sizeof(r));
		if (bytes_written != sizeof(r)) {
			DBGPrintf(F("$$$ Failed to write Index record: %u bytes written: %u\n"), i, bytes_written);
			write_succeeded = false;
		}
	}
	mtp_lock_storage(false);
	return write_succeeded;
}

uint32_t MTPStorage::AppendIndexRecord(const Record &r)
{
	uint32_t new_record = index_entries_++;
	WriteIndexRecord(new_record, r);
	return new_record;
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
	if (i < MTPD_MAX_FILESYSTEMS) {
		// Build it on the fly...
		ret.store = i;
		ret.parent = 0xFFFFFFFFUL;
		ret.sibling = 0;
		ret.child = store_first_child_[i];
		ret.isdir = true;
		ret.scanned = store_scanned_[i];
		ret.dtModify = 0;
		ret.dtCreate = 0;
		strcpy(ret.name, "/");
	} else {
		bool seek_ok  __attribute__((unused)) = index_.seek((i - MTPD_MAX_FILESYSTEMS) * sizeof(ret));		
		int cb_read = index_.read((char *)&ret, sizeof(ret));
		if (cb_read != sizeof(ret)) {
			DBGPrintf("ReadIndexRecord(%u): %u %d %s\n", i, seek_ok, cb_read, ret.name);
			memset(&ret, 0, sizeof(ret));
		}
	}

	mtp_lock_storage(false);
	return ret;
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
		DBGPrintf(
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
void MTPStorage::GenerateIndex(uint32_t store)
{
	if (index_generated) return;
	index_generated = true;
  MTPD::PrintStream()->println("*** MTPStorage::GenerateIndex called ***"); 
	// first remove old index file
	mtp_lock_storage(true);
	if (user_index_file_) {
		// maybe truncate the file
		index_.seek(0, SeekSet);
		index_.truncate();
	} else {
    DBGPrintf("    remove called: %u %s\n", index_file_storage_, indexFile); 
		remove(index_file_storage_, indexFile);
	}
	mtp_lock_storage(false);
	//num_storage = getFSCount();
	index_entries_ = 0;
	Record r;
	// BugBug - will generate index for max file systems count... 
	// Note hacked up storage of these items...
	for (int ii = 0; ii < MTPD_MAX_FILESYSTEMS; ii++) {
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
    printRecordIncludeName(ii, &r);
	}
}

void MTPStorage::ScanDir(uint32_t store, uint32_t i)
{
  DBGPrintf("** ScanDir called %u %u\n", store, i); 
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
      MTPD::PrintStream()->print("  >> "); 
      printRecordIncludeName(sibling, &r);
			child_.close();
		}
		record.scanned = true;
		record.child = sibling;
		WriteIndexRecord(i, record);
	}
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


void MTPStorage::GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
                               uint32_t *parent, uint16_t *store)
{
	Record r = ReadIndexRecord(handle);
	strcpy(name, r.name);
	*parent = r.parent;
	*size = r.isdir ? 0xFFFFFFFFUL : r.child;
	*store = r.store;
}


uint32_t MTPStorage::GetSize(uint32_t handle) {
	return ReadIndexRecord(handle).child;
}

bool MTPStorage::getModifyTime(uint32_t handle, uint32_t &dt)
{
	Record r = ReadIndexRecord(handle);
	dt = r.dtModify;
	return (r.dtModify) ? true : false;
}

bool MTPStorage::getCreateTime(uint32_t handle, uint32_t &dt)
{
  Record r = ReadIndexRecord(handle);
  dt = r.dtCreate;
  return (r.dtCreate) ? true : false;
}

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

bool MTPStorage::updateDateTimeStamps(uint32_t handle, uint32_t dtCreated, uint32_t dtModified)
{
	Record r = ReadIndexRecord(handle);
	DateTimeFields dtf;
	if ((dtCreated == 0) && (dtModified == 0)) {
		DBGPrintf("&&DT (0,0) (%u,%u)\n", r.dtCreate, r.dtModify);
		return true;

	}
	OpenFileByIndex(handle, FILE_READ);
	if (!file_) {
		DBGPrintf(
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
//	file_.close();
	mtp_lock_storage(false);
	return true;
}

void MTPStorage::read(uint32_t handle, uint32_t pos, char *out, uint32_t bytes)
{
	OpenFileByIndex(handle);
	mtp_lock_storage(true);
	file_.seek(pos);
	file_.read(out, bytes);
	mtp_lock_storage(false);
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



uint32_t MTPStorage::Create(uint32_t store, uint32_t parent, bool folder, const char *filename)
{
	DBGPrintf("MTPStorage::create(%u, %u, %u, %s)\n", store, parent, folder, filename);
	uint32_t ret;
	if (parent == 0xFFFFFFFFUL) parent = store;
	ScanDir(store, parent); // make sure the parent is scanned...
	Record p = ReadIndexRecord(parent);
	Record r;

	// See if the name already exists in the parent
	uint32_t index = p.child;
	while (index) {
		r = ReadIndexRecord(index);
		if (strcmp(filename, r.name) == 0) break; // found a match
		index = r.sibling;
	}

	if (index) {
		// found that name in our list
		DBGPrintf("    >> Parent (%u) already contains %s(%u)\n", parent, filename, index);
		if (folder != r.isdir) {
			DBGPrintf("    >> Not same type: cur:%u new:%u\n", r.isdir, folder);
			return 0xFFFFFFFFUL;
		}

		DBGPrintf("    >> using index\n", index);
		return index;
	}

	strlcpy(r.name, filename, MAX_FILENAME_LEN);
	r.store = p.store;
	r.parent = parent;
	r.child = 0;
	r.sibling = p.child;
	r.isdir = folder;
	r.dtModify = 0;
	r.dtCreate = 0;
	// New folder is empty, scanned = true.
	r.scanned = 1;
	ret = p.child = AppendIndexRecord(r);
	WriteIndexRecord(parent, p);
	printRecordIncludeName(parent, &p);
	printRecordIncludeName(ret, &r);
	dumpIndexList();
	if (folder) {
		char filename[MAX_FILENAME_LEN];
		ConstructFilename(ret, filename, MAX_FILENAME_LEN);
		DBGPrintf("    >>(%u, %s)\n", ret, filename);
		mtp_lock_storage(true);
		mkdir(store, filename);
		MTPD::PrintStream()->println("    >> After mkdir"); MTPD::PrintStream()->flush();
		mtp_lock_storage(false);
		OpenFileByIndex(ret, FILE_READ);
		MTPD::PrintStream()->println("    >> After OpenFileByIndex"); MTPD::PrintStream()->flush();
		if (!file_) {
			DBGPrintf(
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
			DBGPrintf(
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
	MTPD::PrintStream()->print(r.dtModify);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->print(r.dtCreate);
	MTPD::PrintStream()->print(" ");
	MTPD::PrintStream()->println(filename);
#endif
	return ret;
}


size_t MTPStorage::write(const char *data, uint32_t bytes)
{
	mtp_lock_storage(true);
	// make sure it does not fall through to default Print version of buffer write
	size_t ret = file_.write((void*)data, bytes);
	mtp_lock_storage(false);
	return ret;
}


void MTPStorage::close()
{
	mtp_lock_storage(true);
	uint32_t size = (uint32_t)file_.size();
	file_.close();
	mtp_lock_storage(false);
	// update record with file size
	Record r = ReadIndexRecord(open_file_);
  if (!r.isdir) {
    r.child = size;
    WriteIndexRecord(open_file_, r);
  }
	open_file_ = 0xFFFFFFFEUL;
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

void MTPStorage::dumpIndexList(void)
{
	if (index_entries_ == 0) return;
	uint32_t fsCount = getFSCount();
	for (uint32_t ii = 0; ii < index_entries_; ii++) {
		if ((ii < fsCount) || (ii >= MTPD_MAX_FILESYSTEMS)) {
			Record p = ReadIndexRecord(ii);
			MTPD::PrintStream()->printf("%d: %d %d %u %d %d %d %u %u %s\n",
				ii, p.store, p.isdir, p.scanned, p.parent, p.sibling, p.child,
				p.dtCreate, p.dtModify, p.name);
		}
	}
}


void MTPStorage::printRecord(int h, Record *p)
{
	MTPD::PrintStream()->printf("%d: %d %d %d %d %d\n", h, p->store, p->isdir,
                              p->parent, p->sibling, p->child);
}

void MTPStorage::printRecordIncludeName(int h, Record *p)
{
	MTPD::PrintStream()->printf("%d: %u %u %u %u %u %u %u %u %s\n", h, p->store,
                              p->isdir, p->scanned, p->parent, p->sibling,
                              p->child, p->dtModify, p->dtCreate, p->name);
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

//=============================================================================
// move: process the mtp moveObject command
//=============================================================================
// lets see if we are just doing a simple rename or if we need to do full
// move. 
bool MTPStorage::move(uint32_t handle, uint32_t newStore, uint32_t newParent)
{
	DBGPrintf("MTPStorage::move %d -> %d %d\n", handle, newStore, newParent);
	setLastError(NO_ERROR);
	Record p1 = ReadIndexRecord(handle);

	ScanDir(newStore, newParent);  // make sure the new parent has been enumerated. 

	char oldName[MAX_FILENAME_LEN];
	ConstructFilename(handle, oldName, MAX_FILENAME_LEN);

	// try hack use temporary one to generate new path name.
	char newName[MAX_FILENAME_LEN];
	// first get the path name for new parent 
	ConstructFilename(newParent, newName, MAX_FILENAME_LEN);
	if (newName[1] != 0) strlcat(newName, "/", MAX_FILENAME_LEN); 
	strlcat(newName, p1.name, MAX_FILENAME_LEN);
	DBGPrintf("    >>From:%u %s to:%u %s\n", p1.store, oldName, newStore, newName);
	if (p1.store == newStore) {
			MTPD::PrintStream()->println("  >> Move same storage");
		if (!rename(newStore, oldName, newName)) {
			DBG_FAIL_MACRO;
			setLastError(RENAME_FAIL);
			// failed, so simply return... did not change anything. 
			return false;
		}
	} else if (!p1.isdir) {
			MTPD::PrintStream()->println("  >> Move differnt storage file");
		if (CopyByPathNames(p1.store, oldName, newStore, newName)) {
			remove(p1.store, oldName);
		} else {
			DBG_FAIL_MACRO;
			return false;
		}
	} else { // move directory cross mtp-disks
			MTPD::PrintStream()->println("  >> Move differnt storage directory");
		if (!moveDir(p1.store, oldName, newStore, newName)) {
			DBG_FAIL_MACRO;
			return false;
		}
	}

	// remove index from old parent
	Record p2 = ReadIndexRecord(p1.parent);
	if (p2.child == handle) {  // was first on in list. 
		p2.child = p1.sibling;
		WriteIndexRecord(p1.parent, p2);
	} else {
		uint32_t jx = p2.child;
		Record px = ReadIndexRecord(jx);
		while (handle != px.sibling) {
			jx = px.sibling;
			px = ReadIndexRecord(jx);
		}
		px.sibling = p1.sibling;
		WriteIndexRecord(jx, px);
	}

	// add to new parent
	p2 = ReadIndexRecord(newParent);
	p1.parent = newParent;
	p1.store = p2.store;
	p1.sibling = p2.child;
	p2.child = handle;
	WriteIndexRecord(handle, p1);
	WriteIndexRecord(newParent, p2);

	// Should be done
	return true;

}

// old and new are directory paths
bool MTPStorage::moveDir(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename)
{
	char tmp0Name[MAX_FILENAME_LEN];
	char tmp1Name[MAX_FILENAME_LEN];

	if (!mkdir(store1, newfilename)) {
		setLastError(MKDIR_FAIL);
		DBG_FAIL_MACRO;
		return false;
	}
	File f1 = open(store0, oldfilename, FILE_READ);
	if (!f1) {
		setLastError(SOURCE_OPEN_FAIL);
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
				if (!CopyByPathNames(store0, tmp0Name, store1, tmp1Name)) {
					DBG_FAIL_MACRO;
					return false;
				}
				if (!remove(store0, tmp0Name)) {
					setLastError(REMOVE_FAIL);
					DBG_FAIL_MACRO;
					return false;
				}
			}
		}
	}
	if (rmdir(store0, oldfilename)) return true;
	setLastError(RMDIR_FAIL);
	DBG_FAIL_MACRO;
	return false;

}

//=============================================================================
// copy: process the copy command, some functions below used by move as well
//=============================================================================

uint32_t MTPStorage::copy(uint32_t handle, uint32_t newStore, uint32_t newParent)
{
	setLastError(NO_ERROR);
	DBGPrintf("MTPStorage::copy(%u, %u, %u)\n", handle, newStore, newParent);
	if (newParent == 0xFFFFFFFFUL) newParent = newStore;
	Record p1 = ReadIndexRecord(handle);
	Record p2 = ReadIndexRecord(newParent); // 0 means root of store

	uint32_t newHandle = Create(p2.store, newParent, p1.isdir, p1.name);
	if (newHandle == 0xFFFFFFFFUL) {
		setLastError(DEST_OPEN_FAIL);
		DBG_FAIL_MACRO;
	} else if (p1.isdir) {
		ScanDir(p1.store, handle);
		CopyFiles(handle, newHandle);
	} else {
		CompleteCopyFile(handle, newHandle);
	}
	return newHandle;
}


// assume handle and newHandle point to existing directories
bool MTPStorage::CopyFiles(uint32_t handle, uint32_t newHandle)
{
	DBGPrintf("%d -> %d\n", handle, newHandle);
	bool copy_completed_without_error = true;
	Record r = ReadIndexRecord(handle);
	uint32_t source_store = r.store;
	uint32_t ix = r.child;
	uint32_t iy = 0;

	r = ReadIndexRecord(newHandle);
	uint32_t target_store = r.store;
	while (ix) { // get child
		Record px = ReadIndexRecord(ix);
		iy = Create(target_store, newHandle, px.isdir, px.name);
		if (iy != 0xFFFFFFFFUL) {
			if (px.isdir) {
				ScanDir(source_store, ix);
				if (!CopyFiles(ix, iy)) {
					copy_completed_without_error = false;
					break;
				}
			} else {
				if (!CompleteCopyFile(ix, iy)) {
					copy_completed_without_error = false;
					break;
				}
			}
		}
		ix = px.sibling;
	}
	r.child = iy;
	WriteIndexRecord(newHandle, r);
	return copy_completed_without_error;
}

#if defined(__IMXRT1062__)
	#define COPY_BUFFER MTPD::disk_buffer_
	#define COPY_BUFFER_SIZE MTPD::DISK_BUFFER_SIZE
#else
	#define COPY_BUFFER (copy_buffer)
	#define COPY_BUFFER_SIZE 512
#endif

bool MTPStorage::CompleteCopyFile(uint32_t from, uint32_t to)
{

	Record r = ReadIndexRecord(to);
	bool copy_completed_without_error = true;
	
	// open the source file...
	// the Create call should have opened the target. 
#if !defined(__IMXRT1062__)
	uint8_t *copy_buffer = (uint8_t *)malloc(COPY_BUFFER_SIZE);
	if (!copy_buffer) return false;
#endif

	char source_file_name[MAX_FILENAME_LEN];
	uint32_t store0 = ConstructFilename(from, source_file_name, MAX_FILENAME_LEN);
	DBGPrintf("  >> CompleteCopyFile(%u, %u) %s\n", from, to, source_file_name);
	File f1 = open(store0, source_file_name, FILE_READ);
	if (!f1) {
		setLastError(DEST_OPEN_FAIL);
		DBG_FAIL_MACRO;
#if !defined(__IMXRT1062__)
		free(copy_buffer);
#endif
		return false;
	}
	file_.truncate();	// make sure to remove old data... Should we do this at end? 


	int nd = -1;

	while (f1.available() > 0) {
		nd = f1.read(COPY_BUFFER, COPY_BUFFER_SIZE);
		if (nd < 0) { // read error
			setLastError(READ_ERROR);
			copy_completed_without_error = false;
			DBG_FAIL_MACRO;
			break;
		}
		size_t cb_written = file_.write(COPY_BUFFER, nd); 
		if (cb_written < (uint32_t)nd) {
			setLastError(WRITE_ERROR);
			DBG_FAIL_MACRO;
			copy_completed_without_error = false;
			break;
		}
		if ((uint32_t)nd < COPY_BUFFER_SIZE) break; // end of file
	}

	DateTimeFields dtf;
	if (f1.getModifyTime(dtf)) {
		DBGPrintf("  >> Updated Modify Date\n");
		file_.setModifyTime(dtf);
		r.dtModify = makeTime(dtf);
	}

#if !defined(__IMXRT1062__)
		free(copy_buffer);
#endif
	// close source file
	f1.close();

	mtp_lock_storage(true);
	r.child = (uint32_t)file_.size();
	WriteIndexRecord(to, r);
#if DEBUG > 1
	MTPD::PrintStream()->print("  >>"); printRecordIncludeName(to, &r);
#endif
	file_.close();
	mtp_lock_storage(false);
	DBGPrintf("  >> return %u\n", copy_completed_without_error);
	return copy_completed_without_error;
}
#if 1
bool MTPStorage::CopyByPathNames(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename)
{
#if !defined(__IMXRT1062__)
	uint8_t *copy_buffer = (uint8_t *)malloc(COPY_BUFFER_SIZE);
	if (!copy_buffer) return false;
#endif
	int nd = -1;

#if DEBUG > 1
	MTPD::PrintStream()->print("MTPStorage::CopyByPathNames - From ");
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
#if !defined(__IMXRT1062__)
		free(copy_buffer);
#endif
		return false;
	}
	File f2 = open(store1, newfilename, FILE_WRITE_BEGIN);
	if (!f2) {
		f1.close();
		DBG_FAIL_MACRO;
#if !defined(__IMXRT1062__)
		free(copy_buffer);
#endif
		return false;
	}
	while (f1.available() > 0) {
		nd = f1.read(COPY_BUFFER, COPY_BUFFER_SIZE);
		if (nd < 0) break; // read error
		f2.write(COPY_BUFFER, nd);
		if ((uint32_t)nd < COPY_BUFFER_SIZE) break; // end of file
	}
	// Lets see if we can set the modify date of the new file to that of the
	// file we are copying from.
	DateTimeFields tm;
	if (f1.getModifyTime(tm)) {
		DBGPrintf("  >> Updated Modify Date");
		f2.setModifyTime(tm);
	}

	// close all files
	f1.close();
	f2.close();
#if !defined(__IMXRT1062__)
		free(copy_buffer);
#endif
	if (nd < 0) {
		DBG_FAIL_MACRO;
		return false;
	}
	return true;
}
#endif

uint32_t MTPStorage::addFilesystem(FS &disk, const char *diskname)
{
	if (fsCount < MTPD_MAX_FILESYSTEMS) {
		name[fsCount] = diskname;
		fs[fsCount] = &disk;
		store_storage_minor_index_[fsCount] = 1; // start off with 1
		DBGPrintf("addFilesystem: %d %s %x\n", fsCount, diskname, (uint32_t)fs[fsCount]);
		return fsCount++;
	} else {
		// See if we can reuse index
		for (uint32_t store=0; store < MTPD_MAX_FILESYSTEMS; store++) {
			if (fs[store] == nullptr) {
				// found one to reuse.
				name[store] = diskname;
				fs[store] = &disk;
				store_first_child_[store] = 0;
				store_scanned_[store] = false;
				store_storage_minor_index_[store]++;
				DBGPrintf("addFilesystem(%u): %d %s %x\n", store_storage_minor_index_[store], store, diskname, (uint32_t)fs[store]);
				return store;
			}
		}		
	}
	return 0xFFFFFFFFUL; // no room left
}


bool MTPStorage::removeFilesystem(uint32_t store)
{
	if ((store < fsCount) && (name[store])) {
		name[store] = nullptr;
		fs[store] = nullptr;
		// Now lets see about pruning
		clearStoreIndexItems(store);

		return true;
	}
	return false;
}

bool MTPStorage::clearStoreIndexItems(uint32_t store)
{
	if (store >= fsCount) return false;
	if (store_first_child_[store] == 0) return true;
	// first pass simple...
	store_first_child_[store] = 0;
	store_scanned_[store] = 0;

	return true;
}


uint32_t MTPStorage::MapFileNameToIndex(uint32_t storage, const char *pathname,
                                        bool addLastNode, bool *node_added)
{
	const char *path_parser = pathname;
	DBGPrintf(
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
		DBGPrintf("Looking for: %s\n", filename);
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
				DBGPrintf("Found Node: %d\n", index);
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

			DBGPrintf("New node created: %d\n", index);
			record = ReadIndexRecord(index);
			printRecordIncludeName(index, &record);
			if (node_added) *node_added = true;
			return index;
		}
	}
	return 0xFFFFFFFFUL;
}

bool MTPStorage::setIndexStore(uint32_t storage) {
  if (storage >= getFSCount())
    return false; // out of range
  CloseIndex();
  index_file_storage_ = storage;
  user_index_file_ = false;
  return true;
}
