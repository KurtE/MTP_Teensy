#include <LFS_MTP_Callback.h>

//=============================================================================
// try to get the right FS for this store and then call it's format if we have
// one...
// Here is for LittleFS...
uint8_t LittleFSMTPCB::formatStore(MTPStorage_SD *mtpstorage, uint32_t store,
                                   uint32_t user_token, uint32_t p2,
                                   bool post_process) {
  // Lets map the user_token back to oint to our object...
  Serial.printf("Format Callback: user_token:%x store: %u p2:%u post:%u \n",
                user_token, store, p2, post_process);
  LittleFS *lfs = (LittleFS *)user_token;
  // see if we have an lfs
  if (lfs) {
    if (g_lowLevelFormat) {
      Serial.printf("Low Level Format: %s post: %u\n",
                    mtpstorage->getStoreName(store), post_process);
      if (!post_process)
        return MTPStorageInterfaceCB::FORMAT_NEEDS_CALLBACK;

      if (lfs->lowLevelFormat('.'))
        return MTPStorageInterfaceCB::FORMAT_SUCCESSFUL;
    } else {
      Serial.printf("Quick Format: %s\n", mtpstorage->getStoreName(store));
      if (lfs->quickFormat())
        return MTPStorageInterfaceCB::FORMAT_SUCCESSFUL;
    }
  } else
    Serial.println("littleFS not set in user_token");
  return MTPStorageInterfaceCB::FORMAT_NOT_SUPPORTED;
}

void LittleFSMTPCB::set_formatLevel(bool level) { g_lowLevelFormat = level; }