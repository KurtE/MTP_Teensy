#ifndef __TeensyFS_H__
#define __TeensyFS_H__
#include <Arduino.h>
#include <FS.h>

//#define USE_PRINTF_DEBUG // Serial4 kernel...
#ifdef USE_PRINTF_DEBUG
#define DBGPRINTF(...) printf_debug(__VA_ARGS__)
extern "C" {
	void printf_debug(const char *format, ...);
}
#else
#define DBGPRINTF(...)
#endif
//-----------------------------------------------------------------------------
// digital pins file
//-----------------------------------------------------------------------------
class TFSDPFile : public  FileImpl {
public:
	TFSDPFile(uint8_t pin_index, uint8_t mode) {
		_pin_index = pin_index;
		_fOpen = true;
		snprintf(_name, sizeof(_name), "%d.txt", _pin_index);
		updateValue();
		_position = 0;
		Serial.printf("\n@@@@D@@@@@ TFSDPFile::TFSDPFile %u %s=%s\n", _pin_index, _name, _value);
	};

	~TFSDPFile() {close(); }


	virtual size_t read(void *buf, size_t nbyte) {
		Serial.printf("))read(%x %u) %u\n", (uint32_t)buf, nbyte, _position);
		char *p = (char*)buf;
		int cb = 0;
		while (nbyte && (_position < _value_len)) {
			*p++ = _value[_position++];
			nbyte--;
			cb++;
		}
		return cb;
	}
	virtual size_t write(const void *buf, size_t size) { 
		// hack not going to look at other data in file...
		const char *p = (const char*)buf; 
		if (size) {
			if (*p == '0') digitalWrite(_pin_index, 0);
			else if (*p == '1') digitalWrite(_pin_index, 1);
			updateValue();
			return size;
		}
		return 0; 
	}
	virtual int available() {return (_position < _value_len) ? _value_len - _position : 0;}
	virtual int peek() { return (_position < _value_len) ? _value[_position] : -1;}
	virtual void flush() {}
	virtual bool truncate(uint64_t size = 0) {Serial.printf("))truncate %u\n", (uint32_t)size);return true;};
	virtual bool seek(uint64_t pos, int mode) {Serial.printf("))seek %u %u\n", (uint32_t)pos, mode);_position = pos; return false;} // should look at mode
	virtual uint64_t position()  { return _position;}
	virtual uint64_t size()  { return _value_len;}
	virtual void close() {_fOpen = false; }
	virtual bool isOpen() {return _fOpen;}
	virtual const char* name() { return _name; }
	virtual bool isDirectory() { return false;}
	virtual File openNextFile(uint8_t mode = 0) {return File();}
	virtual void rewindDirectory(void) {};
	virtual bool setCreateTime(const DateTimeFields &tm) { return false;}
	virtual bool setModifyTime(const DateTimeFields &tm) { return false;}

	bool _fOpen;
	uint8_t _pin_index;
	char _name[10]; // enough room for a name.
	uint8_t _value[3];
	uint8_t _value_len = 2;
	uint8_t _position;

	void updateValue() {
		_value[0] = digitalRead(_pin_index) ? '1' : '0';
		_value[1] = '\n';
		_value[2] = '\0';
		_value_len = 2;		
	}

	static uint8_t mapFilenameToIndex(const char *filename) {
		uint8_t index = 0;
		while ((*filename >= '0') && (*filename <= '9') ) {
			index = index * 10 + *filename++ - '0';
		}
		if (*filename == '\0') return index;
		return 0xff;
	}
};

