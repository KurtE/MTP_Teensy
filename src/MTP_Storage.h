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

// BUGBUG:: added to work for now...
#if TEENSYDUINO >= 156
#define FS_FILE_SUPPORT_DATES
#endif

#include "core_pins.h"

#include "FS.h"
#ifndef FILE_WRITE_BEGIN
#define FILE_WRITE_BEGIN 2
#endif
#ifdef FS_FILE_SUPPORT_DATES

#define MTP_SUPPORT_MODIFY_DATE
#define MTP_SUPPORT_CREATE_DATE
#endif

#define MTPD_MAX_FILESYSTEMS 20
#ifndef MAX_FILENAME_LEN
#define MAX_FILENAME_LEN 256
#endif


struct Record {
  uint32_t parent;
  uint32_t child; // size stored here for files
  uint32_t sibling;
#ifdef MTP_SUPPORT_MODIFY_DATE
  uint32_t dtModify;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
  uint32_t dtCreate;
#endif
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
	uint32_t getStoreID(const char *fsname) {
		for (unsigned int i = 0; i < fsCount; i++) {
			if (strcmp(fsname, name[i]) == 0) return i;
		}
		return 0xFFFFFFFFUL;
	}
	const char *getStoreName(uint32_t store) {
		if (store < (uint32_t)fsCount) return name[store];
		return nullptr;
	}
	FS *getStoreFS(uint32_t store) {
		if (store < (uint32_t)fsCount) return fs[store];
		return nullptr;
	}
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
//    Serial.printf("MTPStorage:usedSize %u %x\n", store, (uint32_t)fs[store]);
		return fs[store]->usedSize();
	}
	bool copy(uint32_t store0, char *oldfilename, uint32_t store1, char *newfilename);
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
	bool CopyFiles(uint32_t storage, uint32_t handle, uint32_t newHandle);
	uint32_t MapFileNameToIndex(uint32_t storage, const char *pathname,
		bool addLastNode = false, bool *node_added = nullptr);
	void OpenIndex();
	void GenerateIndex(uint32_t storage);
	void ScanDir(uint32_t storage, uint32_t i);
	void ScanAll(uint32_t storage);
	void removeFile(uint32_t store, const char *filename);
	void WriteIndexRecord(uint32_t i, const Record &r);
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
};





class MTPStorage_SD;

class MTPStorageInterfaceCB {
public:
  virtual bool formatStore(MTPStorage_SD *mtpstorage, uint32_t store,
                              uint32_t user_token, uint32_t p2) {
    return false;
  }
  virtual uint64_t totalSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                               uint32_t user_token);
  virtual uint64_t usedSizeCB(MTPStorage_SD *mtpstorage, uint32_t store,
                              uint32_t user_token);
  virtual bool loop(MTPStorage_SD *mtpstorage, uint32_t store,
                    uint32_t user_token) {
    return false;
  }
};

class mSD_Base {
public:
  mSD_Base() { fsCount = 0; }
  uint32_t sd_addFilesystem(FS &fs, const char *name,
                            MTPStorageInterfaceCB *callback,
                            uint32_t user_token);

  bool sd_removeFilesystem(uint32_t store);

  uint32_t sd_getStoreID(const char *name) {
    for (int ii = 0; ii < fsCount; ii++)
      if (!strcmp(name, sd_name[ii]))
        return ii;
    return 0xFFFFFFFFUL;
  }

  const char *sd_getStoreName(uint32_t store) {
    if (store < (uint32_t)fsCount)
      return sd_name[store];
    return nullptr;
  }
  FS *sd_getStoreFS(uint32_t store) {
    if (store < (uint32_t)fsCount)
      return sdx[store];
    return nullptr;
  }
  MTPStorageInterfaceCB *sd_getCallback(uint32_t store) {
    if (store < (uint32_t)fsCount)
      return callbacks[store];
    return nullptr;
  }
  uint32_t sd_getUserToken(uint32_t store) {
    if (store < (uint32_t)fsCount)
      return user_tokens[store];
    return 0;
  }

  uint32_t sd_getFSCount(void) { return fsCount; }
  const char *sd_getFSName(uint32_t store) { return sd_name[store]; }

