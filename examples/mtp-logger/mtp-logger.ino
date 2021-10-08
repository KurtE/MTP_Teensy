#include "Arduino.h"

#define NDAT 128L
#define NCH_ACQ 1
#define NCH_I2S 4
#define NBUF_ACQ (NCH_ACQ * NDAT)
#define NBUF_I2S (NCH_I2S * NDAT)
#define N_ADC 2
#define FRAME_I2S (NCH_I2S / N_ADC)

#define I2S_CONFIG 1
// CONFIG 1 (PURE RX)
// PIN  36 BCLK
// PIN  37 FS
// PIN  13 RXD0
// PIN  38 RXD1

#include "MTP_Teensy.h"
#include "SD.h"

#if defined(__IMXRT1062__)
#include "usb_mtp.h"
#endif

#define USE_SD 1
#define USE_LITTLEFS 1 // set to zero if no LtttleFS is existing or to be used

/****  Start device specific change area  ****/
#if USE_SD == 1
// edit SPI to reflect your configuration (following is for T4.1)
#define SD_MOSI 11
#define SD_MISO 12
#define SD_SCK 13

#define SPI_SPEED SD_SCK_MHZ(16) // adjust to sd card

// SDClasses
const char *sd_str[] = {"sdio", "sd6"}; // edit to reflect your configuration
const int cs[] = {BUILTIN_SDCARD, 38};  // edit to reflect your configuration
const int nsd = sizeof(cs) / sizeof(int);

SDClass sdx[nsd];
#endif

// LittleFS classes
#if USE_LITTLEFS == 1
#include "LittleFS.h"
const char *lfs_str[] = {"RAM1", "RAM2"}; // edit to reflect your configuration
const int lfs_size[] = {2'000'000, 4'000'000};
const int nfs = sizeof(lfs_size) / sizeof(int);

LittleFS_RAM
    ramfs[nfs]; // needs to be declared if LittleFS is used in storage.h
#endif

MTPStorage storage;
MTPD mtpd(&storage);

void storage_configure() {
#if USE_SD == 1
#if defined SD_SCK
  SPI.setMOSI(SD_MOSI);
  SPI.setMISO(SD_MISO);
  SPI.setSCK(SD_SCK);
#endif

  for (int ii = 0; ii < nsd; ii++) {
    if (cs[ii] == BUILTIN_SDCARD) {
      if (!sdx[ii].sdfs.begin(SdioConfig(FIFO_SDIO))) {
        Serial.println("No storage");
        while (1)
          ;
      };
      storage.addFilesystem(sdx[ii], sd_str[ii]);
    } else if (cs[ii] < BUILTIN_SDCARD) {
      pinMode(cs[ii], OUTPUT);
      digitalWriteFast(cs[ii], HIGH);
      if (!sdx[ii].sdfs.begin(SdSpiConfig(cs[ii], SHARED_SPI, SPI_SPEED))) {
        Serial.println("No storage");
        while (1)
          ;
      }
      storage.addFilesystem(sdx[ii], sd_str[ii]);
    }
    uint64_t totalSize = sdx[ii].totalSize();
    uint64_t usedSize = sdx[ii].usedSize();
    Serial.printf("Storage %d %d %s ", ii, cs[ii], sd_str[ii]);
    Serial.print(totalSize);
    Serial.print(" ");
    Serial.println(usedSize);
  }
#endif

#if USE_LITTLEFS == 1
  for (int ii = 0; ii < nfs; ii++) {
    {
      if (!ramfs[ii].begin(lfs_size[ii])) {
        Serial.println("No storage");
        while (1)
          ;
      }
      storage.addFilesystem(ramfs[ii], lfs_str[ii]);
    }
    uint64_t totalSize = ramfs[ii].totalSize();
    uint64_t usedSize = ramfs[ii].usedSize();
    Serial.printf("Storage %d %s ", ii, lfs_str[ii]);
    Serial.print(totalSize);
    Serial.print(" ");
    Serial.println(usedSize);
  }
#endif
}
/****  End of device specific change area  ****/

void logg(uint32_t del, const char *txt);

