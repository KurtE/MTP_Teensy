// Storage.h - Teensy MTP Responder library
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
// 19-nov-2020 adapted to FS


#ifndef MTP_Storage_H
#define MTP_Storage_H

#include "core_pins.h"

#include "FS.h"

#ifndef FILE_WRITE_BEGIN
#define FILE_WRITE_BEGIN 2
#endif
#define MTPD_MAX_FILESYSTEMS 0x10   // 16
#ifndef MTP_MAX_FILENAME_LEN
#define MTP_MAX_FILENAME_LEN 256 // reduced by size of record header stuff so fits in 256
#endif
#ifndef MTP_MAX_PATH_LEN
#define MTP_MAX_PATH_LEN 260
#endif
#ifndef MTP_FSTYPE_MAX
#define MTP_FSTYPE_MAX  5
#endif

#if defined(__MK20DX128__) || defined(__MK20DX256__)
#define MTP_RECORD_BLOCKS 0
#else
#define MTP_RECORD_BLOCKS 4
#endif


typedef bool (STORAGE_LOOP_CB)(uint8_t store, FS *pfs);

// The mtp_fstype_t right now is WIP, the main user is MTP_FSTYPE_SD which calls per each object
// but USBFS is a class level
typedef enum {MTP_FSTYPE_UNKNOWN=0, MTP_FSTYPE_SD, MTP_FSTYPE_USBFS} mtp_fstype_t;



class MTPStorage final {
public:

// Start of with same record structure...
	struct __attribute__((__packed__)) Record  {
	  uint32_t dtModify;
	  uint32_t dtCreate;
	  uint16_t parent;
	  uint16_t sibling;
	  uint32_t child; // size stored here for files
	  uint8_t store; // index int physical storage (0 ... num_storages-1)
	  uint8_t isdir:1;
	  uint8_t scanned:1;
	  char name[MTP_MAX_FILENAME_LEN];
	};

	struct __attribute__((__packed__)) RecordFixed  {
	  uint32_t dtModify;
	  uint32_t dtCreate;
	  uint16_t parent;
	  uint16_t sibling;
	  uint32_t child; // size stored here for files
	  uint8_t store; // index int physical storage (0 ... num_storages-1)
	  uint8_t isdir:1;
	  uint8_t scanned:1;
	  char name[0];
	};

#if MTP_RECORD_BLOCKS
	// Maybe group records so no need to hold store.
	// but maybe hold it, max of 16 or 32...
	// blocks if we have 2k blocks and if average names are 16 bytes we could hold
	// average of 64items 
	// Packing records and knowing which block is going to be simple
	// bottom 6 bits of object id is the index within a block
	// upper 10 (or 26 if I go back to 32 bits) will be the 2K record number to
	// read and write. 
	enum {MAX_RECORDS_PER_BLOCK=64, BLOCK_SIZE=2048, BLOCK_SIZE_DATA=BLOCK_SIZE - (2*MAX_RECORDS_PER_BLOCK + 3),
		BLOCK_SIZE_NAME_FUDGE=64, INDEX_STORE_MEM_FILE=(uint32_t)-2};
	struct RecordBlock {
		uint16_t recordOffsets[MAX_RECORDS_PER_BLOCK];
		uint16_t dataIndexNextFree; 
		uint8_t  recordCount; 
		uint8_t  data[BLOCK_SIZE_DATA];
	};

	struct RecordBlockInfo {
		uint16_t	block_index; 	// which block is it.
		uint16_t	last_cycle_used; // remember the last cycle through map we used this page
		uint8_t		dirty;			// is this block dirty
	};
#endif

	constexpr MTPStorage() {  }

  	// Add a file system to the list of storages that will be seen by
  	// the host computer.  Returns the index of the item within the list
	uint32_t addFilesystem(FS &disk, const char *diskname, mtp_fstype_t fstype);

  	// Remove a file system from the list of storages 
  	// Example might be if the device was removed.
	bool removeFilesystem(uint32_t store);
	bool clearStoreIndexItems(uint32_t store);
	bool setIndexStore(uint32_t storage = 0);
	uint32_t getStoreID(const char *fsname) {
		for (unsigned int i = 0; i < fsCount; i++) {
			if (strcmp(fsname, name[i]) == 0) return i;
		}
		return 0xFFFFFFFFUL;
	}
	enum {NO_ERROR=0, SOURCE_OPEN_FAIL, DEST_OPEN_FAIL, READ_ERROR, WRITE_ERROR, 
				RENAME_FAIL, MKDIR_FAIL, REMOVE_FAIL, RMDIR_FAIL};
	inline uint8_t getLastError() {return last_error_;}
	inline void setLastError(uint8_t error) {last_error_ = error;}
	const char *getStoreName(uint32_t store) {
		if (store < (uint32_t)fsCount) return name[store];
		return nullptr;
	}

