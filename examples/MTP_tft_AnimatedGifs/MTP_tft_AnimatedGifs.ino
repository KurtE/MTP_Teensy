#include <AnimatedGIF.h>
#include <MTP_Teensy.h>

// If using the Sparkfun IO Carrier board uncomment this line and
// and using one of the ILI9341 libraries
//#define USE_SF_IOCarrier 1

// Demo sketch to play all GIF files in a directory
#define SD_CS_PIN BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS_PIN 10  // Works on SPI with this CS pin

// optional support for ILI9341_t3n - that adds additional features
#include <ILI9341_t3n.h>
// optional support for ILI9488_t3 - that adds additional features
//#include <ILI9488_t3.h>
//Optional support for ST7735/ST7789 graphic dislplays
//#include <ST7735_t3.h> // Hardware-specific library
//#include <ST7789_t3.h> // Hardware-specific library
//Optional support for RA8875
//#include <SPI.h>
//#include <RA8875.h>

// If ILI9341_t3n is not included include ILI9341_t3 which is installed by Teensyduino
#if !defined(_ILI9341_t3NH_) && !defined(_ILI9488_t3H_) && !defined(__ST7735_t3_H_)  && !defined(_RA8875MC_H_)
#include <ILI9341_t3.h>
#endif

#include <SPI.h>
#include <SD.h>

char szStart[] = "GIF";

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


#ifdef  _ILI9341_t3NH_
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

#elif defined(_ILI9488_t3H_)
ILI9488_t3 tft = ILI9488_t3(&SPI, TFT_CS, TFT_DC, TFT_RST);
#undef TOUCH_CS // may need additional support to work...
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

// For 1.54" TFT with ST7789
ST7789_t3 tft = ST7789_t3(TFT_CS,  TFT_DC, TFT_RST);

//#define SCREEN_WIDTH_TFTHEIGHT_144
// for 1.8" display and mini
//#define SCREEN_WIDTH_TFTHEIGHT_160 // for 1.8" and mini display
#elif defined(_RA8875MC_H_)
#undef TFT_RST
#define TFT_RST 9
#undef TOUCH_CS // may need additional support to work...
RA8875 tft = RA8875(TFT_CS, TFT_RST);

#else
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST);
#endif

#define BLUE  0x001F
#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0

AnimatedGIF gif;

File f, rootFile;
int x_offset, y_offset;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 2000



void setup() {
  // mandatory to begin the MTP session.
  MTP.begin();
  
  // Keep the SD card inactive while working the display.
  pinMode(SD_CS_PIN, INPUT_PULLUP);
  delay(20);

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
 
#else
  tft.begin();
  #if USE_SF_IOCarrier == 1
    tft.invertDisplay(true);
  #endif
  tft.setRotation(1);
#endif

  FillScreen(BLUE);

  Serial.begin(9600);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setRotation(1);
  
  tft.println(F("Waiting for Arduino Serial Monitor..."));
  while (!Serial) {
    if (millis() > 3000) break;
  }

  Serial.print(CrashReport);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);  
  
  Serial.print(F("Initializing SD card..."));
  tft.println(F("Init SD card..."));
  while (!SD.begin(SD_CS_PIN)) {
    Serial.println(F("failed to access SD card!"));
    tft.println(F("failed to access SD card!"));
    delay(2000);
  }
  MTP.addFilesystem(SD, "SD Card");
  
  rootFile = SD.open(szStart);
  //printDirectory(rootFile, 0);
  
  gif.begin(LITTLE_ENDIAN_PIXELS);
  Serial.println("OK!");
  emDisplayed = DISPLAY_IMAGES_TIME; 

#if defined( _ILI9341_t3NH_) || defined(_ILI9488_t3H_)
  tft.useFrameBuffer(true);
#endif
}

void ShowGIF(char *name)
{
  FillScreen(BLACK);
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    GIFINFO gi;

    x_offset = (tft.width() - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (tft.height() - gif.getCanvasHeight())/2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    if (gif.getInfo(&gi)) {
      Serial.printf("frame count: %d\n", gi.iFrameCount);
      Serial.printf("duration: %d ms\n", gi.iDuration);
      Serial.printf("max delay: %d ms\n", gi.iMaxDelay);
      Serial.printf("min delay: %d ms\n", gi.iMinDelay);
    }
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {
      #if defined( _ILI9341_t3NH_) || defined(_ILI9488_t3H_)
      tft.updateScreen();
      #endif
      MTP.loop();
    }
    gif.close();
  } else {
    Serial.printf("Error opening file = %d\n", gif.getLastError());
  }
} /* ShowGIF() */