int16_t state;
int16_t do_logger(uint16_t store, int16_t state);
int16_t do_menu(int16_t state);
int16_t check_filing(int16_t state);

void acq_init(int32_t fsamp);
int16_t acq_check(int16_t state);

void setup() {
  while (!Serial && millis() < 3000)
    ;
  Serial.println("MTP logger");

  usb_mtp_configure();
  storage_configure();

  acq_init(93750); // is fixed for this example, to be modified below
  state = -1;

  Serial.println("Setup done");
  Serial.println(" Enter s to start acquisition and q to stop acquisition");
  Serial.flush();
}

uint32_t loop_count = 0;
void loop() {
  loop_count++;
  state = do_menu(state);
  state = acq_check(state);
  state = check_filing(state);
  //
  if (state < 0)
    mtpd.loop();
  else
    state = do_logger(0, state);

  if (state >= 0)
    logg(1000, "loop");
  // asm("wfi"); // may wait forever on T4.x
}

/**************** Online logging *******************************/
extern uint32_t loop_count, acq_count, acq_miss, maxDel;
extern uint16_t maxCount;
void logg(uint32_t del, const char *txt) {
  static uint32_t to;
  if (millis() - to > del) {
    Serial.printf("%s: %6d %4d %4d %4d %4d %d\n", txt, loop_count, acq_count,
                  acq_miss, maxCount, maxDel, state);
    loop_count = 0;
    acq_count = 0;
    acq_miss = 0;
    maxCount = 0;
    maxDel = 0;
    to = millis();
  }
}

/*************************** Circular Buffer ********************/
#if defined ARDUINO_TEENSY41
#define HAVE_PSRAM 1
#else
#define HAVE_PSRAM 0
#endif

#if HAVE_PSRAM == 1
#define MAXBUF (1000) // 3000 kB   // < 5461 for 16 MByte PSRAM

//    extern "C" uint8_t external_psram_size;
//    uint8_t size = external_psram_size;
//    uint32_t *memory_begin = (uint32_t *)(0x70000000);
//    uint32_t *data_buffer = memory_begin;

uint32_t *data_buffer =
    (uint32_t *)extmem_malloc(MAXBUF * 128 * sizeof(uint32_t));
#else
#if defined(ARDUINO_TEENSY41)
#define MAXBUF (46) // 138 kB
#elif defined(ARDUINO_TEENSY40)
#define MAXBUF (46) // 138 kB
#elif defined(__MK66FX1M0__)
#define MAXBUF (46) // 138 kB
#elif defined(__MK20DX256__)
#define MAXBUF (12) // 36 kB
#endif
uint32_t data_buffer[MAXBUF * NBUF_ACQ];
#endif

static uint16_t front_ = 0, rear_ = 0;
uint16_t getCount() {
  if (front_ >= rear_)
    return front_ - rear_;
  return front_ + MAXBUF - rear_;
}
uint16_t maxCount = 0;

void resetData(void) {
  front_ = 0;
  rear_ = 0;
}

uint16_t pushData(uint32_t *src) {
  uint16_t f = front_ + 1;
  if (f >= MAXBUF)
    f = 0;
  if (f == rear_)
    return 0;

  uint32_t *ptr = data_buffer + f * NBUF_ACQ;
  memcpy(ptr, src, NBUF_ACQ * 4);
  front_ = f;
  //
  uint16_t count;
  count = (front_ >= rear_) ? (front_ - rear_) : front_ + (MAXBUF - rear_);
  if (count > maxCount)
    maxCount = count;
  //
  return 1;
}

uint16_t pullData(uint32_t *dst, uint32_t ndbl) {
  uint16_t r = (rear_ / ndbl);
  if (r == (front_ / ndbl))
    return 0;
  if (++r >= (MAXBUF / ndbl))
    r = 0;
  uint32_t *ptr = data_buffer + r * ndbl * NBUF_ACQ;
  memcpy(dst, ptr, ndbl * NBUF_ACQ * 4);
  rear_ = r * ndbl;
  return 1;
}

/*************************** Filing *****************************/
int16_t file_open(uint16_t store);
int16_t file_writeHeader(void);
int16_t file_writeData(void *diskBuffer, uint32_t ndbl);
int16_t file_close(void);
#define NDBL 1
#define NBUF_DISK (NDBL * NBUF_ACQ)
uint32_t diskBuffer[NBUF_DISK];
uint32_t maxDel = 0;