	FS *getStoreFS(uint32_t store) {
		if (store < (uint32_t)fsCount) return fs[store];
		return nullptr;
	}

	inline uint8_t storeMinorIndex(uint32_t store) {return store_storage_minor_index_[store];}


	// Return count of files systems in storage list
  	uint32_t get_FSCount(void) { return fsCount; }

	// Return count of files systems in storage list
	const char *get_FSName(uint32_t store) { return name[store];
	}

	// Older version, not sure if we still need.
	uint32_t getFSCount(void) { return fsCount;	}
	const char *getFSName(uint32_t store) {	return name[store];	}

	// return the Type of the file system, Used in loop to know type of FS to
	// determine if maybe something should be done in places like calls to loop()
	// Currently only FSTYPE_UNKNOWN=0, FSTYPE_SD=1 are defined
	const mtp_fstype_t get_FSType(uint32_t store) {
		return fstype_[store];
	}

	// sets the Type of the file system, Used in loop to know type of FS to
	// determine if maybe something should be done in places like calls to loop()
	// Currently only FSTYPE_UNKNOWN=0, FSTYPE_SD=1 are defined
	const void set_FSType(uint32_t store, mtp_fstype_t fstype) {
		fstype_[store] = fstype;
	}

	File open(uint32_t store, const char *filename, uint32_t mode) {
		if (fs[store] == nullptr) return File();
		return fs[store]->open(filename, mode);
	}
	bool mkdir(uint32_t store, char *filename) {
		if (fs[store] == nullptr) return false;
		return fs[store]->mkdir(filename);
	}
	bool rename(uint32_t store, char *oldfilename, char *newfilename) {
		if (fs[store] == nullptr) return false;
		return fs[store]->rename(oldfilename, newfilename);
	}
	bool remove(uint32_t store, const char *filename) {
		if (fs[store] == nullptr) return false;
		return fs[store]->remove(filename);
	}
	bool rmdir(uint32_t store, const char *filename) {
		if (fs[store] == nullptr) return false;
		return fs[store]->rmdir(filename);
	}
	uint64_t totalSize(uint32_t store) {
		if (fs[store] == nullptr) {
			Serial.printf("$$$ MTPStorage::totalsize nullptr %u\n", store);
			return (uint64_t)-1;
		}
		return fs[store]->totalSize();
	}
	uint64_t usedSize(uint32_t store) {
		if (fs[store] == nullptr) return (uint64_t)-1;
		return fs[store]->usedSize();
	}
	bool CompleteCopyFile(uint32_t from, uint32_t to); 
	bool CopyByPathNames(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename);
	bool moveDir(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename);
	//void loop();
	bool formatStore(uint32_t store, uint32_t p2) {
		return fs[store]->format((int)p2, '*')? 1 : 0;
	}
	bool readonly(uint32_t storage) {
		return false;
	}
	bool has_directories(uint32_t storage) {
		return true;
	}
	void StartGetObjectHandles(uint32_t storage, uint32_t parent);
	uint32_t GetNextObjectHandle(uint32_t storage);
	void GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
		uint32_t *parent, uint16_t *store);
	uint32_t GetSize(uint32_t handle);
	bool getModifyTime(uint32_t handle, uint32_t &dt);
	bool getCreateTime(uint32_t handle, uint32_t &dt);
	bool updateDateTimeStamps(uint32_t handle, uint32_t dtCreated, uint32_t dtModified);
	uint32_t Create(uint32_t storage, uint32_t parent, bool folder, const char *filename);
	void read(uint32_t handle, uint32_t pos, char *buffer, uint32_t bytes);
	size_t write(const char *data, uint32_t size);
	void close();
	bool DeleteObject(uint32_t object);
	void CloseIndex();
	void ResetIndex();
	bool rename(uint32_t handle, const char *name);
	bool move(uint32_t handle, uint32_t newStorage, uint32_t newParent);
	uint32_t copy(uint32_t handle, uint32_t newStorage, uint32_t newParent);
	bool CopyFiles(uint32_t handle, uint32_t newHandle);
	uint32_t MapFileNameToIndex(uint32_t storage, const char *pathname,
		bool addLastNode = false, bool *node_added = nullptr);
	void OpenIndex();
	void GenerateIndex(uint32_t storage);
	void ScanDir(uint32_t storage, uint32_t i);
	void ScanAll(uint32_t storage);
	void removeFile(uint32_t store, const char *filename);
	bool WriteIndexRecord(uint32_t i, const Record &r);
	uint32_t AppendIndexRecord(const Record &r);
	Record ReadIndexRecord(uint32_t i);
	uint16_t ConstructFilename(int i, char *out, int len);
	void OpenFileByIndex(uint32_t i, uint32_t mode = FILE_READ);
	void printRecord(int h, Record *p);
	void printRecordIncludeName(int h, Record *p);
	void dumpIndexList(void);
	bool loop();
	void printClearRecordReadWriteCounts();

	// returns the minimum time in ms of how often we will check the state of some devices
	// in the loop() function.
	uint32_t get_DeltaDeviceCheckTimeMS() { return time_between_device_checks_ms_; }

	// sets the minimum time in ms of how often we will check the state of some devices
	// in the loop() function.  Note: set to (uint32_t)-1 will disable this code
	void set_DeltaDeviceCheckTimeMS(uint32_t delta_time) { time_between_device_checks_ms_ = delta_time; }

	// This registers a function to call for class type
	// Note if class is set to 
	// loop at the DeltaDeviceCheckTimeMS value.
	bool registerClassLoopCallback(mtp_fstype_t fstype, STORAGE_LOOP_CB *loop_cb, bool per_instance=true);   


