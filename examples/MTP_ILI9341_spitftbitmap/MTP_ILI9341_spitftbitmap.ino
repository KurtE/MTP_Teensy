//=============================================================================
// Simple bitamp display program with MTP support added.
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

// warning, this sketch uses libraries that are not shipped as part of Teensyduino
// ILI9341_t3n - https://github.com/KurtE/ILI9341_t3n 
// JPGDEC - https://github.com/bitbank2/JPEGDEC
// PNGdec - https://github.com/bitbank2/PNGdec

#include <ILI9341_t3n.h>
#include <SPI.h>
#include <SD.h>
#include <MTP_Teensy.h>

//#if defined __has_include
//#if __has_include(<JPEGDEC.h>)
// __has_include does not work on Arduino unless something explictily had included it without...
#include <JPEGDEC.h>
//#endif
//#endif

#include <PNGdec.h>


#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST 8
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

#define SD_CS BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS 6  // Works on SPI with this CS pin

File rootFile;
File myfile;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 2500

void setup(void) {
  // mandatory to begin the MTP session.
  MTP.begin();

  // Keep the SD card inactive while working the display.
  pinMode(SD_CS, INPUT_PULLUP);
  delay(200);

  tft.begin();
  tft.fillScreen(ILI9341_BLUE);

  Serial.begin(9600);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setRotation(1);
  tft.println(F("Waiting for Arduino Serial Monitor..."));
  while (!Serial) {
    if (millis() > 3000) break;
  }

  Serial.print(F("Initializing SD card..."));
  tft.println(F("Init SD card..."));
  while (!SD.begin(SD_CS)) {
    Serial.println(F("failed to access SD card!"));
    tft.println(F("failed to access SD card!"));
    delay(2000);
  }
  MTP.addFilesystem(SD, "SD Card");

  rootFile = SD.open("/");

  Serial.println("OK!");
  emDisplayed = DISPLAY_IMAGES_TIME; 
}

void loop() {
  MTP.loop();
  if (emDisplayed < DISPLAY_IMAGES_TIME) return; 
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
    if ( bmp_file || jpg_file || png_file) break;
  }
  tft.fillScreen(ILI9341_BLACK);
  if (imageFile && bmp_file) {
    bmpDraw(imageFile, imageFile.name(), 0, 0);

  #ifdef  __JPEGDEC__
  } else if(imageFile && jpg_file) {
    processJPGFile(name);
    imageFile.close();
  #endif  

  #ifdef __PNGDEC__
  } else if(imageFile && png_file) {
    processPNGFile(name);
    imageFile.close();
  #endif
  } else {
    tft.fillScreen(ILI9341_GREEN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println(F("No Files Found"));
  }
  emDisplayed = 0;
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
void bmpDraw(File &bmpFile, const char *filename, uint8_t x, uint16_t y) {

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

void processJPGFile(const char *name)
{
  Serial.println();
  Serial.print(F("Loading JPG image '"));
  Serial.print(name);
  Serial.println('\'');
  if (jpeg.open(name, myOpen, myClose, myReadJPG, mySeekJPG, JPEGDraw)) {
    int image_width = jpeg.getWidth();
    int image_height = jpeg.getHeight();
    Serial.printf("Image size: %dx%d\n", image_width, image_height);
    int decode_options = 0;
    if ((image_width > ((int)tft.width() * 8 )) || (image_height > ((int)tft.height() * 8 ))) decode_options = JPEG_SCALE_EIGHTH;
    else if ((image_width > ((int)tft.width() * 4 )) || (image_height > ((int)tft.height() * 4 ))) decode_options = JPEG_SCALE_QUARTER;
    else if ((image_width > ((int)tft.width() * 2 )) || (image_height > ((int)tft.height() * 2 ))) decode_options = JPEG_SCALE_HALF;
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
int JPEGDraw(JPEGDRAW *pDraw) {
  //Serial.printf("jpeg draw: x,y=%d,%d, cx,cy = %d,%d\n",
     //pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  tft.writeRect(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, pDraw->pPixels);
  return 1;
}
#endif

//=============================================================================
// PNG support 
//=============================================================================
//used for png files primarily
#ifdef __PNGDEC__
PNG png;

void processPNGFile(const char *name)
{
  int rc;  
  
  Serial.println();
  Serial.print(F("Loading PNG image '"));
  Serial.print(name);
  Serial.println('\'');
  rc = png.open((const char *)name, myOpen, myClose, myReadPNG, mySeekPNG, PNGDraw);
  if (rc == PNG_SUCCESS) {
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    rc = png.decode(NULL, 0);
    png.close();
  } else {
    Serial.println("Was not a valid jpeg file");
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
uint16_t usPixels[640];  //may have to incresse this based on the max x-valid of your image.

  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  tft.writeRect(0, pDraw->y + 24, pDraw->iWidth, 1, usPixels);
}

#endif