int16_t do_logger(uint16_t store, int16_t state) {
  uint32_t to = millis();
  if (pullData(diskBuffer, NDBL)) {
    if (state == 0) { // acquisition is running, need to open file
      if (!file_open(store))
        return -2;
      state = 1;
    }
    if (state == 1) { // file just opended, need to write header
      if (!file_writeHeader())
        return -3;
      state = 2;
    }
    if (state >= 2) { // write data to disk
      if (!file_writeData(diskBuffer, NBUF_DISK * 4))
        return -4;
    }
  }

  if (state == 3) { // close file, but continue acquisition
    if (!file_close())
      return -5;
    state = 0;
  }

  if (state == 4) { // close file and stop acquisition
    if (!file_close())
      return -6;
    state = -1;
  }

  uint32_t dt = millis() - to;
  if (dt > maxDel)
    maxDel = dt;

  return state;
}

/******************** Menu ***************************/
void do_menu1(void);
void do_menu2(void);
void do_menu3(void);

int16_t do_menu(int16_t state) { // check Serial input
  if (!Serial.available())
    return state;
  char cc = Serial.read();
  switch (cc) {
  case 's': // start acquisition
    if (state >= 0)
      return state;
    state = 0;
    Serial.println("\nStart");
    break;
  case 'q': // stop acquisition
    if (state < 0)
      return state;
    state = 4;
    Serial.println("\nStop");
    break;
  case '?': // get parameters
    do_menu1();
    break;
  case '!': // set parameters
    if (state >= 0)
      return state;
    do_menu2();
    break;
  case ':': // misc commands
    if (state >= 0)
      return state;
    do_menu3();
    break;
  default:
    break;
  }
  return state;
}

/************ Basic File System Interface *************************/
#include "SD.h"
extern SDClass sdx[];
static File mfile;

char header[512];

void makeHeader(char *header);
int16_t makeFilename(char *filename);
int16_t checkPath(uint16_t store, char *filename);

int16_t file_open(uint16_t store) {
  char filename[80];
  if (!makeFilename(filename))
    return 0;
  if (!checkPath(store, filename))
    return 0;
  mfile = sdx[store].open(filename, FILE_WRITE);
  return !(!mfile);
}

int16_t file_writeHeader(void) {
  if (!mfile)
    return 0;
  makeHeader(header);
  size_t nb = mfile.write(header, 512);
  return (nb == 512);
}

int16_t file_writeData(void *diskBuffer, uint32_t nd) {
  if (!mfile)
    return 0;
  uint32_t nb = mfile.write(diskBuffer, nd);
  return (nb == nd);
}

int16_t file_close(void) {
  mfile.close();
  return (!mfile);
}

/*
 * Custom Implementation
 *
 */
/************************ some utilities modified from time.cpp
 * ************************/
// leap year calculator expects year argument as years offset from 1970
#define LEAP_YEAR(Y)                                                           \
  (((1970 + (Y)) > 0) && !((1970 + (Y)) % 4) &&                                \
   (((1970 + (Y)) % 100) || !((1970 + (Y)) % 400)))

static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};

void day2date(uint32_t dd, uint32_t *day, uint32_t *month, uint32_t *year) {
  uint32_t yy = 0;
  uint32_t days = 0;
  while ((unsigned)(days += (LEAP_YEAR(yy) ? 366 : 365)) <= dd) {
    yy++;
  }

  days -= LEAP_YEAR(yy) ? 366 : 365;
  dd -= days; // now it is days in this year, starting at 0

  uint32_t mm = 0;
  uint32_t monthLength = 0;
  for (mm = 0; mm < 12; mm++) {
    monthLength = monthDays[mm];
    if ((mm == 1) && (LEAP_YEAR(yy)))
      monthLength++;
    if (dd >= monthLength) {
      dd -= monthLength;
    } else {
      break;
    }
  }

  *month = mm + 1; // jan is month 1
  *day = dd + 1;   // day of month
  *year = yy + 1970;
}