  File sd_open(uint32_t store, const char *filename, uint32_t mode) {
    return sdx[store]->open(filename, mode);
  }
  bool sd_mkdir(uint32_t store, char *filename) {
    return sdx[store]->mkdir(filename);
  }
  bool sd_rename(uint32_t store, char *oldfilename, char *newfilename) {
    return sdx[store]->rename(oldfilename, newfilename);
  }
  bool sd_remove(uint32_t store, const char *filename) {
    return sdx[store]->remove(filename);
  }
  bool sd_rmdir(uint32_t store, char *filename) {
    return sdx[store]->rmdir(filename);
  }

  uint64_t sd_totalSize(uint32_t store) { return sdx[store]->totalSize(); }
  uint64_t sd_usedSize(uint32_t store) { return sdx[store]->usedSize(); }

  bool sd_copy(uint32_t store0, char *oldfilename, uint32_t store1,
               char *newfilename);
  bool sd_moveDir(uint32_t store0, char *oldfilename, uint32_t store1,
                  char *newfilename);

private:
  int fsCount;
  const char *sd_name[MTPD_MAX_FILESYSTEMS];
  FS *sdx[MTPD_MAX_FILESYSTEMS];
  MTPStorageInterfaceCB *callbacks[MTPD_MAX_FILESYSTEMS];
  uint32_t user_tokens[MTPD_MAX_FILESYSTEMS];
};

// This interface lets the MTP responder interface any storage.
// We'll need to give the MTP responder a pointer to one of these.
class MTPStorageInterface {
public:
  virtual uint32_t addFilesystem(FS &filesystem, const char *name,
                                 MTPStorageInterfaceCB *callback,
                                 uint32_t user_token) = 0;
  virtual bool removeFilesystem(uint32_t storage) = 0;
  virtual uint32_t get_FSCount(void) = 0;
  virtual const char *get_FSName(uint32_t storage) = 0;
  virtual void loop() = 0;

  virtual uint64_t totalSize(uint32_t storage) = 0;
  virtual uint64_t usedSize(uint32_t storage) = 0;
  virtual bool formatStore(uint32_t store, uint32_t p2);

  // Return true if this storage is read-only
  virtual bool readonly(uint32_t storage) = 0;

  // Does it have directories?
  virtual bool has_directories(uint32_t storage) = 0;

  virtual void StartGetObjectHandles(uint32_t storage, uint32_t parent) = 0;
  virtual uint32_t GetNextObjectHandle(uint32_t storage) = 0;

  virtual void GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
                             uint32_t *parent, uint16_t *store) = 0;
  virtual uint32_t GetSize(uint32_t handle) = 0;
#ifdef MTP_SUPPORT_MODIFY_DATE
  virtual bool getModifyTime(uint32_t handle, uint32_t &dt) = 0;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
  virtual bool getCreateTime(uint32_t handle, uint32_t &dt) = 0;
#endif
  virtual bool updateDateTimeStamps(uint32_t handle, uint32_t dtCreated,
                                    uint32_t dtModified) = 0;

  virtual uint32_t Create(uint32_t storage, uint32_t parent, bool folder,
                          const char *filename) = 0;
  virtual void read(uint32_t handle, uint32_t pos, char *buffer,
                    uint32_t bytes) = 0;
  virtual size_t write(const char *data, uint32_t size);
  virtual void close() = 0;
  virtual bool DeleteObject(uint32_t object) = 0;
  virtual void CloseIndex() = 0;

  virtual void ResetIndex() = 0;
  virtual bool rename(uint32_t handle, const char *name) = 0;
  virtual bool move(uint32_t handle, uint32_t newStorage,
                    uint32_t newParent) = 0;
  virtual uint32_t copy(uint32_t handle, uint32_t newStorage,
                        uint32_t newParent) = 0;

  virtual bool CopyFiles(uint32_t storage, uint32_t handle,
                         uint32_t newHandle) = 0;
  virtual uint32_t MapFileNameToIndex(uint32_t storage, const char *pathname,
                                      bool addLastNode = false,
                                      bool *node_added = nullptr) = 0;
  virtual uint32_t openFileIndex(void) = 0;
};


void mtp_yield(void);