//-----------------------------------------------------------------------------
// Analog pins file
//-----------------------------------------------------------------------------
extern float tempmonGetTemp(void);
class TFSAPFile : public  FileImpl {
public:
	TFSAPFile(uint8_t pin_index, uint8_t mode) {
		_pin_index = pin_index;
		_fOpen = true;
		// lets get the current analog voltage for that pin.
		if (_pin_index == 0) snprintf(_value, sizeof(_value), "%6.2f\n", tempmonGetTemp()); // temp
		else if (_pin_index == 1) snprintf(_value, sizeof(_value), "%4u\n", 0); // voltage remember where?
		else snprintf(_value, sizeof(_value), "%4u\n", analogRead(_pin_index - 2)); // random pin.
		_value_len = strlen(_value);
		Serial.printf("\n@@@@A@@@@@ TFSAPFile::TFSAPFile %u %s=%s\n", _pin_index, _names[_pin_index], _value);
	};

	~TFSAPFile() {close(); }

	virtual size_t read(void *buf, size_t nbyte) {
		char *p = (char*)buf;
		int cb = 0;
		while (nbyte && (_position < _value_len)) {
			*p++ = _value[_position++];
			nbyte--;
			cb++;
		}
		return cb;
	}
	virtual size_t write(const void *buf, size_t size) { return 0; }
	virtual int available() {return (_position < _value_len) ? _value_len - _position : 0;}
	virtual int peek() { return (_position < _value_len) ? _value[_position] : -1;}
	virtual void flush() {}
	virtual bool truncate(uint64_t size = 0) {return true;};
	virtual bool seek(uint64_t pos, int mode) {_position = pos; return false;} // should look at mode
	virtual uint64_t position()  { return _position;}
	virtual uint64_t size()  { return _value_len;}
	virtual void close() {_fOpen = false; }
	virtual bool isOpen() {return _fOpen;}
	virtual const char* name() { return _names[_pin_index]; }
	virtual bool isDirectory() { return false;}
	virtual File openNextFile(uint8_t mode = 0) {return File();}
	virtual void rewindDirectory(void) {};
	virtual bool setCreateTime(const DateTimeFields &tm) { return false;}
	virtual bool setModifyTime(const DateTimeFields &tm) { return false;}
	bool _fOpen = false;
	uint8_t _pin_index;
	uint8_t _position = 0;
	char _value[10] = "0000\n";
	uint8_t _value_len = _value_len;
	static const char *_names[12];
	static uint8_t mapFilenameToIndex(const char *filename) {
		for (uint8_t index = 0; index < (sizeof(_names) / sizeof(_names[0])); index++) {
			if (strcmp(filename, _names[index]) == 0) return index;
		}
		return 0xff;
	}
};
const char *TFSAPFile::_names[12] = {"Temp.txt", "Volt.txt", "A0.txt", "A1.txt", "A2.txt", 
		"A3.txt", "A4.txt", "A5.txt", "A6.txt", "A7.txt", "A8.txt", "A9.txt"};

//-----------------------------------------------------------------------------
// Files in the root
//-----------------------------------------------------------------------------
class TFSRootFile : public  FileImpl {
public:
	TFSRootFile(uint8_t root_index, uint8_t mode) {
		_root_index = root_index;
		_fOpen = true;
		_child_index = 0;
	};
	enum {NUM_CHILD_DIGITAL = 24, NUM_CHILD_ANALOG = 12};
	~TFSRootFile() {close(); }
	virtual size_t read(void *buf, size_t nbyte) { return 0; }
	virtual size_t write(const void *buf, size_t size) { return 0; }
	virtual int available() { return 0;}
	virtual int peek() {return -1;}
	virtual void flush() {}
	virtual bool truncate(uint64_t size = 0) {return true;};
	virtual bool seek(uint64_t pos, int mode) {return false;}
	virtual uint64_t position()  { return 0;}
	virtual uint64_t size()  { return 0;}
	virtual void close() {_fOpen = false; }
	virtual bool isOpen() {return _fOpen;}
	virtual const char* name() {
		switch (_root_index) {
		case 0: return (const char *)F("Digital"); break;
		default: return (const char *)F("Analog"); break;
		}
	}
	virtual bool isDirectory() { return true;}
	virtual File openNextFile(uint8_t mode = 0) {
		if (_root_index == 0) {
			if (_child_index < NUM_CHILD_DIGITAL) return File(new TFSDPFile(_child_index++, mode));
		} else {
			if (_child_index < NUM_CHILD_ANALOG) return File(new TFSAPFile(_child_index++, mode));
		}
		return File();
	}
	virtual void rewindDirectory(void) {_child_index = 0;};
	virtual bool setCreateTime(const DateTimeFields &tm) { return false;}
	virtual bool setModifyTime(const DateTimeFields &tm) { return false;}
	bool _fOpen;
	uint8_t _child_index = 0;
	uint8_t _root_index;
};


