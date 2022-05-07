#define DBGSerial Serial

File dataFile, myFile;  // Specifes that dataFile is of File type
int record_count = 0;
bool write_data = false;
uint8_t current_store = 0;
int index_sdio_storage = -1;
int index_sdspi_storage = -1;

#define BUFFER_SIZE_INDEX 128
uint8_t write_buffer[BUFFER_SIZE_INDEX];
#define buffer_mult 4
uint8_t buffer_temp[buffer_mult*BUFFER_SIZE_INDEX];

int bytesRead;
uint32_t drive_index = 0;

// These can likely be left unchanged
#define MYBLKSIZE 2048 // 2048
#define SLACK_SPACE  40960 // allow for overhead slack space :: WORKS on FLASH {some need more with small alloc units}
#define size_bigfile 1024*1024*24  //24 mb file

// Various Globals
const uint32_t lowOffset = 'a' - 'A';
const uint32_t lowShift = 13;
uint32_t errsLFS = 0, warnLFS = 0; // Track warnings or Errors
uint32_t lCnt = 0, LoopCnt = 0; // loop counters
uint64_t rdCnt = 0, wrCnt = 0; // Track Bytes Read and Written
int filecount = 0;
int loopLimit = 0; // -1 continuous, otherwise # to count down to 0
bool bWriteVerify = true;  // Verify on Write Toggle
File file3; // Single FILE used for all functions
