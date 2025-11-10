/*
  Project: Digital SWR/Power Meter
  TinySH1106.cpp: Library to interface with SH1106. Goal is to minimize RAM usage to be compatable with ATTiny
  Andrew Johnson AJohns1288@Gmail.com

  Adapted from:
  Tiny Graphics Library v3 - see http://www.technoblogy.com/show?23OS
  David Johnson-Davies - www.technoblogy.com - 23rd September 2018

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "TinySH1106.h"
#include "font.h"

char TinySH1106::ReverseByte (char x) {
  x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
  x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
  x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
  return x;    
}

void TinySH1106::initDisplay () {
  sendCommand(DISPLAY_OFF); 
  sendCommand(SEG_REMAP_REV);  // Flip horizontal
  sendCommand(SET_SCAN_DEC);  // Flip horizontal
  sendCommand(DISPLAY_ON);  // Display on
  setMuxRatio(0x3F);
  setContrast(0xCF);
  setOffset(DEFAULT_OFFSET);
}

// Plot point x,y into buffer if in current slice
// void TinySH1106::PlotPoint (int x, int y) {
//   Wire.beginTransmission(wireAddr);
//   sendCommand(0x00 + ((x + 2) & 0x0F));        // Column low nibble
//   sendCommand(0x10 + ((x + 2)>>4));            // Column high nibble
//   sendCommand(0xB0 + (y >> 3));                // Page
//   sendCommand(0xE0);                           // Read modify write
//   Wire.write(SINGLE_DATA);
//   Wire.endTransmission();
//   Wire.requestFrom(wireAddr, 2);
//   Wire.read();                            // Dummy read
//   int j = Wire.read();
//   Wire.beginTransmission(wireAddr);
//   Wire.write(SINGLE_DATA);
//   Wire.write((1<<(y & 0x07)) | j);
//   sendCommand(0xEE);                           // Cancel read modify write
//   Wire.endTransmission();
// }

void TinySH1106::drawBitmap(const unsigned char s[], uint8_t x, uint8_t y)
{

  //Macros for register settings, this is done since pages and offsets are limited to 7, so we can combine to save memory
  #define SET_PAGE_OFFSET(o) drawBitmapReg|=(o & 0x07)
  #define GET_PAGE_OFFSET (drawBitmapReg & 0x07)
  #define SET_PAGE_START(o) drawBitmapReg|=(((y/8) & 0x07)<<4)
  #define GET_PAGE_START ((drawBitmapReg & 0x70)>>4)
  #define SET_PAGE_SPLIT drawBitmapReg|=(0x08)
  #define CLEAR_PAGE_SPLIT drawBitmapReg&=~(0x08)
  #define GET_PAGE_SPLIT (drawBitmapReg&0x08)

  uint8_t drawBitmapReg=0; //Register to store offset in page, start page, and if bitmap is split across n+1 pages. 
  //Format: xsss pooo | x: not used, sss: start page, p: split page, ooo: offset

  SET_PAGE_OFFSET(y);
  SET_PAGE_START(y);

  uint8_t bmpWidth=s[0]; //bitmap width
  uint8_t bmpHeight=s[1]; //bitmap height
  uint8_t bits=0; //var to store progmem reads

  uint8_t pagesNeeded=(s[1]-1)/8; //Number of pages needed to display bitmap, zero indexed 

  GET_PAGE_OFFSET==0?pagesNeeded++:pagesNeeded+=2; //change to one indexed, add another page due to offset if needed
  GET_PAGE_OFFSET==0?CLEAR_PAGE_SPLIT:SET_PAGE_SPLIT;

  for (uint8_t p = GET_PAGE_START; p < GET_PAGE_START+pagesNeeded; p++) { //Loop for each page

    setPage((p)); //Set page to active page
    setCol(x);     //Set column to active
    sendCommand(RMW);

    for (uint8_t col=0; col<s[0]; col++) {
      uint8_t j=readData();
      if(p == (GET_PAGE_START))//first page, only one bitmap row needed
      {
          bits=pgm_read_byte(&s[(col+2)]); //get bits
          sendData(bits<<GET_PAGE_OFFSET);//Left shift shifts down by H
      }
      else if(p < (GET_PAGE_START+pagesNeeded-1))//not first not last, will need two bitmap rows, offset and ORd
      {
        bits=(pgm_read_byte(&s[(col+2)+((p-GET_PAGE_START)-1)*(s[0])]))>>(8-GET_PAGE_OFFSET); //Get above row offset
        bits|=((pgm_read_byte(&s[(col+2)+((p-GET_PAGE_START))*(s[0])]))<<GET_PAGE_OFFSET)&0xFF; //get current row offset

        sendData(bits);

      }
      else //last page
      {
          if(GET_PAGE_SPLIT) //bitmap not aligned to pages, need to shift
          {
            bits=(pgm_read_byte(&s[(col+2)+((p-GET_PAGE_START)-1)*(s[0])]));
            sendData(bits>>(8-GET_PAGE_OFFSET));
          }
          else //bitmap aligned to page bounds, no need to shift
          {
            bits=(pgm_read_byte(&s[(col+2)+((p-GET_PAGE_START))*(s[0])]));
            sendData(bits);
          }
      }
    }
    sendCommand(RMW_END); 
  }

  #undef SET_PAGE_OFFSET
  #undef GET_PAGE_OFFSET
  #undef SET_PAGE_START
  #undef GET_PAGE_START
  #undef SET_PAGE_SPLIT
  #undef CLEAR_PAGE_SPLIT
  #undef GET_PAGE_SPLIT
}

void TinySH1106::drawChar(const unsigned char c, uint8_t x, uint8_t y)
{

  //Macros for register settings, this is done since pages and offsets are limited to 7, so we can combine to save memory
  #define SET_PAGE_OFFSET(o) drawBitmapReg|=(o & 0x07)
  #define GET_PAGE_OFFSET (drawBitmapReg & 0x07)
  #define SET_PAGE_START(o) drawBitmapReg|=(((y/8) & 0x07)<<4)
  #define GET_PAGE_START ((drawBitmapReg & 0x70)>>4)
  #define SET_PAGE_SPLIT drawBitmapReg|=(0x08)
  #define CLEAR_PAGE_SPLIT drawBitmapReg&=~(0x08)
  #define GET_PAGE_SPLIT (drawBitmapReg&0x08)

  uint8_t drawBitmapReg=0; //Register to store offset in page, start page, and if bitmap is split across n+1 pages. 
  //Format: xsss pooo | x: not used, sss: start page, p: split page, ooo: offset

  SET_PAGE_OFFSET(y);
  SET_PAGE_START(y);

  uint8_t bmpWidth=FONT_WIDTH; //bitmap width
  uint8_t bmpHeight=FONT_HEIGHT; //bitmap height
  uint8_t bits=0; //var to store progmem reads

  uint8_t pagesNeeded=(FONT_HEIGHT-1)/8; //Number of pages needed to display bitmap, zero indexed 

  GET_PAGE_OFFSET==0?pagesNeeded++:pagesNeeded+=2; //change to one indexed, add another page due to offset if needed
  GET_PAGE_OFFSET==0?CLEAR_PAGE_SPLIT:SET_PAGE_SPLIT;

  for (uint8_t p = GET_PAGE_START; p < GET_PAGE_START+pagesNeeded; p++) { //Loop for each page

    setPage(p); //Set page to active page
    setCol(x);     //Set column to active
    sendCommand(RMW);

    for (uint8_t col=0; col<FONT_WIDTH; col++) {
      uint8_t j=readData();
      if(p == (GET_PAGE_START))//first page, only one bitmap row needed
      {
          bits=pgm_read_byte(&CharMap[CHAR_INDEX(c)][col]); //get bits
          sendData(bits<<GET_PAGE_OFFSET);//Left shift shifts down by H
      }
      else if(p < (GET_PAGE_START+pagesNeeded-1))//not first not last, will need two bitmap rows, offset and ORd
      {
        bits=(pgm_read_byte(&CharMap[CHAR_INDEX(c)][(col+2)+((p-GET_PAGE_START)-1)*(FONT_WIDTH)]))>>(8-GET_PAGE_OFFSET); //Get above row offset
        bits|=((pgm_read_byte(&CharMap[CHAR_INDEX(c)][(col+2)+((p-GET_PAGE_START))*(FONT_WIDTH)]))<<GET_PAGE_OFFSET)&0xFF; //get current row offset

        sendData(bits);

      }
      else //last page
      {
          if(GET_PAGE_SPLIT) //bitmap not aligned to pages, need to shift
          {
            bits=(pgm_read_byte(&CharMap[CHAR_INDEX(c)][(col+2)+((p-GET_PAGE_START)-1)*(FONT_WIDTH)]));
            sendData(bits>>(8-GET_PAGE_OFFSET));
          }
          else //bitmap aligned to page bounds, no need to shift
          {
            bits=(pgm_read_byte(&CharMap[CHAR_INDEX(c)][(col+2)+((p-GET_PAGE_START))*(FONT_WIDTH)]));
            sendData(bits);
          }
      }
    }
    sendCommand(RMW_END); 
  }

  #undef SET_PAGE_OFFSET
  #undef GET_PAGE_OFFSET
  #undef SET_PAGE_START
  #undef GET_PAGE_START
  #undef SET_PAGE_SPLIT
  #undef CLEAR_PAGE_SPLIT
  #undef GET_PAGE_SPLIT
}

void TinySH1106::PlotChar (char c, int x, int y) {
  char h = y & 0x07;

  for (char p = 0; p < 2; p++) {
    setPage(p+y);          // Page
    setCol(x);      // Column


      sendCommand(RMW);

    for (uint8_t col=0; col<FONT_WIDTH; col++) {

      uint8_t j = readData();

      sendData(pgm_read_byte(&CharMap[CHAR_INDEX(c)][col+p*FONT_WIDTH]));

    }
    sendCommand(RMW_END);
  }
}

TinySH1106::TinySH1106(uint8_t addr) {
  wireAddr = addr;
}



void TinySH1106::setCursor(uint8_t x, uint8_t y)
{
  xPos=x;
  yPos=y;
}

void TinySH1106::setOffset(uint8_t off)
{
  offset=off;
}

void TinySH1106::displayOn(void) {
    sendCommand(DISPLAY_ON);        //display on
}

void TinySH1106::displayOff(void) {
  sendCommand(DISPLAY_OFF);          //display off
}

void TinySH1106::setPage(char p) {
  sendCommand(SET_PAGE|(p&0x07));          //display off
}


void TinySH1106::print(char c[])
{
  for(int i=0; i<132; i++){
    if (c[i] == 0) return;
    PlotChar(c[i], xPos, yPos);
    xPos = xPos + FONT_WIDTH+FONT_KERN;
  }
}


//TinyWireM for ATTiny
#if _SH1106_USE_ATTINY

void TinySH1106::begin() {
  TinyWireM.begin();
  initDisplay();
  clear();
}

void TinySH1106::setCol(char c) {
  c=c+offset;
  TinyWireM.beginTransmission(wireAddr);
  TinyWireM.write(MULTI_COMMAND);
  TinyWireM.write(SET_COL_LO|(c & 0x0F));
  TinyWireM.write(SET_COL_HI|(c >> 4));                         
  TinyWireM.endTransmission();
}

void TinySH1106::setMuxRatio(char m) {
  TinyWireM.beginTransmission(wireAddr);
  TinyWireM.write(MULTI_COMMAND);
  TinyWireM.write(SET_MUX);
  TinyWireM.write((m & 0x3F));                         
  TinyWireM.endTransmission();
}

void TinySH1106::setContrast(char contrast) {
   TinyWireM.beginTransmission(wireAddr);      //begin transmitting
   TinyWireM.write(SINGLE_COMMAND);                          //command mode
   TinyWireM.write(SET_CONTRAST);
   TinyWireM.write(contrast);
   TinyWireM.endTransmission();
}

void TinySH1106::sendCommand(unsigned char com) {
   TinyWireM.beginTransmission(wireAddr);      //begin transmitting
   TinyWireM.write(SINGLE_COMMAND);                          //command mode
   TinyWireM.write(com);
   TinyWireM.endTransmission();                    // stop transmitting
  
}

void TinySH1106::sendData(unsigned char com) {
   TinyWireM.beginTransmission(wireAddr);      //begin transmitting
   TinyWireM.write(SINGLE_DATA);                          //command mode
   TinyWireM.write(com);
   TinyWireM.endTransmission();                    // stop transmitting
  
}

char TinySH1106::readData() {
   TinyWireM.requestFrom(wireAddr, 2); //Get 2 bytes, first is dummy
   TinyWireM.read();                        // Dummy read
  return TinyWireM.read();
  
}

void TinySH1106::clear()
{
  for (int page = 0 ; page < 8; page++) {
    sendCommand(SET_PAGE | page);
    sendCommand(SET_COL_LO | 2);
		sendCommand(SET_COL_HI | 0);

    for (int q = 0 ; q < 16; q++) {
      TinyWireM.beginTransmission(wireAddr);
      TinyWireM.write(MULTI_DATA);
      for (int i = 0 ; i < 8; i++) TinyWireM.write(0);
      TinyWireM.endTransmission();
    }
  }
}
#else // WIRE function calls for non ATTiny

void TinySH1106::begin() {
  Wire.begin();
  initDisplay();
  clear();
}

void TinySH1106::setCol(char c) {
  c=c+offset;
  Wire.beginTransmission(wireAddr);
  Wire.write(MULTI_COMMAND);
  Wire.write(SET_COL_LO|(c & 0x0F));
  Wire.write(SET_COL_HI|(c >> 4));                         
  Wire.endTransmission();
}

void TinySH1106::setMuxRatio(char m) {
  Wire.beginTransmission(wireAddr);
  Wire.write(MULTI_COMMAND);
  Wire.write(SET_MUX);
  Wire.write((m & 0x3F));                         
  Wire.endTransmission();
}

void TinySH1106::setContrast(char contrast) {
   Wire.beginTransmission(wireAddr);      //begin transmitting
   Wire.write(SINGLE_COMMAND);                          //command mode
   Wire.write(SET_CONTRAST);
   Wire.write(contrast);
   Wire.endTransmission();
}

void TinySH1106::sendCommand(unsigned char com) {
   Wire.beginTransmission(wireAddr);      //begin transmitting
   Wire.write(SINGLE_COMMAND);                          //command mode
   Wire.write(com);
   Wire.endTransmission();                    // stop transmitting
  
}

void TinySH1106::sendData(unsigned char com) {
   Wire.beginTransmission(wireAddr);      //begin transmitting
   Wire.write(SINGLE_DATA);                          //command mode
   Wire.write(com);
   Wire.endTransmission();                    // stop transmitting
  
}

char TinySH1106::readData() {
   Wire.requestFrom(wireAddr, 2); //Get 2 bytes, first is dummy
   Wire.read();                        // Dummy read
  return Wire.read();
  
}

void TinySH1106::clear()
{
  for (int page = 0 ; page < 8; page++) {
    sendCommand(SET_PAGE | page);
    sendCommand(SET_COL_LO | 2);
		sendCommand(SET_COL_HI | 0);

    for (int q = 0 ; q < 16; q++) {
      Wire.beginTransmission(wireAddr);
      Wire.write(MULTI_DATA);
      for (int i = 0 ; i < 8; i++) Wire.write(0);
      Wire.endTransmission();
    }
  }
}
#endif //end (AVR_ARCH==2||AVR_ARCH==25)
