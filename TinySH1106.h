/*
  Project: Digital SWR/Power Meter
  TinySH1106.h: Library header to interface with SH1106. Goal is to minimize RAM usage to be compatable with ATTiny

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

#pragma once

#include <Arduino.h>

//For ATMega328 compatability - use tiny wire for attiny but wire for others
#if (AVR_ARCH==2||AVR_ARCH==25)
#include <TinyWireM.h>
#define _SH1106_USE_ATTINY 1
#else
#include <Wire.h>
#define _SH1106_USE_ATTINY 0
#endif

//SH1106 Command definitions
#define SINGLE_COMMAND 0x80
#define MULTI_COMMAND 0x00
#define SINGLE_DATA 0xC0
#define MULTI_DATA 0x40

#define DISPLAY_OFF 0xAE
#define DISPLAY_ON 0xAF
#define DISPLAY_RESUME 0xA4
#define DISPLAY_ALLON 0xA5
#define DISPLAY_NORMAL 0xA6
#define DISPLAY_INVERT 0xA7
#define SET_CONTRAST 0x81

#define SEG_REMAP_NORM 0xA0
#define SEG_REMAP_REV 0xA1
#define SET_SCAN_DEC 0xC8
#define SET_SCAN_INC 0xC0
#define DISPLAY_OFFSET 0xD3
#define SET_MUX 0xA8
#define SET_CLK 0xD5

#define SET_START_LINE 0x40
#define SET_COL_HI 0x10
#define SET_COL_LO 0x00
#define SET_PAGE 0xB0

#define RMW 0xE0
#define RMW_END 0xEE
#define NOP 0xE3

//Panel values
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DEFAULT_OFFSET 4


class TinySH1106 {

private:
    // I2C
    uint8_t wireAddr;

    //Cursor positions
    char xPos;
    char yPos;
    uint8_t offset;

	// flip font  (not visible publicly)
	char ReverseByte (char x);
  
  void initDisplay ();
  void PlotChar (char c, int x, int y);
  void drawChar (unsigned char c, uint8_t x, uint8_t y);

 public:
   // For I2C display: create the display object 
   TinySH1106(uint8_t wireAddr);

   // Initialize the display
   void begin();

   // Cycle through the initialization
   void reset(void);

   void PlotPoint (int x, int y);

   // Turn the display on
   void displayOn(void);

   // Turn the display offs
   void displayOff(void);

   // Clear the display buffer
   void clear(void);

      // Set display contrast
   void setContrast(char contrast);

   void setOffset(uint8_t off);

   // Send a command to the display (low level function)
   void sendCommand(unsigned char com);

   void sendData(unsigned char com);

   char readData();

   void drawBitmap(const unsigned char s[], uint8_t x, uint8_t y);


   // Send all the init commands
   void sendInitCommands(void);

   void setCursor(uint8_t x, uint8_t y);
   void setPage(char p);
   void setCol(char c);
   void setMuxRatio(char m);
   
   // Write a string to display at position given by setCursor(x,y).
   void print (char s[]);

   };