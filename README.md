# MTP_Teensy

## Readme updates are in progress. 

*Warning* this version requires Teensyduino 1.57-beta1 or later to build.
You can download this version from the lines on the page https://forum.pjrc.com/threads/70196"

MTP Responder for Teensy 3.x and 4.x

Uses SD interface and Bill Greiman's SdFat_V2 as distributed via Teenyduino supporting exFAT and SDIO

code is based on https://github.com//WMXZ-EU/MTP_t4 with modification by WMXZ

code is based on https://github.com/yoonghm/MTP with modification by WMXZ

see also https://forum.pjrc.com/threads/43050-MTP-Responder-Contribution for discussions

## Features
 - Supports multiple MTP-disks (SDIO, multiple SPI disks, LittleFS_xxx disks)
 - copying files from Teensy to PC  and from PC to Teensy is working
 - disk I/O to/from PC is buffered to get some speed-up overcoming uSD latency issues
 - both Serialemu and true Serial may be used- True Serial port is, however, showing up as Everything in Com port. This is a workaround to get Serial working.
 - deletion of files
 - recursive deletion of directories
 - creation of directories
 - moving files and directories within and cross MTP-disk disks
 - copying files and directories within and cross MTP-disk disks
 - creation and modification timestamps are shown 

## Limitations
 - Maximal filename length is 256 but can be changed in Storage.h by changing the MAX_FILENAME_LEN definition
 - within-MTP copy not yet implemented (i.e no within-disk and cross-disk copy)
 - creation of files using file explorer is not supported, but directories can be created
 
## Reset of Session
Modification of disk content (directories and Files) by Teensy is only be visible on PC when done before mounting the MTP device. To refresh disk content it is necessary to unmount and remount Teensy MTP device. AFAIK: On Windows this can be done by using device manager and disable and reanable Teensy (found under portable Device). On Linux this is done with standard muount/unmount commands.

Session may be reset from Teensy by sending a reset event. This is shown in mtp-test example where sending the character 'r' from PC to Teensy generates a reset event. It is suggested to close file explorer before reseting mtp

## Examples
 - mtp-test:   basic MTP test program
 - mtp-logger: basic data logger with MTP access
 
## Installation:

 ## Known Issues
   - copying of files and directories work but are not displayed in file explorer, manual unmount/mount sequence required
   - deleting large nested directories may generate a time-out error and brick MTP. No work around known, only restart Teensy
    
 ## ToBeDone