void date2day(uint32_t *dd, uint32_t day, uint32_t month, uint32_t year) {
  day -= 1;
  month -= 1;
  year -= 1970;
  uint32_t dx = 0;
  for (uint32_t ii = 0; ii < year; ii++) {
    dx += LEAP_YEAR(ii) ? 366 : 365;
  }
  for (uint32_t ii = 0; ii < month; ii++) {
    dx += monthDays[ii];
    if ((ii == 2) && (LEAP_YEAR(year)))
      dx++; // after feb check for leap year
  }
  *dd = dx + day;
}

/************* Menu implementation ******************************/
void do_menu1(void) { // get parameters
}
void do_menu2(void) { // set parameters
}
void do_menu3(void) { // misc commands
}

/****************** File Utilities *****************************/
void makeHeader(char *header) { memset(header, 0, 512); }

int16_t makeFilename(char *filename) {
  uint32_t tt = rtc_get();
  int hh, mm, ss;
  int dd;
  ss = tt % 60;
  tt /= 60;
  mm = tt % 60;
  tt /= 60;
  hh = tt % 24;
  tt /= 24;
  dd = tt;
  sprintf(filename, "/%d/%02d_%02d_%02d.raw", dd, hh, mm, ss);
  Serial.println(filename);
  return 1;
}

int16_t checkPath(uint16_t store, char *filename) {
  int ln = strlen(filename);
  int i1 = -1;
  for (int ii = 0; ii < ln; ii++)
    if (filename[ii] == '/')
      i1 = ii;
  if (i1 < 0)
    return 1; // no path
  filename[i1] = 0;
  if (!sdx[store].exists(filename)) {
    Serial.println(filename);
    if (!sdx[store].mkdir(filename))
      return 0;
  }

  filename[i1] = '/';
  return 1;
}

uint32_t t_on = 60;
int16_t check_filing(int16_t state) {
  static uint32_t to;
  if (state == 2) {
    uint32_t tt = rtc_get();
    uint32_t dt = tt % t_on;
    if (dt < to)
      state = 3;
    to = dt;
  }
  return state;
}

/****************** Data Acquisition
 * *******************************************/
#define DO_TEST 1
#define DO_I2S 2
#define DO_ACQ DO_TEST

#if DO_ACQ == DO_TEST
/****************** Intervall timer(dummy example)
 * *****************************/
#include "IntervalTimer.h"

IntervalTimer t1;

static uint32_t acq_buffer[NBUF_ACQ];

uint32_t acq_period = 1000;
int16_t acq_state = -1;
void acq_isr(void);

void acq_init(int32_t fsamp) {
  acq_period = 128'000'000 / fsamp;
  acq_state = 0;
}

void acq_start(void) {
  if (acq_state)
    return;
  resetData();
  t1.begin(acq_isr, acq_period);
  acq_state = 1;
}

void acq_stop(void) {
  if (acq_state <= 0)
    return;
  t1.end();
  acq_state = 0;
}

uint32_t acq_count = 0;
uint32_t acq_miss = 0;

void acq_isr(void) {
  acq_count++;
  for (int jj = 0; jj < NCH_ACQ; jj++) {
    for (int ii = 0; ii < NBUF_ACQ; ii++) {
      acq_buffer[jj + ii * NCH_ACQ] = acq_count;
    }
  }
  if (!pushData(acq_buffer))
    acq_miss++;
}

int16_t acq_check(int16_t state) {
  if (!state) { // start acquisition
    acq_start();
  }
  if (state > 3) { // stop acquisition
    acq_stop();
  }
  return state;
}
#else
// try I2S (not working yet)
static uint32_t tdm_rx_buffer[2 * NBUF_I2S];
static uint32_t acq_rx_buffer[NBUF_ACQ];
#define I2S_DMA_PRIO 6

#include "DMAChannel.h"
DMAChannel dma;

void acq_isr(void);

#if defined(__MK66FX1M0__)
// Teensy 3.6
#define MCLK_SRC 3
#define MCLK_SCALE 1

// set MCLK to 48 MHz or a integer fraction (MCLK_SCALE) of it
// MCLK_MULT is set to 1 or 2 to minimize jitter
// this reduces the possibilities for sampling frequencies

