//=============================================================================
// Simple Sketch that initializes MTP and adds one SD drive as the only
// storage. 
// Notes:
//    If SD.h is included before MTP_Teensy.h, the code will automatically 
//    inert a handler callback that will try to detect when the SD is inserted
//    or removed.
//
//    The host often times will not automatically refresh to show the updated
//    status of the drive after events such as this.  However doing a refresh
//    example F5 on Windows, will update and show the new status.
//
//    If MTP_Teensy.h is included before SD.h, by default, this checking
//    for drive insertions is not installed.  You can tell the addFilesystem that
//    the storage is an SD drive like:
//        MTP.addFilesystem(SD, "SD Card", MTP_FSTYPE_SD);
//
//=============================================================================
#include <SD.h>
#include <MTP_Teensy.h>

#define CS_SD BUILTIN_SDCARD  // Works on T_3.6 and T_4.1
//#define CS_SD 10  // Works on SPI with this CS pin
void setup()
{
  // mandatory to begin the MTP session.
  MTP.begin();

  // Add SD Card
  SD.begin(CS_SD);
  MTP.addFilesystem(SD, "SD Card");
}

void loop() {
  MTP.loop();  //This is mandatory to be placed in the loop code.
}

