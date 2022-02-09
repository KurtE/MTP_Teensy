//=============================================================================
// Simple image (BMP optional JPEG and PNG) display program with MTP support added.
//=============================================================================

/***************************************************
  Some of this code originated with the spitftbitmap.ino sketch
  that is part of the Adafruit_ILI9341 library. 
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// warning, this sketch uses libraries that are not installed as part of Teensyduino
// ILI9341_t3n - https://github.com/KurtE/ILI9341_t3n 
// JPGDEC - https://github.com/bitbank2/JPEGDEC (also on arduino library manager)
// PNGdec - https://github.com/bitbank2/PNGdec (also on arduino library manager)

// If using the Sparkfun IO Carrier board uncomment this line and
// and using one of the ILI9341 libraries
//#define USE_SF_IOCarrier 1

// optional support for ILI9341_t3n - that adds additional features
//#include <ILI9341_t3n.h>
// optional support for ILI9488_t3 - that adds additional features
//#include <ILI9488_t3.h>
//Optional support for ST7735/ST7789 graphic dislplays
//#include <ST7735_t3.h> // Hardware-specific library
//#include <ST7789_t3.h> // Hardware-specific library
//#define USE_KURTE_MMOD1
//Optional support for RA8875
//#include <SPI.h>
//#include <RA8875.h>
//Optional support for RA8876
//#include <FT5206.h>
//#include <RA8876_t3.h>

// If user did not include any other driver will defalt to ILI9341_t3 which is installed by Teensyduino
#if !defined(_ILI9341_t3NH_) && !defined(_ILI9488_t3H_) && !defined(__ST7735_t3_H_) \
 && !defined(_RA8875MC_H_) && !defined(_RA8876_T3)
#include <ILI9341_t3.h>
#endif

#if defined(_ILI9341_t3NH_) || defined(_ILI9488_t3H_) || defined(__ST7735_t3_H) || defined(__ST7789_t3_H_) //|| defined(ILI9341_SUPPORTS_CLIPPING)
#define TFT_CLIP_SUPPORT
#if !defined(ILI9341_SUPPORTS_CLIPPING)
#define TFT_SUPPORT_FB
#endif
#endif

#ifdef _ILI9341_t3H_
#define TFT_EMULATE_FB
#endif

// don't try on tlc or 3.2
#if !(defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__))
#define TFT_USE_FRAME_BUFFER
#endif

#include <SPI.h>
#include <SD.h>
#include <MTP_Teensy.h>

// optional JPEG support requires external library
// uncomment if you wish to use. 
#include <JPEGDEC.h>

// optional PNG support requires external library
#include <PNGdec.h>

#ifdef ARDUINO_TEENSY41
extern "C" {
extern int8_t external_psram_size;
}
#endif


//****************************************************************************
// This is calibration data for the raw touch data to the screen coordinates
//****************************************************************************
// Warning, These are
#define TS_MINX 337
#define TS_MINY 529
#define TS_MAXX 3729
#define TS_MAXY 3711

//****************************************************************************
// Settings and objects
//****************************************************************************
#if USE_SF_IOCarrier == 1
#define TFT_DC  5
#define TFT_CS 4
#define TFT_RST -1
#else
#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST -1
#endif

// used for XPT2046
#define TOUCH_CS 27
#define TOUCH_TIRQ 26

// RA8875 capacitive IRQ
#define RA8875_INT 6

#define MAXTOUCHLIMIT     1//1...5

// RA8875 capacitive IRQ
#define RA8876_INT 6
#define RA8876_CTPRST 31
#define MAXTOUCHLIMIT     1//1...5


#define SD_CS BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS 10  // Works on SPI with this CS pin

#ifdef  _ILI9341_t3NH_
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
// setup for PJRC/EBAY boards 
#define SUPPORTS_XPT2046_TOUCH

#elif defined(_ILI9488_t3H_)
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
// Note: board may support but may require MISO buffer chip
//#define SUPPORTS_XPT2046_TOUCH

#elif defined(__ST7735_t3_H_) || defined(__ST7789_t3_H_)
// Option 1: use any pins but a little slower
// Note: code will detect if specified pins are the hardware SPI pins
//       and will use hardware SPI if appropriate
// For 1.44" and 1.8" TFT with ST7735 use
//ST7789_t3 tft = ST7789_t3(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCK, TFT_RST);

// For 1.54" or other TFT with ST7789, This has worked with some ST7789
// displays without CS pins, for those you can pass in -1 or 0xff for CS
// More notes by the tft.init call
//ST7789_t3 tft = ST7789_t3(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// Option 2: must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
// For 1.44" and 1.8" TFT with ST7735 use
//ST7735_t3 tft = ST7735_t3(TFT_CS, TFT_DC, TFT_RST);

#ifdef USE_KURTE_MMOD1
#undef TFT_DC
#undef TFT_CS
#undef TFT_RST
#define TFT_DC 4
#define TFT_CS 10
#define TFT_RST 2
#endif
// For 1.54" TFT with ST7789
ST7789_t3 tft = ST7789_t3(TFT_CS,  TFT_DC, TFT_RST);
#undef TOUCH_CS

//#define SCREEN_WIDTH_TFTHEIGHT_144
// for 1.8" display and mini
//#define SCREEN_WIDTH_TFTHEIGHT_160 // for 1.8" and mini display

#elif defined(_RA8875MC_H_)
#undef TFT_RST
#define TFT_RST 9
RA8875 tft = RA8875(TFT_CS, TFT_RST);
#ifdef ARDUINO_TEENSY41
#endif


#elif defined(_RA8876_T3)
#undef TFT_RST
#define TFT_RST 9
RA8876_t3 tft = RA8876_t3(TFT_CS, TFT_RST);
#ifdef ARDUINO_TEENSY41
#endif

#else
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
#define SUPPORTS_XPT2046_TOUCH
#endif

#ifndef BLUE 
#define BLUE  0x001F
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0
#define RED   0xf800
#endif
#if defined(TOUCH_CS) && defined(SUPPORTS_XPT2046_TOUCH)
#include <XPT2046_Touchscreen.h>
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_TIRQ);
#endif

int g_tft_width = 0;
int g_tft_height = 0;
#ifdef TFT_EMULATE_FB
uint16_t *g_frame_buffer = nullptr;
#endif
bool g_use_efb = true;

File rootFile;
File myfile;

bool g_fast_mode = false;
bool g_picture_loaded = false;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 2500

// Options file information
static const PROGMEM char options_file_name[] = "PictureViewOptions.ini";
int g_debug_output = 0;
int g_stepMode = 0;
int g_BMPScale = -1;
int g_JPGScale = 0;
int g_PNGScale = 1;
int g_center_image = 0;
int g_display_image_time = 2500;
int g_background_color = BLACK;


// scale boundaries {2, 4, 8, 16<maybe>}
enum {SCL_HALF=0, SCL_QUARTER, SCL_EIGHTH, SCL_16TH};
int g_jpg_scale_x_above[4];
int g_jpg_scale_y_above[4];

// variables used in some of the display output functions
int g_image_offset_x = 0;
int g_image_offset_y = 0;
uint8_t g_image_scale = 1;
uint32_t g_WRCount = 0;  // debug count how many time writeRect called


//****************************************************************************
// forward function definitions. 
//****************************************************************************
#ifdef TFT_CLIP_SUPPORT
inline void writeClippedRect(int16_t x, int16_t y, int16_t cx, int16_t cy, uint16_t *pixels, bool waitForWRC = true ) {
  tft.writeRect(x + g_image_offset_x, y + g_image_offset_y, cx, cy, pixels);
}
#else
extern void writeClippedRect(int x, int y, int cx, int cy, uint16_t *pixels, bool waitForWRC = true); 
#endif


//****************************************************************************
// Setup
//****************************************************************************
void setup(void) {
  // mandatory to begin the MTP session.
  MTP.begin();

  // Keep the SD card inactive while working the display.
  pinMode(SD_CS, INPUT_PULLUP);
  delay(20);
  #if defined(TOUCH_CS) && defined(SUPPORTS_XPT2046_TOUCH)
  pinMode(TOUCH_CS, OUTPUT); digitalWriteFast(TOUCH_CS, HIGH);
  pinMode(TFT_CS, OUTPUT); digitalWriteFast(TFT_CS, HIGH);
  ts.begin();
  #endif

#if defined(__ST7735_t3_H) || defined(__ST7789_t3_H_)
  // Use this initializer if you're using a 1.8" TFT 128x160 displays
  //tft.initR(INITR_BLACKTAB);

  // Or use this initializer (uncomment) if you're using a 1.44" TFT (128x128)
  //tft.initR(INITR_144GREENTAB);

  // Or use this initializer (uncomment) if you're using a .96" TFT(160x80)
  //tft.initR(INITR_MINI160x80);

  // Or use this initializer (uncomment) for Some 1.44" displays use different memory offsets
  // Try it if yours is not working properly
  // May need to tweek the offsets
  //tft.setRowColStart(32,0);

  // Or use this initializer (uncomment) if you're using a 1.54" 240x240 TFT
  //tft.init(240, 240);   // initialize a ST7789 chip, 240x240 pixels

  // OR use this initializer (uncomment) if using a 2.0" 320x240 TFT:
  tft.init(240, 320);           // Init ST7789 320x240

  // OR use this initializer (uncomment) if using a 240x240 clone 
  // that does not have a CS pin2.0" 320x240 TFT:
  //tft.init(240, 240, SPI_MODE2);           // Init ST7789 240x240 no CS
  tft.setRotation(1);
 #elif defined(_RA8875MC_H_)
  //  begin display: Choose from: RA8875_480x272, RA8875_800x480, RA8875_800x480ALT, Adafruit_480x272, Adafruit_800x480
  tft.begin(RA8875_800x480, 16, 12000000);
  tft.setRotation(0);

  #if defined(RA8875_INT) && defined(USE_FT5206_TOUCH)
  tft.useCapINT(RA8875_INT);//we use the capacitive chip Interrupt out!
  //the following set the max touches (max 5)
  //it can be placed inside loop but BEFORE touched()
  //to limit dinamically the touches (for example to 1)
  tft.setTouchLimit(MAXTOUCHLIMIT);
  tft.enableCapISR(true);//capacitive touch screen interrupt it's armed
  Serial.println("** RA8875 FT5026 touch enabled **");
  #else
  tft.print("you should open RA8875UserSettings.h file and uncomment USE_FT5206_TOUCH!");
  #endif
 
 #elif defined(_RA8875MC_H_)
  //  begin display: Choose from: RA8875_480x272, RA8875_800x480, RA8875_800x480ALT, Adafruit_480x272, Adafruit_800x480
  tft.begin(RA8875_800x480, 16, 12000000);
  tft.setRotation(0);

  #if defined(RA8875_INT)
  tft.useCapINT(RA8875_INT);//we use the capacitive chip Interrupt out!
  //the following set the max touches (max 5)
  //it can be placed inside loop but BEFORE touched()
  //to limit dinamically the touches (for example to 1)
  tft.setTouchLimit(MAXTOUCHLIMIT);
  tft.enableCapISR(true);//capacitive touch screen interrupt it's armed
  Serial.println("** RA8875 FT5026 touch enabled **");
  #else
  tft.print("you should open RA8875UserSettings.h file and uncomment USE_FT5206_TOUCH!");
  #endif


 #elif defined(_RA8876_T3)
  tft.begin();
   #ifdef RA8876_INT
  tft.useCapINT(RA8876_INT, RA8876_CTPRST);//we use the capacitive chip Interrupt out!
  //the following set the max touches (max 5)
  //it can be placed inside loop but BEFORE touched()
  //to limit dinamically the touches (for example to 1)
  tft.setTouchLimit(MAXTOUCHLIMIT);
  tft.enableCapISR(true);//capacitive touch screen interrupt it's armed

   #endif
   tft.backlight(true);
  tft.setRotation(0);
 #else
  tft.begin();
  #if USE_SF_IOCarrier == 1
    tft.invertDisplay(true);
  #endif
  tft.setRotation(1);
 #endif

  g_tft_width = tft.width();
  g_tft_height = tft.height();

  FillScreen(BLUE);
  g_jpg_scale_x_above[0] = (g_tft_width*3)/2;
  g_jpg_scale_x_above[1] = g_tft_width*3;
  g_jpg_scale_x_above[2] = g_tft_width*6;
  g_jpg_scale_x_above[3] = g_tft_width*12;

  g_jpg_scale_y_above[0] = (g_tft_height*3)/2;
  g_jpg_scale_y_above[1] = g_tft_height*3;
  g_jpg_scale_y_above[2] = g_tft_height*6;
  g_jpg_scale_y_above[3] = g_tft_height*12;



#ifdef TFT_EMULATE_FB
#if defined(ARDUINO_TEENSY41) && defined(_RA8875MC_H_)
  if (external_psram_size) {
    g_frame_buffer = (uint16_t *)extmem_malloc(g_tft_width * g_tft_height * sizeof(uint16_t));
    Serial.printf(">>> RA8875/6 and Extmem(%u) - extmem_malloc\n", external_psram_size);
  }
#else
g_frame_buffer = (uint16_t *)malloc(g_tft_width * g_tft_height * sizeof(uint16_t));
#endif
#endif


  //Serial.print(F("Initializing SD card..."));
  tft.println(F("Init SD card..."));
  if (!SD.begin(SD_CS)) {
    tft.setTextSize(2);
    FillScreen(RED);
    while (!SD.begin(SD_CS)) {
      //Serial.println(F("failed to access SD card!"));
      tft.printf("failed to access SD card on cs:%u!\n", SD_CS);
      delay(2000);
    }
  }
  MTP.addFilesystem(SD, "SD Card");


  //Serial.begin(9600);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println(F("Waiting for Arduino Serial Monitor..."));
  while (!Serial) {
    if (millis() > 3000) break;
  }
  if (CrashReport) Serial.print(CrashReport);
  Serial.printf("\nScreen Width: %u Height: %d\n", g_tft_width, g_tft_height);

  rootFile = SD.open("/");

  Serial.println("OK!");
  emDisplayed = g_display_image_time; 

  File optionsFile = SD.open(options_file_name);
  if (optionsFile) {
    ProcessOptionsFile(optionsFile);
    optionsFile.close();
  }
  ShowAllOptionValues();

#if defined( _ILI9341_t3NH_) || defined(_ILI9488_t3H_) || defined(__ST7735_t3_H_) || defined(__ST7789_t3_H_)
  #ifdef TFT_USE_FRAME_BUFFER
  tft.useFrameBuffer(true);
#endif  
#endif
   #ifdef _RA8876_T3
  tft.printTSRegisters(Serial, 0, 33);
  tft.printTSRegisters(Serial, 0x80, 0xb5-0x80);

   #endif

}

//****************************************************************************
// loop
//****************************************************************************
void loop() {
  MTP.loop();
  ProcessTouchScreen();
  // don't process unless time elapsed or g_fast_mode 
  // Timing may depend on which type of display we are using... 
  // if it has logical frame buffer, maybe as soon as we display an image, 
  // try to load the next one, and then wait until the image time to 
  // tell display to update...
  #ifdef TFT_USE_FRAME_BUFFER
  if (!g_fast_mode && !g_stepMode && (!g_picture_loaded) && (emDisplayed < (uint32_t)g_display_image_time)) return; 
  #else
  if (!g_fast_mode && !g_stepMode && (emDisplayed < (uint32_t)g_display_image_time)) return; 
  #endif
  //---------------------------------------------------------------------------
  // Find the next file to read in. 
  //---------------------------------------------------------------------------
  if (!g_picture_loaded) {
    bool did_rewind = false;
    const char *name = nullptr;
    uint8_t name_len;
    bool bmp_file = false;
    bool jpg_file = false;
    bool png_file = false;
    
    Serial.println("\nLoop looking for image file");
    
    File imageFile;
    
    for (;;) {
      imageFile = rootFile.openNextFile();
      if (!imageFile) {
        if (did_rewind) break; // only go around once. 
        rootFile.rewindDirectory();
        imageFile = rootFile.openNextFile();
        did_rewind = true;
      }
      // maybe should check file name quick and dirty
      name = imageFile.name();
      name_len = strlen(name);
      if (!name) continue;

      if((strcmp(&name[name_len-4], ".bmp") == 0) || (strcmp(&name[name_len-4], ".BMP") == 0)) bmp_file = true;
      if((strcmp(&name[name_len-4], ".jpg") == 0) || (strcmp(&name[name_len-4], ".JPG") == 0)) jpg_file = true;
      if(stricmp(&name[name_len-4], ".bmp") == 0) bmp_file = true;
      if(stricmp(&name[name_len-4], ".jpg") == 0) jpg_file = true;
      if(stricmp(&name[name_len-4], ".png") == 0) png_file = true;
      if (stricmp(name, options_file_name) == 0) ProcessOptionsFile(imageFile);
      if ( bmp_file || jpg_file || png_file) break;
    }

    //---------------------------------------------------------------------------
    // Found a file so try to process it. 
    //---------------------------------------------------------------------------
    if (imageFile) {
      #if defined(_RA8876_T3)
      //tft.useCanvas();
      //tft.selectScreen(SCREEN_2);
      #endif
      elapsedMillis emDraw = 0;
      char file_name[MTP_MAX_FILENAME_LEN];
      strncpy(file_name, name, sizeof(file_name));
      g_WRCount = 0;
      if (bmp_file) {
        bmpDraw(imageFile, imageFile.name(), true);

      #ifdef  __JPEGDEC__
      } else if(jpg_file) {
        processJPGFile(name, true);
        imageFile.close();
      #endif  

      #ifdef __PNGDEC__
      } else if(png_file) {
        processPNGFile(name, true);
        imageFile.close();
      #endif
      }
      Serial.printf("!!File:%s Time:%u writeRect calls:%u\n", file_name, (uint32_t)emDraw, g_WRCount);
    } else {
      FillScreen(GREEN);
      tft.setTextColor(WHITE);
      tft.setTextSize(2);
      tft.println(F("No Files Found"));
    }
    g_picture_loaded = true;
  }

  //---------------------------------------------------------------------------
  // If the display has a command to update the screen now, see if we should
  // do now or wait until proper time 
  //---------------------------------------------------------------------------
  #ifdef TFT_USE_FRAME_BUFFER
  if (g_fast_mode || g_stepMode || (emDisplayed >= (uint32_t)g_display_image_time)) {
    if (g_picture_loaded) {
      #if defined( _ILI9341_t3NH_) || defined(_ILI9488_t3H_) || defined(_RA8876_T3) || defined(__ST7735_t3_H_) || defined(__ST7789_t3_H_)
      tft.updateScreen();
      #endif
      #ifdef TFT_EMULATE_FB
      if (g_frame_buffer && g_use_efb) {
        elapsedMillis emOut;
        tft.writeRect(0, 0, g_tft_width, g_tft_height, g_frame_buffer);
        Serial.printf("EFB writeRect(%u %u) %u\n", g_tft_width, g_tft_height, (uint32_t)emOut);
      }
      #endif
    }
    #endif  
    //---------------------------------------------------------------------------
    // Process any keyboard input.  
    //---------------------------------------------------------------------------
    if (g_stepMode) {
      int ch;
      Serial.printf("Step Mode: enter anything to continue");
      while((ch = Serial.read()) == -1) MTP.loop();  // in case at startup...
      while (ch != -1) {
        if (ch == 'd') g_debug_output = !g_debug_output;
        if (ch == 's') g_stepMode = !g_stepMode;
        if (ch == 'f') g_use_efb = !g_use_efb;

        ch = Serial.read();
      }
    } else if (Serial.available()) {
      int ch;
      while(Serial.read() != -1) ;
      Serial.printf("Paused: enter anything to continue");
      while((ch = Serial.read()) == -1) MTP.loop();
      while (ch != -1) {
        if (ch == 'd') g_debug_output = !g_debug_output;
        if (ch == 's') g_stepMode = !g_stepMode;
        if (ch == 'f') g_use_efb = !g_use_efb;
        ch = Serial.read();
      }
    }
    emDisplayed = 0;
    g_picture_loaded = false;
  #ifdef TFT_USE_FRAME_BUFFER
  }
  #endif
}

//=============================================================================
// Options file support - process only if file changed dates (Or first time) 
//    example looking for update file.
// This is a real simple parser x=y where x is string y is int... 
//=============================================================================
DateTimeFields g_dtf_optFileLast = {99}; // not valid so change first time...
#define MAX_KEY_NAME 20
typedef struct {
  const char key_name[MAX_KEY_NAME];
  int *key_value_addr;
} key_name_value_t;

static const PROGMEM key_name_value_t keyNameValues[] = {
  {"Background", &g_background_color},
  {"debug", &g_debug_output},
  {"Step", &g_stepMode},
  {"BMPScale", &g_BMPScale},
  {"JPGScale", &g_JPGScale},
  {"PNGScale", &g_PNGScale},
  {"ScaleXAbove2", &g_jpg_scale_x_above[SCL_HALF]},
  {"ScaleXAbove4", &g_jpg_scale_x_above[SCL_QUARTER]},
  {"ScaleXAbove8", &g_jpg_scale_x_above[SCL_EIGHTH]},
  {"ScaleXAbove16", &g_jpg_scale_x_above[SCL_16TH]},
  {"ScaleYAbove2", &g_jpg_scale_y_above[SCL_HALF]},
  {"ScaleYAbove4", &g_jpg_scale_y_above[SCL_QUARTER]},
  {"ScaleYAbove8", &g_jpg_scale_y_above[SCL_EIGHTH]},
  {"ScaleYAbove16", &g_jpg_scale_y_above[SCL_16TH]},
  {"Center", &g_center_image},
  {"ImageTimeMS", &g_display_image_time}
};

bool ReadOptionsLine(File &optFile, char *key_name, uint8_t sizeof_key, int &key_value) {
  int ch;

  key_value = 0;
  // first lets get key name ignore all whitespace...
  while ((ch = optFile.read()) <= ' ') {
    if (ch < 0) return false;
  }

  uint8_t ich = 0;
  while (ich < (sizeof_key - 1)) {
    if (ch == '=') {
      ch = optFile.read();  
      break;
    }
    key_name[ich++] = ch;
    ch = optFile.read();  
    if (ch < 0) return false; // 
  }
  key_name[ich] = '\0';

  int sign_value = 1;
  if (ch == '-') {
    sign_value = -1;
    ch = optFile.read();  
    if (ch == -1) return false;
  }    

  while ((ch >= '0') && (ch <= '9')) {
    key_value = key_value * 10 + ch - '0';
    ch = optFile.read();  
  }
  // should probably check for other stuff, but...
  key_value *= sign_value;

  // total hacky but allow hex value
  if ((key_value == 0) && ((ch=='x') || (ch=='X'))) {
    ch = optFile.read();  
    for(;;) {
      if ((ch >= '0') && (ch <= '9'))key_value = key_value * 16 + ch - '0';
      else if ((ch >= 'a') && (ch <= 'f'))key_value = key_value * 16 + 10 + ch - 'a';
      else if ((ch >= 'A') && (ch <= 'F'))key_value = key_value * 16 + 10 + ch - 'A';
      else break;
      ch = optFile.read();  
    }    
  } 
  
  return true;
}


bool ProcessOptionsFile(File &optfile) {
  DateTimeFields dtf;
  int key_value;
  char key_name[20];
  if (!optfile) return false;
  if (!optfile.getModifyTime(dtf)) return false; 
  if (memcmp(&dtf, &g_dtf_optFileLast, sizeof(dtf)) == 0) return false; 
  g_dtf_optFileLast = dtf; 
  Serial.printf("Updated Options file found date: M: %02u/%02u/%04u %02u:%02u\n",
      dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );

  // do simple scan through file
  bool found = false;
  while (ReadOptionsLine(optfile, key_name, sizeof(key_name), key_value)) {
    Serial.printf("\t>>%s=%d", key_name, key_value);
    for (uint8_t key_index = 0; key_index < (sizeof(keyNameValues)/sizeof(keyNameValues[0])); key_index++) {
      if (stricmp(key_name, keyNameValues[key_index].key_name) == 0) {
        Serial.printf(" was: %d\n", *(keyNameValues[key_index].key_value_addr));
        *(keyNameValues[key_index].key_value_addr) = key_value;
        found = true;
        break;
      }
    }
    if (!found)Serial.println(" ** Unknown Key **");
  }

  return true;
}

void ShowAllOptionValues() {
  Serial.println("\n----------------------------------");
  Serial.printf("Sketch uses Option file: %s at the root of SD Card\n", options_file_name);
  Serial.println("\t<All key names>=<current key value");
  for (uint8_t key_index = 0; key_index < (sizeof(keyNameValues)/sizeof(keyNameValues[0])); key_index++) {
    Serial.printf("\t%s=%d\n", keyNameValues[key_index].key_name,*(keyNameValues[key_index].key_value_addr));
  }
  Serial.println("----------------------------------\n");
}

#if defined(_RA8875MC_H_)
#ifdef TFT_EMULATE_FB
void FillScreen(uint16_t color) {
  if (g_frame_buffer && g_use_efb) {
    for (uint32_t i = 0; i < g_tft_width * g_tft_height; i++) {
      g_frame_buffer[i] = color;
    }
  } else {
    tft.fillWindow(color); 
  }
} 
#else 
inline void FillScreen(uint16_t color) {tft.fillWindow(color);}
#endif
inline uint16_t Color565(uint8_t r,uint8_t g,uint8_t b) {return tft.Color565(r, g, b);}
inline void   Color565ToRGB(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) {tft.Color565ToRGB(color, r, g, b);}

#elif defined(_RA8876_T3)
void FillScreen(uint16_t color) {
  tft.useCanvas();
  tft.fillScreen(color);
}
inline uint16_t Color565(uint8_t r,uint8_t g,uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
inline void   Color565ToRGB(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) {
 //color565toRGB   - converts 565 format 16 bit color to RGB
    r = (color>>8)&0x00F8;
    g = (color>>3)&0x00FC;
    b = (color<<3)&0x00F8;
  }

#elif defined(__ST7735_t3_H_) || defined(__ST7789_t3_H_)
inline void FillScreen(uint16_t color) {tft.fillScreen(color);}
inline uint16_t Color565(uint8_t r,uint8_t g,uint8_t b) {return tft.Color565(r, g, b);}
inline void   Color565ToRGB(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) {
 //color565toRGB   - converts 565 format 16 bit color to RGB
    r = (color>>8)&0x00F8;
    g = (color>>3)&0x00FC;
    b = (color<<3)&0x00F8;
  }
#else

#ifdef TFT_EMULATE_FB
void FillScreen(uint16_t color) {
  if (g_frame_buffer && g_use_efb) {
    for (int i = 0; i < g_tft_width * g_tft_height; i++) {
      g_frame_buffer[i] = color;
    }
  } else {
    tft.fillScreen(color); 
  }
}  

#else
inline void FillScreen(uint16_t color) {tft.fillScreen(color);}
#endif
inline uint16_t Color565(uint8_t r,uint8_t g,uint8_t b) {return tft.color565(r, g, b);}
inline void   Color565ToRGB(uint16_t color, uint8_t &r, uint8_t &g, uint8_t &b) {tft.color565toRGB(color, r, g, b);}
#endif

//=============================================================================
// BMP support 
//=============================================================================
// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance for tiny AVR chips.

#define BUFFPIXEL 80
void bmpDraw(File &bmpFile, const char *filename, bool fErase) {

//  File     bmpFile;
  int      image_width, image_height;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = image_width; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint16_t buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;


  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    uint32_t bmpFileSize  __attribute__((unused)) = read32(bmpFile); // Read & ignore creator bytes
    //Serial.print(F("File size: ")); Serial.println(bmpFileSize);
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    //Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    uint32_t bmpHdrSize  __attribute__((unused)) = read32(bmpFile);
    //Serial.print(F("Header size: ")); Serial.println(bmpHdrSize);
    image_width  = read32(bmpFile);
    image_height = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      //Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.printf("Image size: %dx%d depth:%u", image_width, image_height, bmpDepth);
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (image_width * 3 + 3) & ~3;

        // If image_height is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(image_height < 0) {
          image_height = -image_height;
          flip      = false;
        }

        g_image_scale = 1;
        if (g_BMPScale > 0) {
          g_image_scale = g_BMPScale; // use what they passed in
        } else if (g_BMPScale < 0) {
          if (image_width > g_tft_width) g_image_scale = (image_width + g_tft_width - 1) / g_tft_width;
          if (image_height > g_tft_height) {
            int yscale = (image_height + g_tft_height - 1) / g_tft_height;
            if (yscale > g_image_scale) g_image_scale = yscale;
          }
        } else {  
          if ((image_width > g_jpg_scale_x_above[SCL_16TH]) || (image_height >  g_jpg_scale_y_above[SCL_16TH])) {
            g_image_scale = 16;
          } else if ((image_width > g_jpg_scale_x_above[SCL_EIGHTH]) || (image_height >  g_jpg_scale_y_above[SCL_EIGHTH])) {
            g_image_scale = 8;
          } else if ((image_width > g_jpg_scale_x_above[SCL_QUARTER]) || (image_height >  g_jpg_scale_y_above[SCL_QUARTER])) {
            g_image_scale = 4;
          } else if ((image_width > g_jpg_scale_x_above[SCL_HALF]) || (image_height >  g_jpg_scale_y_above[SCL_HALF])) {
            g_image_scale = 2;
          }        
        }
        if (g_center_image) {
          g_image_offset_x = (g_tft_width - (image_width / g_image_scale)) / 2;
          g_image_offset_y = (g_tft_height - (image_height / g_image_scale)) / 2;
        } else {
          g_image_offset_x = 0;
          g_image_offset_y = 0;
        }
        Serial.printf("Scale: 1/%d Image Offsets (%d, %d)\n", g_image_scale, g_image_offset_x, g_image_offset_y);

        if (fErase && (((image_width/g_image_scale) < g_tft_width) || ((image_height/g_image_scale) < image_height))) {
          FillScreen((uint16_t)g_background_color);
        }

        // now we will allocate large buffer for SCALE*width
        uint16_t *usPixels = (uint16_t*)malloc(image_width * g_image_scale * sizeof(uint16_t));
        if (usPixels) {
          for (row=0; row<image_height; row++) { // For each scanline...

            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
              pos = bmpImageoffset + (image_height - 1 - row) * rowSize;
            else     // Bitmap is stored top-to-bottom
              pos = bmpImageoffset + row * rowSize;
            if(bmpFile.position() != pos) { // Need seek?
              bmpFile.seek(pos);
              buffidx = sizeof(sdbuffer); // Force buffer reload
            }
            
            uint16_t *pusRow = usPixels + image_width * (row % g_image_scale);  
            for (col=0; col<image_width; col++) { // For each pixel...
              // Time to read more pixel data?
              if (buffidx >= sizeof(sdbuffer)) { // Indeed
                bmpFile.read(sdbuffer, sizeof(sdbuffer));
                buffidx = 0; // Set index to beginning
              }

              // Convert pixel from BMP to TFT format, push to display
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];
              pusRow[col] = Color565(r,g,b);
            } // end pixel
            if (g_image_scale == 1) {
              writeClippedRect(0, row, image_width, 1, pusRow);
            } else {
              ScaleWriteClippedRect(row, image_width, usPixels);
            }
          } // end scanline
          free(usPixels); // free it after we are done
          usPixels = nullptr;
        } // malloc succeeded

      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}



#if defined( __JPEGDEC__) || defined(__PNGDEC__)
void * myOpen(const char *filename, int32_t *size) {
  myfile = SD.open(filename);
  *size = myfile.size();
  return &myfile;
}
void myClose(void *handle) {
  if (myfile) myfile.close();
}
#endif

//=============================================================================
// TFT Helper functions to work on ILI9341_t3
// which doe snot have offset/clipping support
//=============================================================================

#if !defined(TFT_CLIP_SUPPORT)

// helper to helper...
inline void writeRect(int x, int y, int cx, int cy, uint16_t *pixels)
{
#if defined(_RA8876_T3)
  tft.useCanvas();
  tft.putPicture_16bpp(x, y, cx, cy);
  tft.startSend();
  SPI.transfer(RA8876_SPI_DATAWRITE);
  SPI.transfer(pixels, NULL, cx*cy*2);
  tft.endSend(true);
#else
  tft.writeRect(x, y, cx, cy, pixels);
#endif
}

void writeClippedRect(int x, int y, int cx, int cy, uint16_t *pixels, bool waitForWRC) 
{
  x += g_image_offset_x;
  y += g_image_offset_y;
  int end_x = x + cx;
  int end_y = y + cy;
  
  if (g_debug_output) Serial.printf("\t(%d, %d, %d, %d) %p", x, y, cx, cy, pixels);

  if ((x >= 0) && (y >= 0) && (end_x <= g_tft_width) && (end_y <= g_tft_height)) {
    #ifdef TFT_EMULATE_FB
    if (g_frame_buffer && g_use_efb) {
      uint16_t *pfb = &g_frame_buffer[y * g_tft_width + x];
      while(cy--) {
        memcpy(pfb, pixels, cx *2); // output one clipped rows worth
        pfb += g_tft_width;
        pixels += cx;
      }
    } else
    #endif
    {
      writeRect(x, y, cx, cy, pixels);
    }
  
    g_WRCount++;
    if (g_debug_output) Serial.println(" Full");
    if (waitForWRC)WaitforWRComplete();
  // only process if something is visible.   
  } else if ((end_x >= 0) && (end_y >= 0) && (x < g_tft_width) && (y < g_tft_height)) {
    int cx_out = cx;
    int cy_out = cy;
    if (x < 0) {
      pixels += -x; // point to first word we will use. 
      cx_out += x;
      x = 0; 
    }
    if (end_x > g_tft_width) cx_out -= (end_x - g_tft_width);
    if (y < 0) {
      pixels += -y*cx; // point to first word we will use. 
      cy_out += y;
      y = 0; 
    }
    if (end_y > g_tft_height) cy_out -= (end_y - g_tft_height);
    if (cx_out && cy_out) { 
      #ifdef TFT_EMULATE_FB
      if (g_frame_buffer && g_use_efb) {
        uint16_t *pfb = &g_frame_buffer[y * g_tft_width + x];
        while(cy_out--) {
          memcpy(pfb, pixels, cx_out *2); // output one clipped rows worth
          pfb += g_tft_width;
          pixels += cx;
        }
      } else
      #endif
      {
        if (cy_out > 1) {
          //compress the buffer
          uint16_t *pixels_out = pixels;
          uint16_t *p = pixels;
          end_y = cy_out; // reuse variable
          while (--end_y) {
            p += cx_out; // increment to where we will copy the pixels to
            pixels_out += cx; // increment by one full row
            memcpy(p, pixels_out, cx_out*sizeof(uint16_t));
          }
        }
        writeRect(x, y, cx_out, cy_out, pixels);
      }
      if (g_debug_output)Serial.printf(" -> (%d, %d, %d, %d)* %p\n", x, y, cx_out, cy_out, pixels);
      g_WRCount++;
      if (waitForWRC)WaitforWRComplete();
    } else {
      if (g_debug_output)Serial.println(" Clipped");
    }
  } else {
    if (g_debug_output)Serial.println(" Clipped");
  }
} 
void WaitforWRComplete() {
  #if defined(_RA8876_T3)
  // bugbug: ra8876 may use dma code, and since some of our decoders
  // want to reuse the same memory we wait for these to complete
  while(!tft.DMAFinished()) ;
  #endif

}
#endif

// Function to draw pixels to the display
void ScaleWriteClippedRect(int row, int image_width, uint16_t *usPixels) {
  // this methos assumes you are writing the data into the proper spots in Image_width*CLIP_LINES rectangle
  if ((row % g_image_scale) == (g_image_scale -1)) {
    uint16_t newx = 0;
    for(uint16_t pix_cnt=0; pix_cnt < image_width; pix_cnt += g_image_scale) {
      uint8_t red = 0; uint8_t green = 0; uint8_t blue = 0;
      float r =0; float g = 0; float b = 0;
      for (uint8_t i = 0; i < g_image_scale; i++) {
        for (uint8_t j = 0; j < g_image_scale; j++) {
          Color565ToRGB(usPixels[pix_cnt + i + (j*image_width)], red, green, blue); 
          // Sum the squares of components instead 
          r += red * red;
          g += green * green;
          b += blue * blue;
        }
      }
      // overwrite the start of our buffer with 
      usPixels[newx++] = Color565((uint8_t) sqrt(r/(g_image_scale*g_image_scale)), (uint8_t)sqrt(g/(g_image_scale*g_image_scale)), (uint8_t)sqrt(b/(g_image_scale*g_image_scale)));
    }
    writeClippedRect(0, row/g_image_scale, image_width/g_image_scale, 1, usPixels);
  }
}


//=============================================================================
// JPeg support 
//=============================================================================
//used for jpeg files primarily
#ifdef __JPEGDEC__
JPEGDEC jpeg;


void processJPGFile(const char *name, bool fErase)
{
  Serial.println();
  Serial.print(F("Loading JPG image '"));
  Serial.print(name);
  Serial.println('\'');
  uint8_t scale = 1;
  if (jpeg.open(name, myOpen, myClose, myReadJPG, mySeekJPG, JPEGDraw)) {
    int image_width = jpeg.getWidth();
    int image_height = jpeg.getHeight();
    int decode_options = 0;
    Serial.printf("Image size: %dx%d", image_width, image_height);
    switch(g_JPGScale) {
      case 1: scale = 1; decode_options=0; break;
      case 2: scale = 2; decode_options=JPEG_SCALE_HALF; break;
      case 4: scale = 4; decode_options=JPEG_SCALE_QUARTER; break;
      case 8: scale = 8; decode_options=JPEG_SCALE_EIGHTH; break;
      default: 
      {
        if ((image_width > g_jpg_scale_x_above[SCL_16TH]) || (image_height >  g_jpg_scale_y_above[SCL_16TH])) {
          decode_options = JPEG_SCALE_EIGHTH | JPEG_SCALE_HALF;
          scale = 16;
        } else if ((image_width > g_jpg_scale_x_above[SCL_EIGHTH]) || (image_height >  g_jpg_scale_y_above[SCL_EIGHTH])) {
          decode_options = JPEG_SCALE_EIGHTH;
          scale = 8;
        } else if ((image_width > g_jpg_scale_x_above[SCL_QUARTER]) || (image_height >  g_jpg_scale_y_above[SCL_QUARTER])) {
          decode_options = JPEG_SCALE_QUARTER;
          scale = 4;
        } else if ((image_width > g_jpg_scale_x_above[SCL_HALF]) || (image_height >  g_jpg_scale_y_above[SCL_HALF])) {
          decode_options = JPEG_SCALE_HALF;
          scale = 2;
        }        
      }
    }
    if (fErase && ((image_width/scale < g_tft_width) || (image_height/scale < g_tft_height))) {
      FillScreen((uint16_t)g_background_color);
    }

    if (g_center_image) {
      g_image_offset_x = (g_tft_width - image_width/scale) / 2;
      g_image_offset_y = (g_tft_height - image_height/scale) / 2;
    } else {
      g_image_offset_x = 0;
      g_image_offset_y = 0;
    }
    g_image_scale = scale;
    Serial.printf("Scale: 1/%d Image Offsets (%d, %d)\n", g_image_scale, g_image_offset_x, g_image_offset_y);

    jpeg.decode(0, 0, decode_options);
    jpeg.close();
  } else {
    Serial.println("Was not a valid jpeg file");
  }
}


int32_t myReadJPG(JPEGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}
int32_t mySeekJPG(JPEGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}

int JPEGDraw(JPEGDRAW *pDraw) {
  if (g_debug_output) Serial.printf("jpeg draw: x,y=%d,%d, cx,cy = %d,%d\n",
     pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);

  writeClippedRect(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}
#endif

//=============================================================================
// PNG support 
//=============================================================================
//used for png files primarily
#ifdef __PNGDEC__
PNG png;
int g_image_width;
int g_image_height;

void processPNGFile(const char *name, bool fErase)
{
  int rc;  
  
  Serial.println();
  Serial.print(F("Loading PNG image '"));
  Serial.print(name);
  Serial.println('\'');
  rc = png.open((const char *)name, myOpen, myClose, myReadPNG, mySeekPNG, PNGDraw);
  if (rc == PNG_SUCCESS) {
    g_image_width = png.getWidth();
    g_image_height = png.getHeight();
    g_image_scale = 1; // default...
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", g_image_width, g_image_height, png.getBpp(), png.getPixelType());
    if (g_PNGScale > 0) {
      g_image_scale = g_PNGScale; // use what they passed in
    } else if (g_PNGScale < 0) {
      if (g_image_width > g_tft_width) g_image_scale = (g_image_width + g_tft_width - 1) / g_tft_width;
      if (g_image_height > g_tft_height) {
        int yscale = (g_image_height + g_tft_height - 1) / g_tft_height;
        if (yscale > g_image_scale) g_image_scale = yscale;
      }
    } else {  
      if ((g_image_width > g_jpg_scale_x_above[SCL_16TH]) || (g_image_height >  g_jpg_scale_y_above[SCL_16TH])) {
        g_image_scale = 16;
      } else if ((g_image_width > g_jpg_scale_x_above[SCL_EIGHTH]) || (g_image_height >  g_jpg_scale_y_above[SCL_EIGHTH])) {
        g_image_scale = 8;
      } else if ((g_image_width > g_jpg_scale_x_above[SCL_QUARTER]) || (g_image_height >  g_jpg_scale_y_above[SCL_QUARTER])) {
        g_image_scale = 4;
      } else if ((g_image_width > g_jpg_scale_x_above[SCL_HALF]) || (g_image_height >  g_jpg_scale_y_above[SCL_HALF])) {
        g_image_scale = 2;
      }        
    }

    if (fErase && (((g_image_width/g_image_scale) < g_tft_width) || ((g_image_height/g_image_scale) < g_image_height))) {
      FillScreen((uint16_t)g_background_color);
    }

    if (g_center_image) {
      g_image_offset_x = (g_tft_width - (png.getWidth() / g_image_scale)) / 2;
      g_image_offset_y = (g_tft_height - (png.getHeight() / g_image_scale)) / 2;
    } else {
      g_image_offset_x = 0;
      g_image_offset_y = 0;
    }

    Serial.printf("Scale: 1/%d Image Offsets (%d, %d)\n", g_image_scale, g_image_offset_x, g_image_offset_y);
    uint16_t *usPixels = (uint16_t*)malloc(g_image_width * ((g_image_scale==1)? 16 : g_image_scale) * sizeof(uint16_t));
    if (usPixels) {
      rc = png.decode(usPixels, 0);
      png.close();
      free(usPixels);
    } else Serial.println("Error could not allocate line buffer");
  } else {
    Serial.printf("Was not a valid PNG file RC:%d\n", rc);
  }
}

int32_t myReadPNG(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}
int32_t mySeekPNG(PNGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}

// Function to draw pixels to the display
void PNGDraw(PNGDRAW *pDraw) {
  uint16_t *usPixels = (uint16_t*)pDraw->pUser;
  if(g_image_scale == 1) {
    uint16_t *pusRow = usPixels + pDraw->iWidth * (pDraw->y & 0xf); // we have 16 lines to work with
    png.getLineAsRGB565(pDraw, pusRow, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    // but we will output 8 lines at time. 
    if ((pDraw->y == g_image_height - 1) || ((pDraw->y & 0x7) == 0x7)) {
//      WaitforWRComplete(); // make sure previous writes are done
      writeClippedRect(0, pDraw->y & 0xfff8, pDraw->iWidth, (pDraw->y & 0x7) + 1, 
        usPixels + (pDraw->y & 0x8) * pDraw->iWidth, false);
    }
  } else {
    uint16_t *pusRow = usPixels + pDraw->iWidth * (pDraw->y % g_image_scale);  
    png.getLineAsRGB565(pDraw, pusRow, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
    ScaleWriteClippedRect(pDraw->y, pDraw->iWidth, usPixels);
  }
}


#endif

//=============================================================================
// Touch screen support 
//=============================================================================
#if defined(TOUCH_CS) && defined(SUPPORTS_XPT2046_TOUCH)
void ProcessTouchScreen()
{
  // See if there's any  touch data for us
//  if (ts.bufferEmpty()) {
//    return;
//  }

  // You can also wait for a touch
  if (! ts.touched()) {
    g_fast_mode = false;
    return;
  }

  // first hack, if screen pressed go very fast
  g_fast_mode = true;

  // Retrieve a point
  TS_Point p = ts.getPoint();

  // p is in ILI9341_t3 setOrientation 1 settings. so we need to map x and y differently.

  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print("\tPressure = "); Serial.print(p.z);


  // Scale from ~0->4000 to tft.width using the calibration #'s
#if 1 // SCREEN_ORIENTATION_1
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, g_tft_width);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, g_tft_height);
#else
  
  uint16_t px = map(p.y, TS_MAXY, TS_MINY, 0, g_tft_width);
  p.y = map(p.x, TS_MINX, TS_MAXX, 0, g_tft_height);
  p.x = px;
#endif  
    Serial.print(" ("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");

}
#elif defined(_RA8875MC_H_) && defined(RA8875_INT) && defined(USE_FT5206_TOUCH)
void ProcessTouchScreen()
{
  if (tft.touched()){//if touched(true) detach isr
  //at this point we need to fill the FT5206 registers...
    tft.updateTS();//now we have the data inside library
    Serial.print(">> touches:");
    Serial.print(tft.getTouches());
    Serial.print(" | gesture:");
    Serial.print(tft.getGesture(),HEX);
    Serial.print(" | state:");
    Serial.print(tft.getTouchState(),HEX);
    uint16_t coordinates[MAXTOUCHLIMIT][2];//to hold coordinates
    tft.getTScoordinates(coordinates);//done
    //now coordinates has the x,y of all touches
    for (uint8_t i=0;i<=tft.getTouches();i++){
      Serial.printf(" (%d,%d)", coordinates[i][0],coordinates[i][1]);
    }
    tft.enableCapISR();//rearm ISR if needed (touched(true))
    Serial.println();
    //otherwise it doesn't do nothing...
    g_fast_mode = true;
  } else {
    g_fast_mode = false;
  }
}
#elif defined(_RA8876_T3) && defined(RA8876_INT)
void ProcessTouchScreen()
{
  if (tft.touched()){//if touched(true) detach isr
  //at this point we need to fill the FT5206 registers...
    tft.updateTS();//now we have the data inside library
    Serial.print(">> touches:");
    Serial.print(tft.getTouches());
    Serial.print(" | gesture:");
    Serial.print(tft.getGesture(),HEX);
    Serial.print(" | state:");
    Serial.print(tft.getTouchState(),HEX);
    uint16_t coordinates[MAXTOUCHLIMIT][2];//to hold coordinates
    tft.getTScoordinates(coordinates);//done
    //now coordinates has the x,y of all touches
    for (uint8_t i=0;i<=tft.getTouches();i++){
      Serial.printf(" (%d,%d)", coordinates[i][0],coordinates[i][1]);
    }
    tft.enableCapISR();//rearm ISR if needed (touched(true))
    Serial.println();
    //otherwise it doesn't do nothing...
    g_fast_mode = true;
  } else {
    g_fast_mode = false;
  }

} 
#else
void ProcessTouchScreen()
{
}
#endif