private:
	unsigned int fsCount = 0;
	const char *name[MTPD_MAX_FILESYSTEMS] = {nullptr};
	FS *fs[MTPD_MAX_FILESYSTEMS] = {nullptr};
	mtp_fstype_t fstype_[MTPD_MAX_FILESYSTEMS] = {MTP_FSTYPE_UNKNOWN};
	bool loop_check_known_fstypes_changed_ = false;
	uint16_t store_first_child_[MTPD_MAX_FILESYSTEMS] = {0};
	uint8_t store_scanned_[MTPD_MAX_FILESYSTEMS] = {0};
	uint8_t store_storage_minor_index_[MTPD_MAX_FILESYSTEMS] = {0};
	uint32_t index_entries_ = 0;
	bool index_generated_ = false;
	bool all_scanned_ = false;
	uint32_t next_ = 0;
	bool follow_sibling_ = 0;
	File index_;
	File file_;
	File child_;
	uint32_t index_file_storage_ = 0;
	bool user_index_file_ = false;
	int num_storage = 0;
	const char **sd_str = 0;
	uint32_t mode_ = 0;
	uint32_t open_file_ = 0xFFFFFFFEUL;
	uint8_t last_error_ = 0;

	// probably temporary...
	uint32_t debug_read_record_count_ = 0;
	uint32_t debug_fs_read_record_count_ = 0;
	uint32_t debug_write_record_count_ = 0;
	uint32_t debug_fs_write_record_count_ = 0;

	// experiment with building in SD Checking
	// 0-not tested yet, 1-inserted, 0xff-not inserted
	static STORAGE_LOOP_CB *s_loop_fstype_cbs[MTP_FSTYPE_MAX];
	static bool s_loop_fstypes_per_instance[MTP_FSTYPE_MAX];

	uint32_t millis_atlast_device_check_ = 0;  // can not use elapsedMillis as per const...
	enum {DEFAULT_TIME_BETWEEN_DEVICE_CHECKS_MS = 500};
	uint32_t time_between_device_checks_ms_ = DEFAULT_TIME_BETWEEN_DEVICE_CHECKS_MS;

	#if MTP_RECORD_BLOCKS
	static RecordBlock recordBlocks_[MTP_RECORD_BLOCKS];
	static RecordBlockInfo recordBlocksInfo_[MTP_RECORD_BLOCKS];
	uint16_t map_objectid_to_block_cycle_ = 0; // used to measure how long ago a block was touched...
	void ClearRecordBlock(uint8_t index);
	uint8_t CacheRecordBlock(uint16_t block_index);
	int16_t maxrecordBlockWritten_ = -1;
	#endif
};

void mtp_yield(void);
#endif