//-----------------------------------------------------------------------------
// root
//-----------------------------------------------------------------------------
class TFSRoot : public  FileImpl {
public:
	TFSRoot(int8_t mode) {
		_fOpen = true;
		_child_index = 0;
	};
	enum {NUM_CHILD_OBJECTS = 2};
	~TFSRoot() {close(); }
	virtual size_t read(void *buf, size_t nbyte) { return 0; }
	virtual size_t write(const void *buf, size_t size) { return 0; }
	virtual int available() { return 0;}
	virtual int peek() {return -1;}
	virtual void flush() {}
	virtual bool truncate(uint64_t size = 0) {return true;};
	virtual bool seek(uint64_t pos, int mode) {return false;}
	virtual uint64_t position()  { return 0;}
	virtual uint64_t size()  { return 0;}
	virtual void close() {_fOpen = false; }
	virtual bool isOpen() {return _fOpen;}
	virtual const char* name() {return "/";}
	virtual bool isDirectory() { return true;}
	virtual File openNextFile(uint8_t mode = 0) {
		if (_child_index < NUM_CHILD_OBJECTS) return File(new TFSRootFile(_child_index++, mode));
		return File();
	}
	virtual void rewindDirectory(void) {_child_index = 0;};
	virtual bool setCreateTime(const DateTimeFields &tm) { return false;}
	virtual bool setModifyTime(const DateTimeFields &tm) { return false;}
	bool _fOpen;
	int8_t _child_index = 0;
};

//-----------------------------------------------------------------------------
// File System
//-----------------------------------------------------------------------------
class TeensyFS : public FS
{
public:
	TeensyFS() {};
	virtual File open(const char *filename, uint8_t mode = FILE_READ);
	virtual bool exists(const char *filepath) {Serial.printf("TeensyFS::exists(%s)\n", filepath); return true;}
	virtual bool mkdir(const char *filepath)  {Serial.printf("TeensyFS::mkdir(%s)\n", filepath); return true;}
	virtual bool rename(const char *oldfilepath, const char *newfilepath) {Serial.printf("TeensyFS::rename(%s, %s)\n", oldfilepath, newfilepath); return true;}
	virtual bool remove(const char *filepath)  {Serial.printf("TeensyFS::remove(%s)\n", filepath); return true;}
	virtual bool rmdir(const char *filepath)  { Serial.printf("TeensyFS::rmdir(%s)\n", filepath); return true;}
	virtual uint64_t usedSize()  { return 0ul;}
	virtual uint64_t totalSize() { return 1000ul;}
};

File TeensyFS::open(const char *filename, uint8_t mode) {
	// This is crud...
	Serial.printf("@@@ TeensyFS::open %s %u\n", filename, mode);
	if (strcmp(filename, "/") == 0) return File(new TFSRoot(mode));
	if (strcmp(filename, "/Digital") == 0) return File(new TFSRootFile(0, mode));
	if (strcmp(filename, "/Analog") == 0) return File(new TFSRootFile(1, mode));
	if (strncmp(filename, "/Digital/", 9) == 0) {
		uint8_t index = TFSDPFile::mapFilenameToIndex(&filename[9]);
		if (index != 0XFF) return File(new TFSDPFile(index, mode));
	}
	else if (strncmp(filename, "/Analog/", 8) == 0) {
		uint8_t index = TFSAPFile::mapFilenameToIndex(&filename[8]);
		if (index != 0XFF) return File(new TFSAPFile(index, mode));
	}
	return File();
}

#endif // __TeensyFS_H__