#if (F_PLL == 96000000) // PLL is 96 MHz for F_CPU==48 or F_CPU==96 MHz
#define MCLK_MULT 1
#define MCLK_DIV (2 * MCLK_SCALE)
//  #define MCLK_DIV  (3*MCLK_SCALE)
#elif F_PLL == 120000000
#define MCLK_MULT 2
#define MCLK_DIV (5 * MCLK_SCALE)
#elif F_PLL == 144000000
#define MCLK_MULT 1
#define MCLK_DIV (3 * MCLK_SCALE)
#elif F_PLL == 192000000
#define MCLK_MULT 1
#define MCLK_DIV (4 * MCLK_SCALE)
#elif F_PLL == 240000000
#define MCLK_MULT 1
#define MCLK_DIV (5 * MCLK_SCALE)
#else
#error "set F_CPU to (48, 96, 120, 144, 192, 240) MHz"
#endif

#define BIT_DIV 4

/*
 * estimation of sampling frequency
MCLK 98 MHz * 1 / 2 = 48 MHz
N_ADC =1
case 0
nch=8
Bit clock: 93750*(8*32) = 93750*256 = 24 MHz ->(I2S_RCR2_DIV(0))
case 1
nch=4
Bit clock: 93750*(4*32) = 93750*128 = 12 MHz ->(I2S_RCR2_DIV(1))
case 2
nch=2
Bit clock: 93750*(2*32) = 93750*64 = 6 MHz ->(I2S_RCR2_DIV(3))

N_ADC =2
case 3
nch=4
Bit clock: 93750*((4/2)*32) = 93750*64 = 6 MHz ->(I2S_RCR2_DIV(3))

bitclock = fs *( nch*32)/n_adc = f_pll*mckl_mult/mckl_div/(2*bit_div)

fs=bitclock*((nch/n_adc)*32)

bitclock= f_pll*mckl_mult/mckl_div/(2*bit_div)
fs=f_pll*mckl_mult/mckl_div/(2*bit_div)/((nch/n_adc)*32)
*/

const int32_t fsamp0 =
    (((F_PLL * MCLK_MULT) / MCLK_DIV) / (2 * BIT_DIV) / (NCH_I2S * 32 / N_ADC));

