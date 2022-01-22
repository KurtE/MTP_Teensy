#include <ILI9341_t3n.h>
#include <AnimatedGIF.h>
#include <SPI.h>
#include <SD.h>

#include <MTP_Teensy.h>

char szStart[] = "GIF";

// Demo sketch to play all GIF files in a directory
#define SD_CS_PIN BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS_PIN 10  // Works on SPI with this CS pin

// Display settings for Adafruit PyPortal
#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST 8
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

AnimatedGIF gif;

File f, rootFile;
int x_offset, y_offset;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 2500

// Draw a line of image directly on the LCD
// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > DISPLAY_WIDTH)
       iWidth = DISPLAY_WIDTH;
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

void setup() {

  // Keep the SD card inactive while working the display.
  pinMode(SD_CS_PIN, INPUT_PULLUP);
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
  printDirectory(rootFile, 0);
  
  gif.begin(LITTLE_ENDIAN_PIXELS);
  Serial.println("OK!");
  emDisplayed = DISPLAY_IMAGES_TIME; 

}

void ShowGIF(char *name)
{
  tft.fillScreen(ILI9341_BLACK);
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    GIFINFO gi;

    x_offset = (DISPLAY_WIDTH - gif.getCanvasWidth())/2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (DISPLAY_HEIGHT - gif.getCanvasHeight())/2;
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
  tft.fillScreen(ILI9341_BLACK);
  if (imageFile && gif_file) {
    strcpy(szTmp, szStart);
    strcat(szTmp, "/");
    strcat(szTmp, imageFile.name());
    Serial.println(szTmp);
    ShowGIF((char *)szTmp);
    imageFile.close();
  } else {
    tft.fillScreen(ILI9341_GREEN);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.println(F("No Files Found"));
  }
  emDisplayed = 0;
} /* loop() */


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
