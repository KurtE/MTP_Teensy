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
#define MTPD_MAX_FILESYSTEMS 20
#ifndef MAX_FILENAME_LEN
#define MAX_FILENAME_LEN 256
#endif


struct Record {
  uint32_t parent;
  uint32_t child; // size stored here for files
  uint32_t sibling;
  uint32_t dtModify;
  uint32_t dtCreate;
  uint8_t isdir;
  uint8_t scanned;
  uint16_t store; // index int physical storage (0 ... num_storages-1)
  char name[MAX_FILENAME_LEN];
};

class MTPStorage final {
public:
	MTPStorage() { fsCount = 0; }
	uint32_t addFilesystem(FS &disk, const char *diskname);
	uint32_t addFilesystem(FS &fs, const char *name, void *unused1, uint32_t unused2) {
		return addFilesystem(fs, name);
	}
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

	uint32_t getFSCount(void) {
		return fsCount;
	}
  	uint32_t get_FSCount(void) {
		return fsCount;
	}
	const char *getFSName(uint32_t store) {
		return name[store];
	}
	const char *get_FSName(uint32_t store) {
		return name[store];
	}
	File open(uint32_t store, const char *filename, uint32_t mode) {
		return fs[store]->open(filename, mode);
	}
	bool mkdir(uint32_t store, char *filename) {
		return fs[store]->mkdir(filename);
	}
	bool rename(uint32_t store, char *oldfilename, char *newfilename) {
		return fs[store]->rename(oldfilename, newfilename);
	}
	bool remove(uint32_t store, const char *filename) {
		return fs[store]->remove(filename);
	}
	bool rmdir(uint32_t store, const char *filename) {
		return fs[store]->rmdir(filename);
	}
	uint64_t totalSize(uint32_t store) {
		return fs[store]->totalSize();
	}
	uint64_t usedSize(uint32_t store) {
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
	void loop() {
	}
private:
	unsigned int fsCount;
	const char *name[MTPD_MAX_FILESYSTEMS];
	FS *fs[MTPD_MAX_FILESYSTEMS];
	uint32_t store_first_child_[MTPD_MAX_FILESYSTEMS];
	uint8_t store_scanned_[MTPD_MAX_FILESYSTEMS];
	uint8_t store_storage_minor_index_[MTPD_MAX_FILESYSTEMS];
	uint32_t index_entries_ = 0;
	bool index_generated = false;
	bool all_scanned_ = false;
	uint32_t next_;
	bool follow_sibling_;
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
};

void mtp_yield(void);
#endif