void acq_init(int32_t fsamp) {
  Serial.printf("%d %d\n", fsamp, fsamp0);
  SIM_SCGC6 |= SIM_SCGC6_I2S;
  SIM_SCGC7 |= SIM_SCGC7_DMA;
  SIM_SCGC6 |= SIM_SCGC6_DMAMUX;

/*
P23 PTC2   I2S0_TX_FS (6)
P9  PTC3   I2S0_TX_BCLK (6)
P13 PTC5                    I2S0_RXD0 (4)
P11 PTC6   I2S0_MCLK (6)    I2S0_RX_BCLK (4)
P12 PTC7                    I2S0_RX_FS (4)
P35 PTC8                    I2S0_MCLK (4)
P36 PTC9                    I2S0_RX_BCLK (4)
P37 PTC10                   I2S0_RX_FS (4)
P38 PTC11                   I2S0_RXD1 (4)
*/
#if I2S_CONFIG == 0
//            CORE_PIN39_CONFIG = PORT_PCR_MUX(6);  //pin39, PTA17, I2S0_MCLK
//            CORE_PIN11_CONFIG = PORT_PCR_MUX(4);  //pin11, PTC6,  I2S0_RX_BCLK
//            CORE_PIN12_CONFIG = PORT_PCR_MUX(4);  //pin12, PTC7,  I2S0_RX_FS
//            CORE_PIN13_CONFIG = PORT_PCR_MUX(4);  //pin13, PTC5,  I2S0_RXD0
#elif I2S_CONFIG == 1
  CORE_PIN35_CONFIG = PORT_PCR_MUX(4); // PTC8,  I2S0_MCLK
  CORE_PIN36_CONFIG = PORT_PCR_MUX(4); // PTC9,  I2S0_RX_BCLK
  CORE_PIN37_CONFIG = PORT_PCR_MUX(4); // PTC10, I2S0_RX_FS
#elif I2S_CONFIG == 2
//            CORE_PIN35_CONFIG = PORT_PCR_MUX(4) | PORT_PCR_SRE | PORT_PCR_DSE;
//            //pin35, PTC8,   I2S0_MCLK (SLEW rate (SRE)?)
//            CORE_PIN36_CONFIG = PORT_PCR_MUX(4);  //pin36, PTC9, I2S0_RX_BCLK
//            CORE_PIN37_CONFIG = PORT_PCR_MUX(4);  //pin37, PTC10,  I2S0_RX_FS
//            CORE_PIN27_CONFIG = PORT_PCR_MUX(6);  //pin27, PTA15,  I2S0_RXD0
#endif

  I2S0_RCSR = 0;

  // enable MCLK output // MCLK = INP *((MULT)/(DIV))
  I2S0_MDR = I2S_MDR_FRACT((MCLK_MULT - 1)) | I2S_MDR_DIVIDE((MCLK_DIV - 1));
  while (I2S0_MCR & I2S_MCR_DUF)
    ;
  I2S0_MCR = I2S_MCR_MICS(MCLK_SRC) | I2S_MCR_MOE;

  I2S0_RMR = 0; // enable receiver mask
  I2S0_RCR1 = I2S_RCR1_RFW(3);

  I2S0_RCR2 =
      I2S_RCR2_SYNC(0) | I2S_RCR2_BCP | I2S_RCR2_BCD // Bit clock in master mode
      |
      I2S_RCR2_DIV((BIT_DIV - 1)); // divides MCLK down to Bitclock (BIT_DIV)*2

  I2S0_RCR4 = I2S_RCR4_FRSZ((FRAME_I2S - 1)) | I2S_RCR4_FSE // frame sync early
              | I2S_RCR4_FSD // Frame sync in master mode
              | I2S_RCR4_MF;

  I2S0_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

  dma.begin(true); // Allocate the DMA channel first

#if N_ADC == 1
  CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // PTC5,  I2S0_RXD0

  I2S0_RCR3 = I2S_RCR3_RCE;

  dma.TCD->SADDR = &I2S0_RDR0;
  dma.TCD->SOFF = 0;
  dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
  dma.TCD->NBYTES_MLNO = 4;
  dma.TCD->SLAST = 0;
#elif N_ADC == 2
  CORE_PIN13_CONFIG = PORT_PCR_MUX(4); // PTC5,  I2S0_RXD0
  CORE_PIN38_CONFIG = PORT_PCR_MUX(4); // PTC11, I2S0_RXD1

  I2S0_RCR3 = I2S_RCR3_RCE_2CH;

  dma.TCD->SADDR = &I2S0_RDR0;
  dma.TCD->SOFF = 4;
  dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
  dma.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_SMLOE |
                             DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
                             DMA_TCD_NBYTES_MLOFFYES_NBYTES(8);
  dma.TCD->SLAST = -8;
#endif
  dma.TCD->DADDR = tdm_rx_buffer;
  dma.TCD->DOFF = 4;
  dma.TCD->CITER_ELINKNO = NBUF_I2S;
  dma.TCD->DLASTSGA = -sizeof(tdm_rx_buffer);
  dma.TCD->BITER_ELINKNO = NBUF_I2S;
  dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
  dma.triggerAtHardwareEvent(DMAMUX_SOURCE_I2S0_RX);
  dma.enable();

  I2S0_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
  dma.attachInterrupt(acq_isr, I2S_DMA_PRIO * 16);
}

void acq_start(void) {}
void acq_stop(void) {}

#elif defined(__IMXRT1062__)
// Teensy 4.x

#define IMXRT_CACHE_ENABLED 2 // 0=disabled, 1=WT, 2= WB

/************************* I2S
 * *************************************************/