// Storage implementation for SD. SD needs to be already initialized.
class MTPStorage_SD : public MTPStorageInterface, mSD_Base {
public:
  uint32_t addFilesystem(FS &fs, const char *name,
                         MTPStorageInterfaceCB *callback = nullptr,
                         uint32_t user_token = 0) {
    return sd_addFilesystem(fs, name, callback, user_token);
  }
  bool removeFilesystem(uint32_t storage) {
    return sd_removeFilesystem(storage);
  }
  void dumpIndexList(void);
  uint32_t getStoreID(const char *name) { return sd_getStoreID(name); }
  uint32_t getFSCount(void) { return sd_getFSCount(); }
  const char *getStoreName(uint32_t store) { return sd_getStoreName(store); }
  FS *getStoreFS(uint32_t store) { return sd_getStoreFS(store); }
  MTPStorageInterfaceCB *getCallback(uint32_t store) {
    return sd_getCallback(store);
  }
  uint32_t getUserToken(uint32_t store) { return sd_getUserToken(store); }

  uint32_t openFileIndex(void) { return open_file_; }

  void setIndexFile(File *index_file = nullptr); // allow application to pass in
                                                 // index_file to be used.
  bool
  setIndexStore(uint32_t storage =
                    0); // or can specify specif store in list defaults to 0

  /** set the file's last access date */
  const uint8_t T_ACCESS = 1;
  /** set the file's creation date and time */
  const uint8_t T_CREATE = 2;
  /** Set the file's write date and time */
  const uint8_t T_WRITE = 4;

private:
  File index_;
  File file_;
  File child_;
  uint32_t index_file_storage_ = 0;
  bool user_index_file_ = false;

  int num_storage = 0;
  const char **sd_str = 0;

  uint32_t mode_ = 0;
  uint32_t open_file_ = 0xFFFFFFFEUL;

  bool readonly(uint32_t storage);
  bool has_directories(uint32_t storage);

  uint64_t totalSize(uint32_t storage);
  uint64_t usedSize(uint32_t storage);
  bool formatStore(uint32_t store, uint32_t p2);

  void CloseIndex();
  void OpenIndex();
  void GenerateIndex(uint32_t storage);
  void ScanDir(uint32_t storage, uint32_t i);
  void ScanAll(uint32_t storage);

  void removeFile(uint32_t store, char *filename);

  uint32_t index_entries_ = 0;
  bool index_generated = false;

  bool all_scanned_ = false;
  uint32_t next_;
  bool follow_sibling_;

  void WriteIndexRecord(uint32_t i, const Record &r);
  uint32_t AppendIndexRecord(const Record &r);
  Record ReadIndexRecord(uint32_t i);
  uint16_t ConstructFilename(int i, char *out, int len);
  void OpenFileByIndex(uint32_t i, uint32_t mode = FILE_READ);
  void printRecord(int h, Record *p);
  void printRecordIncludeName(int h, Record *p);

  uint32_t get_FSCount(void) { return sd_getFSCount(); }
  const char *get_FSName(uint32_t storage) { return sd_getFSName(storage); }
  void loop();

  void StartGetObjectHandles(uint32_t storage, uint32_t parent) override;
  uint32_t GetNextObjectHandle(uint32_t storage) override;
  void GetObjectInfo(uint32_t handle, char *name, uint32_t *size,
                     uint32_t *parent, uint16_t *store) override;
  uint32_t GetSize(uint32_t handle) override;
#ifdef MTP_SUPPORT_MODIFY_DATE
  bool getModifyTime(uint32_t handle, uint32_t &dt) override;
#endif
#ifdef MTP_SUPPORT_CREATE_DATE
  bool getCreateTime(uint32_t handle, uint32_t &dt) override;
#endif
  virtual bool updateDateTimeStamps(uint32_t handle, uint32_t dtCreated,
                                    uint32_t dtModified) override;

  void read(uint32_t handle, uint32_t pos, char *out, uint32_t bytes) override;
  bool DeleteObject(uint32_t object) override;

  uint32_t Create(uint32_t storage, uint32_t parent, bool folder,
                  const char *filename) override;

  size_t write(const char *data, uint32_t bytes) override;
  void close() override;

  bool rename(uint32_t handle, const char *name) override;
  bool move(uint32_t handle, uint32_t newStorage, uint32_t newParent) override;
  uint32_t copy(uint32_t handle, uint32_t newStorage,
                uint32_t newParent) override;

  bool CopyFiles(uint32_t storage, uint32_t handle,
                 uint32_t newHandle) override;
  void ResetIndex() override;
  uint32_t MapFileNameToIndex(uint32_t storage, const char *pathname,
                              bool addLastNode = false,
                              bool *node_added = nullptr) override;

  friend class MTPStorageInterfaceCB;
};

#endif
