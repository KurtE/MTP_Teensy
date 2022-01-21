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

// optional support for ILI9341_t3n - that adds additional features
//#include <ILI9341_t3n.h>

// If ILI9341_t3n is not included include ILI9341_t3 which is installed by Teensyduino
#ifndef  _ILI9341_t3NH_
#include <ILI9341_t3.h>
#endif

#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <SD.h>
#include <MTP_Teensy.h>

// optional JPEG support requires external library
// uncomment if you wish to use. 
#include <JPEGDEC.h>

// optional PNG support requires external library
#include <PNGdec.h>

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
#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST -1

#define TOUCH_CS 27
#define TOUCH_TIRQ 26

#define SD_CS BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS 6  // Works on SPI with this CS pin



#ifdef  _ILI9341_t3NH_
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);
#else
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
#endif

#ifdef TOUCH_CS
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_TIRQ);
#endif

File rootFile;
File myfile;
bool fast_mode = false;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 2500

// Options file information
static const PROGMEM char options_file_name[] = "PictureViewOptions.ini";
bool g_debug_output = false;
int g_JPGScale = 0;
int g_center_image = 0;

//****************************************************************************
// Setup
//****************************************************************************
void setup(void) {
  // mandatory to begin the MTP session.
  MTP.begin();

  // Keep the SD card inactive while working the display.
  pinMode(SD_CS, INPUT_PULLUP);
  delay(20);
  #ifdef TOUCH_CS
  pinMode(TOUCH_CS, OUTPUT); digitalWriteFast(TOUCH_CS, HIGH);
  pinMode(TFT_CS, OUTPUT); digitalWriteFast(TFT_CS, HIGH);
  !ts.begin();
  #endif

  tft.begin();
  tft.fillScreen(ILI9341_BLUE);

  //Serial.print(F("Initializing SD card..."));
  tft.println(F("Init SD card..."));
  while (!SD.begin(SD_CS)) {
    //Serial.println(F("failed to access SD card!"));
    tft.println(F("failed to access SD card!"));
    delay(2000);
  }
  MTP.addFilesystem(SD, "SD Card");


  //Serial.begin(9600);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setRotation(1);
  tft.println(F("Waiting for Arduino Serial Monitor..."));
  while (!Serial) {
    if (millis() > 3000) break;
  }

  rootFile = SD.open("/");

  Serial.println("OK!");
  emDisplayed = DISPLAY_IMAGES_TIME; 

  File optionsFile = SD.open(options_file_name);
  if (optionsFile) {
    ProcessOptionsFile(optionsFile);
    optionsFile.close();
  }

#ifdef  _ILI9341_t3NH_
  tft.useFrameBuffer(true);
#endif
}

//****************************************************************************
// loop
//****************************************************************************
void loop() {
  MTP.loop();
  #ifdef TOUCH_CS
  ProcessTouchScreen();
  #endif
  // don't process unless time elapsed or fast_mode 
  if (!fast_mode && (emDisplayed < DISPLAY_IMAGES_TIME)) return; 
  bool did_rewind = false;
  const char *name = nullptr;
  uint8_t name_len;
  bool bmp_file = false;
  bool jpg_file = false;
  bool png_file = false;
  
  Serial.println("Loop looking for image file");
  
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
//  tft.fillScreen(ILI9341_BLACK);
  if (imageFile && bmp_file) {
    bmpDraw(imageFile, imageFile.name(), 0, 0, true);

  #ifdef  __JPEGDEC__
  } else if(imageFile && jpg_file) {
    processJPGFile(name, true);
    imageFile.close();
  #endif  

  #ifdef __PNGDEC__
  } else if(imageFile && png_file) {
    processPNGFile(name, true);
    imageFile.close();
  #endif
  } else {
    tft.fillScreen(ILI9341_GREEN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println(F("No Files Found"));
  }
  #ifdef  _ILI9341_t3NH_
  tft.updateScreen();
  #endif
  if (Serial.available()) {
    int ch;
    while(Serial.read() != -1) ;
    Serial.printf("Paused: enter anything to continue");
    while((ch = Serial.read()) == -1) ;
    if (ch == 'd') g_debug_output = !g_debug_output;
    while(Serial.read() != -1) ;

  }
  emDisplayed = 0;
}

//=============================================================================
// Options file support - process only if file changed dates (Or first time) 
//    example looking for update file.
// This is a real simple parser x=y where x is string y is int... 
//=============================================================================
DateTimeFields g_dtf_optFileLast = {99}; // not valid so change first time...

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
  return true;
}


bool ProcessOptionsFile(File &optfile) {
  DateTimeFields dtf;
  int key_value;
  char key_name[32];
  if (!optfile) return false;
  if (!optfile.getModifyTime(dtf)) return false; 
  if (memcmp(&dtf, &g_dtf_optFileLast, sizeof(dtf)) == 0) return false; 
  g_dtf_optFileLast = dtf; 
  Serial.printf("Updated Options file found date: M: %02u/%02u/%04u %02u:%02u\n",
      dtf.mon + 1, dtf.mday, dtf.year + 1900, dtf.hour, dtf.min );

  // do simple scan through file
  while (ReadOptionsLine(optfile, key_name, sizeof(key_name), key_value)) {
    Serial.printf("\t>>%s=%d\n", key_name, key_value);
    if (stricmp(key_name, "debug") == 0) {
      g_debug_output = key_value;
      Serial.printf("\tDebug set to: %d\n", g_debug_output);
    } else if (stricmp(key_name, "JPGScale") == 0) {
      g_JPGScale = key_value;
      Serial.printf("\tJPG Scale: %d\n", g_JPGScale);
    } else if (stricmp(key_name, "Center") == 0) {
      g_center_image = key_value;
      Serial.printf("\tCenter Image: %d\n", g_center_image);
    }
  }

  return true;
}

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