void set_audioClock(int nfact, int32_t nmult, uint32_t ndiv,
                    bool force) // sets PLL4
{
  if (!force && (CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_ENABLE))
    return;

  CCM_ANALOG_PLL_AUDIO =
      CCM_ANALOG_PLL_AUDIO_BYPASS | CCM_ANALOG_PLL_AUDIO_ENABLE |
      CCM_ANALOG_PLL_AUDIO_POST_DIV_SELECT(2) // 2: 1/4; 1: 1/2; 0: 1/1
      | CCM_ANALOG_PLL_AUDIO_DIV_SELECT(nfact);

  CCM_ANALOG_PLL_AUDIO_NUM = nmult & CCM_ANALOG_PLL_AUDIO_NUM_MASK;
  CCM_ANALOG_PLL_AUDIO_DENOM = ndiv & CCM_ANALOG_PLL_AUDIO_DENOM_MASK;

  CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_POWERDOWN; // Switch on PLL
  while (!(CCM_ANALOG_PLL_AUDIO & CCM_ANALOG_PLL_AUDIO_LOCK)) {
  }; // Wait for pll-lock

  const int div_post_pll = 1; // other values: 2,4
  CCM_ANALOG_MISC2 &= ~(CCM_ANALOG_MISC2_DIV_MSB | CCM_ANALOG_MISC2_DIV_LSB);
  if (div_post_pll > 1)
    CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_LSB;
  if (div_post_pll > 3)
    CCM_ANALOG_MISC2 |= CCM_ANALOG_MISC2_DIV_MSB;

  CCM_ANALOG_PLL_AUDIO &= ~CCM_ANALOG_PLL_AUDIO_BYPASS; // Disable Bypass
}

void acq_init(int32_t fsamp) {
  CCM_CCGR5 |= CCM_CCGR5_SAI1(CCM_CCGR_ON);

  // if either transmitter or receiver is enabled, do nothing
  if (I2S1_RCSR & I2S_RCSR_RE)
    return;
  // PLL:
  int fs = fsamp;
  int ovr = FRAME_I2S * 32;
  // PLL between 27*24 = 648MHz und 54*24=1296MHz
  int n1 = 4; // 4; //SAI prescaler 4 => (n1*n2) = multiple of 4
  int n2 = 1 + (24000000 * 27) / (fs * ovr * n1);
  Serial.printf("fs=%d, n1=%d, n2=%d, %d (>27 && < 54)\r\n", fs, n1, n2,
                n1 * n2 * (fs / 1000) * ovr / 24000);

  double C = ((double)fs * ovr * n1 * n2) / 24000000;
  int c0 = C;
  int c2 = 10000;
  int c1 = C * c2 - (c0 * c2);
  set_audioClock(c0, c1, c2, true);

  // clear SAI1_CLK register locations
  CCM_CSCMR1 =
      (CCM_CSCMR1 & ~(CCM_CSCMR1_SAI1_CLK_SEL_MASK)) |
      CCM_CSCMR1_SAI1_CLK_SEL(2); // &0x03 // (0,1,2): PLL3PFD0, PLL5, PLL4

  n1 = n1 / 2; // Double Speed for TDM

  CCM_CS1CDR =
      (CCM_CS1CDR &
       ~(CCM_CS1CDR_SAI1_CLK_PRED_MASK | CCM_CS1CDR_SAI1_CLK_PODF_MASK)) |
      CCM_CS1CDR_SAI1_CLK_PRED((n1 - 1))    // &0x07
      | CCM_CS1CDR_SAI1_CLK_PODF((n2 - 1)); // &0x3f

  IOMUXC_GPR_GPR1 = (IOMUXC_GPR_GPR1 & ~(IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL_MASK)) |
                    (IOMUXC_GPR_GPR1_SAI1_MCLK_DIR |
                     IOMUXC_GPR_GPR1_SAI1_MCLK1_SEL(0)); // Select MCLK

  I2S1_RMR = 0;
  I2S1_RCR1 = I2S_RCR1_RFW(4);
  I2S1_RCR2 = I2S_RCR2_SYNC(0) | I2S_TCR2_BCP | I2S_RCR2_MSEL(1) |
              I2S_RCR2_BCD | I2S_RCR2_DIV(0);

  I2S1_RCR4 = I2S_RCR4_FRSZ((FRAME_I2S - 1)) | I2S_RCR4_SYWD(0) | I2S_RCR4_MF |
              I2S_RCR4_FSE | I2S_RCR4_FSD;
  I2S1_RCR5 = I2S_RCR5_WNW(31) | I2S_RCR5_W0W(31) | I2S_RCR5_FBT(31);

  CORE_PIN23_CONFIG = 3; // 1:MCLK
  CORE_PIN21_CONFIG = 3; // 1:RX_BCLK
  CORE_PIN20_CONFIG = 3; // 1:RX_SYNC
#if N_ADC == 1
  I2S1_RCR3 = I2S_RCR3_RCE;
  CORE_PIN8_CONFIG = 3; // RX_DATA0
  IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2;

  dma.TCD->SADDR = &I2S1_RDR0;
  dma.TCD->SOFF = 0;
  dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
  dma.TCD->NBYTES_MLNO = 4;
  dma.TCD->SLAST = 0;
#elif N_ADC == 2
  I2S1_RCR3 = I2S_RCR3_RCE_2CH;
  CORE_PIN8_CONFIG = 3;                  // RX_DATA0
  CORE_PIN6_CONFIG = 3;                  // RX_DATA1
  IOMUXC_SAI1_RX_DATA0_SELECT_INPUT = 2; // GPIO_B1_00_ALT3, pg 873
  IOMUXC_SAI1_RX_DATA1_SELECT_INPUT = 1; // GPIO_B0_10_ALT3, pg 873

  dma.TCD->SADDR = &I2S1_RDR0;
  dma.TCD->SOFF = 4;
  dma.TCD->ATTR = DMA_TCD_ATTR_SSIZE(2) | DMA_TCD_ATTR_DSIZE(2);
  dma.TCD->NBYTES_MLOFFYES = DMA_TCD_NBYTES_SMLOE |
                             DMA_TCD_NBYTES_MLOFFYES_MLOFF(-8) |
                             DMA_TCD_NBYTES_MLOFFYES_NBYTES(8);
  dma.TCD->SLAST = -8;
#endif
  dma.TCD->DADDR = tdm_rx_buffer;
  dma.TCD->DOFF = 4;
  dma.TCD->CITER_ELINKNO = NBUF_I2S;
  dma.TCD->DLASTSGA = -sizeof(tdm_rx_buffer);
  dma.TCD->BITER_ELINKNO = NBUF_I2S;
  dma.TCD->CSR = DMA_TCD_CSR_INTHALF | DMA_TCD_CSR_INTMAJOR;
  dma.triggerAtHardwareEvent(DMAMUX_SOURCE_SAI1_RX);
  dma.enable();

  I2S1_RCSR = I2S_RCSR_RE | I2S_RCSR_BCE | I2S_RCSR_FRDE | I2S_RCSR_FR;
  dma.attachInterrupt(acq_isr, I2S_DMA_PRIO * 16);
}

