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

// This code should only be build and run if the USBHost_t36 library was included in the sketch
#if defined(__has_include) && __has_include(<USBHost_t36.h>)
#include <USBHost_t36.h>
// I am going to create a simple class with a constructor and maybe some simple data.
// that can be associated with the SD...
// This is sort of a prototype, could expand to something more later.
typedef struct {
	USBFSBase *pusbfs; // pointer to object
	uint32_t store_id;
	char storage_name[20];
} map_usbfs_mtp_t;


class MTP_USBFS_Callback {
public:

	// constructor
	MTP_USBFS_Callback() {
		// register to be called once per loop for all of oru dirves.
		MTP.storage()->registerClassLoopCallback(MTP_FSTYPE_USBFS, &checkUSBFSChangedState, false);
	}

	// static callback for device check (media present)
	static bool checkUSBFSChangedState(uint8_t storage_index, FS *pfs);

	enum {MAX_USBFS_MTP_PAIRS = 8};
	static map_usbfs_mtp_t s_map_fs_mtp[MAX_USBFS_MTP_PAIRS];

};

map_usbfs_mtp_t MTP_USBFS_Callback::s_map_fs_mtp[MAX_USBFS_MTP_PAIRS] = {{nullptr, 0xFFFFFFFFUL, "" }};

// define one here so it's constructor will be called.
static MTP_USBFS_Callback mtusbfscb;


bool MTP_USBFS_Callback::checkUSBFSChangedState(uint8_t storage_index, FS *pfs) {
	// if no states changed return fast
	if (!USBFSBase::anyFSChangedState()) return false;
	USBFSBase::anyFSChangedState(0); // clear it out
	MTP_class::PrintStream()->println("\n*** AT least one USBFS Filesystem change state ***");

	bool send_device_reset = false;
	// lets walk the list of file systems
	// we will ignore the pusbfs that was passed in.
	USBFSBase *pusbfs = USBFSBase::nextFS(nullptr);

	while (pusbfs) {
		uint8_t state_changed = pusbfs->stateChanged();
		if (state_changed) {
			
			// something changed.
			// See if it is a valid FS
			// See if we can find the file system already in list or first blank
			uint8_t index_free = MAX_USBFS_MTP_PAIRS;
			uint8_t index;
			for (index = 0; index < MAX_USBFS_MTP_PAIRS; index++) {
				if (s_map_fs_mtp[index].pusbfs == pusbfs) break;
				if ((s_map_fs_mtp[index].pusbfs == nullptr) && (index_free == MAX_USBFS_MTP_PAIRS))
					index_free = index;
			}
			//MTP_class::PrintStream()->printf(">>>%p State changed: %x index:%u\n", pusbfs, state_changed, index);

			if (*pusbfs) {
				// So the Filesystem is valid, so either added or maybe formatted
				if (state_changed == USBFSBase::USBFS_STATE_CHANGE_FORMAT) {
			        // Something with this file system changed. 
					if (index < MAX_USBFS_MTP_PAIRS) {
						MTP_class::PrintStream()->printf("Changed USB Volume:%u %p name:%s store:%u\n", index, pusbfs,
						              s_map_fs_mtp[index].storage_name, s_map_fs_mtp[index].store_id);
				        send_device_reset = true; // assume we need for MTP to reset as who knows which things changed.
				    }

				} else {
					if (index == MAX_USBFS_MTP_PAIRS) index = index_free;
					if (index < MAX_USBFS_MTP_PAIRS) {
						s_map_fs_mtp[index].pusbfs = pusbfs;	// remember this one.
						// Lets see if we can get the volume label:
						char volName[20];
  						#pragma GCC diagnostic push
  						#pragma GCC diagnostic ignored "-Wformat-truncation" /* Or "-Wformat-truncation" */
						if (pusbfs->getVolumeLabel(volName, sizeof(volName)))
							snprintf(s_map_fs_mtp[index].storage_name, sizeof(s_map_fs_mtp[index].storage_name), "MSC%d-%s", index, volName);
						else
							snprintf(s_map_fs_mtp[index].storage_name, sizeof(s_map_fs_mtp[index].storage_name), "MSC%d", index);
						s_map_fs_mtp[index].storage_name[sizeof(s_map_fs_mtp[index].storage_name) - 1] = 0; // make sure null terminated
						#pragma GCC diagnostic pop

						s_map_fs_mtp[index].store_id = MTP.addFilesystem(*pusbfs, s_map_fs_mtp[index].storage_name);

						// Try to send store added. if > 0 it went through = 0 stores have not been enumerated
						MTP_class::PrintStream()->printf("Added USB Volume:%u %p name:%s store:%u\n", index, pusbfs,
						              s_map_fs_mtp[index].storage_name, s_map_fs_mtp[index].store_id);
						if (MTP.send_StoreAddedEvent(s_map_fs_mtp[index].store_id) < 0) send_device_reset = true;
					}
				}
			} else {
				// drive went away.
				if (index < MAX_USBFS_MTP_PAIRS) {
					MTP_class::PrintStream()->printf("Removed USB Volume:%u %p name:%s store:%u\n", index, pusbfs,
					              s_map_fs_mtp[index].storage_name, s_map_fs_mtp[index].store_id);
					if (s_map_fs_mtp[index].store_id != 0xFFFFFFFFUL) {
						if (MTP.send_StoreRemovedEvent(s_map_fs_mtp[index].store_id) < 0) send_device_reset = true;
						MTP.storage()->removeFilesystem(s_map_fs_mtp[index].store_id);
					}
					// clear out the data
					s_map_fs_mtp[index].store_id = 0xFFFFFFFFUL;
					s_map_fs_mtp[index].pusbfs = nullptr;
				}
			}
			pusbfs->stateChanged(0); // clear the state.
		}
		// setup to look at next
		pusbfs = USBFSBase::nextFS(pusbfs);
	}
	return send_device_reset;
}

#endif