void loop() {
  
  MTP.loop();
 
  if (emDisplayed < DISPLAY_IMAGES_TIME) return; 

  bool did_rewind = false;
  const char *name = nullptr;
  uint8_t name_len;
  bool gif_file = false;
  char szTmp[256];

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
    
    if((strcmp(&name[name_len-4], ".gif") == 0)) gif_file = true;
    if(stricmp(&name[name_len-4], ".gif") == 0) gif_file = true;
    if ( gif_file ) break;

  }
  FillScreen(BLACK);
  if (imageFile && gif_file) {
    strcpy(szTmp, szStart);
    strcat(szTmp, "/");
    strcat(szTmp, imageFile.name());
    Serial.println(szTmp);
    ShowGIF((char *)szTmp);
    imageFile.close();
  } else {
    FillScreen(GREEN);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.println(F("No Files Found"));
  }
  emDisplayed = 0;
} /* loop() */



#if defined(_RA8875MC_H_)
inline void FillScreen(uint16_t color) {tft.fillWindow(color);}

#elif defined(__ST7735_t3_H_) || defined(__ST7789_t3_H_)
inline void FillScreen(uint16_t color) {tft.fillScreen(color);}
#else
inline void FillScreen(uint16_t color) {tft.fillScreen(color);}
#endif

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > tft.width())
       iWidth = tft.height();
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    
    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          //spilcdSetPosition(&lcd, pDraw->iX+x+x_offset, y+y_offset, iCount, 1, DRAW_TO_LCD);
          //spilcdWriteDataBlock(&lcd, (uint8_t *)usTemp, iCount*2, DRAW_TO_LCD);
      
          //tft.setAddrWindow(pDraw->iX+x+x_offset, y+y_offset, iCount, 1);
          for(uint16_t j=0; j<iCount; j++)
            tft.drawPixel(pDraw->iX+x+x_offset+j, y+y_offset, usTemp[j]);
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<iWidth; x++)
        usTemp[x] = usPalette[*s++];
      //spilcdSetPosition(&lcd, pDraw->iX+x_offset, y+y_offset, iWidth, 1, DRAW_TO_LCD);
      //spilcdWriteDataBlock(&lcd, (uint8_t *)usTemp, iWidth*2, DRAW_TO_LCD);
      for (x=0; x<iWidth; x++)
        tft.drawPixel(pDraw->iX+x_offset, y+y_offset, usTemp[x]);

    
    }
} /* GIFDraw() */

void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  f = SD.open(fname, FILE_READ);
  if (f)
  {
    *pSize = f.size();
    Serial.printf("GIFOpenFile: %s, %d\n", fname, f.size());
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

void printDirectory(File dir, int numSpaces) {
   while(true) {
     File entry = dir.openNextFile();
     if (! entry) {
       //Serial.println("** no more files **");
       break;
     }
     printSpaces(numSpaces);
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numSpaces+2);
     } else {
       // files have sizes, directories do not
       unsigned int n = log10(entry.size());
       if (n > 10) n = 10;
       printSpaces(50 - numSpaces - strlen(entry.name()) - n);
       Serial.print("  ");
       Serial.print(entry.size(), DEC);
       DateTimeFields datetime;
       if (entry.getModifyTime(datetime)) {
         printSpaces(4);
         printTime(datetime);
       }
       Serial.println();
     }
     entry.close();
   }
}

void printSpaces(int num) {
  for (int i=0; i < num; i++) {
    Serial.print(" ");
  }
}

void printTime(const DateTimeFields tm) {
  const char *months[12] = {
    "January","February","March","April","May","June",
    "July","August","September","October","November","December"
  };
  if (tm.hour < 10) Serial.print('0');
  Serial.print(tm.hour);
  Serial.print(':');
  if (tm.min < 10) Serial.print('0');
  Serial.print(tm.min);
  Serial.print("  ");
  Serial.print(tm.mon < 12 ? months[tm.mon] : "???");
  Serial.print(" ");
  Serial.print(tm.mday);
  Serial.print(", ");
  Serial.print(tm.year + 1900);
}