void acq_start(void) {}
void acq_stop(void) {}
#endif

uint32_t acq_count = 0;
uint32_t acq_miss = 0;

void acq_isr(void) {
  uint32_t daddr;
  uint32_t *src;
  acq_count++;

  daddr = (uint32_t)(dma.TCD->DADDR);
  dma.clearInterrupt();

  if (daddr < (uint32_t)tdm_rx_buffer + sizeof(tdm_rx_buffer) / 2) {
    // DMA is receiving to the first half of the buffer
    // need to remove data from the second half
    src = &tdm_rx_buffer[NBUF_I2S];
  } else {
    // DMA is receiving to the second half of the buffer
    // need to remove data from the first half
    src = &tdm_rx_buffer[0];
  }

#if IMXRT_CACHE_ENABLED >= 1
  arm_dcache_delete((void *)src, sizeof(tdm_rx_buffer) / 2);
#endif

  for (int jj = 0; jj < NCH_ACQ; jj++) {
    for (int ii = 0; ii < NBUF_ACQ; ii++) {
      acq_rx_buffer[jj + ii * NCH_ACQ] = src[jj + ii * NCH_I2S];
    }
  }

  if (!pushData(acq_rx_buffer))
    acq_miss++;
}

int16_t acq_check(int16_t state) {
  if (!state) { // start acquisition
    acq_start();
  }
  if (state > 3) { // stop acquisition
    acq_stop();
  }
  return state;
}
#endif