// Larger buffers are slightly more efficient, but if
// the buffer is too large, extra data is read unnecessarily.
// For example, if the image is 240 pixels wide, a 100
// pixel buffer will read 3 groups of 100 pixels.  The
// last 60 pixels from the 3rd read may not be used.

#define BUFFPIXEL 80
void bmpDraw(File &bmpFile, const char *filename, uint8_t x, uint16_t y, bool fErase) {

//  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint16_t buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  uint16_t awColors[320];  // hold colors for one row at a time...

  if((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        if (fErase && (x || y || (w != tft.width()) || (h != tft.height()))) {
          // Maybe update to only fill unused or maybe fill on each line
          tft.fillScreen(ILI9341_BLACK);
        }

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            awColors[col] = tft.color565(r,g,b);
          } // end pixel
          tft.writeRect(0, row, w, 1, awColors);
        } // end scanline
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
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
        if ((image_width > ((int)tft.width() * 8 )) || (image_height > ((int)tft.height() * 8 ))) {
          decode_options = JPEG_SCALE_EIGHTH;
          scale = 8;
        } else if ((image_width > ((int)tft.width() * 4 )) || (image_height > ((int)tft.height() * 4 ))) {
          decode_options = JPEG_SCALE_QUARTER;
          scale = 4;
        } else if ((image_width > ((int)tft.width() * 2 )) || (image_height > ((int)tft.height() * 2 ))) {
          decode_options = JPEG_SCALE_HALF;
          scale = 2;
        }        
      }
    }
    Serial.printf("Scale: 1/%d\n", scale);
    if (fErase && ((image_width/scale < tft.width()) || (image_height/scale < tft.height()))) {
      tft.fillScreen(ILI9341_BLACK);
    }

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
// Function to draw pixels to the display
#ifdef  _ILI9341_t3NH_
inline void writeClippedRect(int16_t x, int16_t y, int16_t cx, int16_t cy, uint16_t *pixels) {
  tft.writeRect(x, y, cx, cy, pixels);
}
#else
void writeClippedRect(int x, int y, int cx, int cy, uint16_t *pixels) 
{
  if ((x >= 0) && (y >= 0) && ((x + cx) <= tft.width()) && ((y + cy) <= tft.height())) {
    tft.writeRect(x, y, cx, cy, pixels);
    if (g_debug_output) Serial.printf("\t(%d, %d, %d, %d)\n", x, y, cx, cy);
  } else {
    int width = cx;
    if ((x + width) > tft.width()) width = tft.width() - x; 
    uint16_t *ppixLine = pixels;
    for (int yt = y; yt < (y + cy); yt++) {
      if (yt < 0) continue;
      if (yt >= tft.height()) break;
      if (x >=0) {
        tft.writeRect(x, yt, width, 1, ppixLine);
        if (g_debug_output)Serial.printf("\t(%d, %d, %d, %d)\n", x, y, width, 1);
      } else {
        tft.writeRect(0, yt, width, 1, ppixLine - x);
        if (g_debug_output)Serial.printf("\t(%d, %d, %d, %d ++)\n", 0, y, width, 1);
      }
      ppixLine += cx;
    }
  }  
} 
#endif

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
uint16_t *usPixels = nullptr;  //may have to incresse this based on the max x-valid of your image.
void processPNGFile(const char *name, bool fErase)
{
  int rc;  
  
  Serial.println();
  Serial.print(F("Loading PNG image '"));
  Serial.print(name);
  Serial.println('\'');
  rc = png.open((const char *)name, myOpen, myClose, myReadPNG, mySeekPNG, PNGDraw);
  if (rc == PNG_SUCCESS) {
    usPixels = (uint16_t*)malloc(png.getWidth() * 2);
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    if (fErase && ((png.getWidth() < tft.width()) || (png.getHeight() < tft.height()))) {
      tft.fillScreen(ILI9341_BLACK);
    }


    if (usPixels) {
      rc = png.decode(NULL, 0);
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
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  writeClippedRect(0,  pDraw->y + 24, pDraw->iWidth, 1, usPixels);
}

#endif

//=============================================================================
// Touch screen support 
//=============================================================================
void ProcessTouchScreen()
{
  // See if there's any  touch data for us
//  if (ts.bufferEmpty()) {
//    return;
//  }

  // You can also wait for a touch
  if (! ts.touched()) {
    fast_mode = false;
    return;
  }

  // first hack, if screen pressed go very fast
  fast_mode = true;

  // Retrieve a point
  TS_Point p = ts.getPoint();

  // p is in ILI9341_t3 setOrientation 1 settings. so we need to map x and y differently.

  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print("\tPressure = "); Serial.print(p.z);


  // Scale from ~0->4000 to tft.width using the calibration #'s
#if 1 // SCREEN_ORIENTATION_1
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());
#else
  
  uint16_t px = map(p.y, TS_MAXY, TS_MINY, 0, tft.width());
  p.y = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
  p.x = px;
#endif  
    Serial.print(" ("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");

}