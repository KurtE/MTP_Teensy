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
#include "MTP_Teensy.h"
#include "MTP_Storage.h"

// This code should only be build and run if the SD library was included in the sketch
#if defined(__has_include) && __has_include(<SD.h>)
#include <SD.h>
// I am going to create a simple class with a constructor and maybe some simple data. 
// that can be associated with the SD...
// This is sort of a prototype, could expand to something more later.
class MTP_SD_Callback {
public:

	// constructor
	MTP_SD_Callback() {
		MTP.storage()->registerClassLoopCallback(MTP_FSTYPE_SD, &checkMediaPresent);   
	}
	
	// static callback for device check (media present)
	static bool checkMediaPresent(uint8_t storage_index, FS *pfs);

	static uint8_t media_present_prev_[MTPD_MAX_FILESYSTEMS];

};

uint8_t MTP_SD_Callback::media_present_prev_[MTPD_MAX_FILESYSTEMS] = {0};

// define one here so it's constructor will be called. 
static MTP_SD_Callback mtsdcb;


bool MTP_SD_Callback::checkMediaPresent(uint8_t storage_index, FS *pfs) {
	// we will assume that this is an SD object	
	bool storage_changed = false;
	SDClass *sdfs = (SDClass *)pfs;
	elapsedMicros emTest = 0;
	bool media_present = sdfs->mediaPresent();
	//Serial.printf("MTP_SD_Callback::checkMediaPresent(%u, %p)\n", storage_index, pfs);
	switch (media_present_prev_[storage_index]) {
	case 0: media_present_prev_[storage_index] = media_present ? 0x1 : 0xff;
	  break;
	case 1:
	  if (!media_present) {
	    MTP_class::PrintStream()->printf("SD Disk %s(%u) removed dt:%u\n", 
	    								MTP.storage()->get_FSName(storage_index), storage_index, (uint32_t)emTest);
	    media_present_prev_[storage_index] = 0xff;
#if 1
		MTP.send_StoreRemovedEvent(storage_index);
		delay(5);
		MTP.send_StoreAddedEvent(storage_index);
#else
	    MTP.send_StorageInfoChangedEvent(storage_index);
#endif	    
		storage_changed = true;
	  }
	  break;
	default:  
	  if (media_present) {
	    MTP_class::PrintStream()->printf("SD Disk %s(%u) inserted dt:%u\n", 
	    								MTP.storage()->get_FSName(storage_index), storage_index, (uint32_t)emTest);
	    media_present_prev_[storage_index] = 0x1;
#if 1
		MTP.send_StoreRemovedEvent(storage_index);
		delay(5);
		MTP.send_StoreAddedEvent(storage_index);
#else
	    MTP.send_StorageInfoChangedEvent(storage_index);
#endif	    
	  	storage_changed = true;
	  }
	}
  return storage_changed;
}
#endif
