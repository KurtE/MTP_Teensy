/***************************************************
 * 
 * Animated GIF display program with MTP support added.
 * based on bitbang2 (Larry Bank) animatedGIF library
 * 
 ***************************************************/
 
// WARNING, this sketch uses libraries that are not installed as part of Teensyduino
// ILI9341_t3n - https://github.com/KurtE/ILI9341_t3n 
// AnimatedGIF - https://github.com/bitbank2/AnimatedGIF
 
#include <ILI9341_t3n.h>
#include <AnimatedGIF.h>
#include <Audio.h>
#include <SPI.h>
#include <SD.h>

#include <MTP_Teensy.h>

char szStart[] = "GIF";  // Assumes all animated gifs are in a GIF directory.  Change this line to suit your needs

#define SD_CS_PIN BUILTIN_SDCARD  // Works on T_3.6 and T_4.1 ...
//#define SD_CS_PIN 10  // Works on SPI with this CS pin

//****************************************************************************
// Settings and objects
//****************************************************************************
#define TFT_DC  9
#define TFT_CS 10
#define TFT_RST -1
ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

AnimatedGIF gif;

File f, rootFile;
int x_offset, y_offset;

elapsedMillis emDisplayed;
#define DISPLAY_IMAGES_TIME 25

AudioPlaySdWav           playWav1;
// Use one of these 3 output types: Digital I2S, Digital S/PDIF, or Analog DAC
AudioOutputI2S           audioOutput;
//AudioOutputSPDIF       audioOutput;
//AudioOutputAnalog      audioOutput;
//On Teensy LC, use this for the Teensy Audio Shield:
//AudioOutputI2Sslave    audioOutput;

AudioConnection          patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection          patchCord2(playWav1, 1, audioOutput, 1);
AudioControlSGTL5000     sgtl5000_1;

// Options file information
static const PROGMEM char options_file_name[] = "PictureViewOptions.ini";
bool g_debug_output = false;
int g_JPGScale = 0;
int g_center_image = 0;

void setup() {
  // mandatory to begin the MTP session.
  MTP.begin();

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
  delay(25);

  // Add Audio 
  AudioMemory(8);
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);

  gif.begin(LITTLE_ENDIAN_PIXELS);
  Serial.println("OK!");

  emDisplayed = DISPLAY_IMAGES_TIME; 
  
  File optionsFile = SD.open(options_file_name);
  if (optionsFile) {
    ProcessOptionsFile(optionsFile);
    optionsFile.close();
  }

}

//****************************************************************************
// loop
//****************************************************************************
void loop() {
  AudioNoInterrupts();  // When using Audio with SD devices audio interrupts need to be turned on then back on.
  MTP.loop();
  AudioInterrupts();
  if (emDisplayed < DISPLAY_IMAGES_TIME) return; 

  bool did_rewind = false;
  const char *name = nullptr;
  uint8_t name_len;
  bool gif_file = false;
  char szTmp[256];

  Serial.println("Loop looking for image file");
  
  File imageFile;
  playWav1.play("GIF/2001_.wav");
  while(playWav1.isPlaying()) {
    for (;;) {
      AudioNoInterrupts();
      imageFile = rootFile.openNextFile();
      AudioInterrupts();
      if (!imageFile) {
        if (did_rewind) break; // only go around once. 
        AudioNoInterrupts();
        rootFile.rewindDirectory();
        AudioInterrupts();
        AudioNoInterrupts();
        imageFile = rootFile.openNextFile();
        AudioInterrupts();
        did_rewind = true;
      }

      // maybe should check file name quick and dirty
      AudioNoInterrupts();
      name = imageFile.name();
      AudioInterrupts();
      name_len = strlen(name);
      Serial.println(name);
      if (!name) continue;
    
      if((strcmp(&name[name_len-4], ".gif") == 0)) gif_file = true;
      if(stricmp(&name[name_len-4], ".gif") == 0) gif_file = true;
      if ( gif_file ) break;
    }
    tft.fillScreen(ILI9341_BLACK);
    if (imageFile && gif_file) {
        strcpy(szTmp, szStart);
        strcat(szTmp, "/");
        strcat(szTmp, name);
        //Serial.println(szTmp);
        ShowGIF((char *)szTmp);
        AudioNoInterrupts();
        imageFile.close();
        AudioInterrupts();
    } else {
        tft.fillScreen(ILI9341_GREEN);
        tft.setTextColor(ILI9341_WHITE);
        tft.setTextSize(2);
        tft.println(F("No Files Found"));
    }
    if (Serial.available()) {
      int ch;
      while(Serial.read() != -1) ;
      Serial.printf("Paused: enter anything to continue");
      while((ch = Serial.read()) == -1) ;
      if (ch == 'd') g_debug_output = !g_debug_output;
      while(Serial.read() != -1) ;
    }
  }  
  emDisplayed = 0;

} /* loop() */


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
// AnimatedGIF support 
//=============================================================================
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
      AudioNoInterrupts();
      MTP.loop();
      AudioInterrupts();
    }
    gif.close();
  } else {
    Serial.printf("Error opening file = %d\n", gif.getLastError());
  }
} /* ShowGIF() */


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
  AudioNoInterrupts();
  f = SD.open(fname, FILE_READ);
  AudioInterrupts();
  if (f)
  {
    AudioNoInterrupts();
    *pSize = f.size();
     AudioInterrupts();
    Serial.printf("GIFOpenFile: %s\n", fname);
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    AudioNoInterrupts();
     f->close();
     AudioInterrupts();
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
    AudioNoInterrupts();
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    AudioInterrupts();
    AudioNoInterrupts();
    pFile->iPos = f->position();
    AudioInterrupts();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  AudioNoInterrupts();
  f->seek(iPosition);
  AudioNoInterrupts();
  AudioInterrupts();
  pFile->iPos = (int32_t)f->position();
  AudioInterrupts();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */
