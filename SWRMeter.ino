/*
  Project: Digital SWR/Power Meter
  SWRMeter.ino: Code that reads voltages, calculates power/SWR based on those voltages, then displays on screen

  NOTE: This was designed to run on an ATTiny85 with the latest ATTiny core. It will not compile without the latest ATTinyCore.

    Copyright (C) 2024, Andrew Johnson AJohns1288@Gmail.com

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

//Include needed files
#include "TinySH1106.h" //Display library
#include "font.h" //Fonts for display library
#include "LUT.h"  //Lookup table function 

//Flag to display counts instead of power. Used for setting the calibration curve
#define _CALIBRATE_SHOW_COUNTS 0

//Hardware defines/////////////////////////
//Pins to use for measurement
#define FWD_PWR_PIN A2
#define REF_PWR_PIN A3

//Voltage limits
#define MIN_VAL 8  //Min clip on voltage, in ADC counts
///////////////////////////////////////////


//DSP defines//////////////////////////////
//Note: These DSP functions were put in as an experimental exercise. They can be adjusted as needed

#define _DSP_USE_MULTI_SAMPLE 1   //Enabled: ADC read takes 5 samples, excludes extremes, and averages for each reading. Disabled: One sample per reading
#define _DSP_USE_WINDOW_AVG 1     //Enabled: ADC read is averaged across previous 5 readings. Disabled: ADC read is not averaged (Averaging takes place after read)
#define _DSP_USE_EWMA_FILTER 1    //Enabled: ADC read is filtered. Disabled: ADC read is not filtered. (Filtering takes place after averaging)
#define _DSP_USE_ADC_NOISE_RED 1  //Enabled: ADC reads will use the noise reduction feature (sleep during conversion), Disabled: ADC reads will occur with all CPU running
#define _DSP_USE_DITHER 1         //Enabled: ADC is read twice in a row, one bit is added, then sum divded by 2. This helps with single bit noise, Disabled: ADC is read once.
#define _DSP_PEAK_HOLD_TIME 3000  //Amount of time, in ms, that the peak value will be held on the display
#define _DSP_PEAK_HOLD_DEC 50     //Rate that the display value will decay to the current value after the hold time expires
#define ADC_TIME 250              //Minimum sample rate, in us, that the ADCs are read. Note that this is the minimum, actual readings may take longer.

//Filter constants for EWMA filter in fixed 10e-1 point(8=0.800)
#define FWD_FILTER_CONST 3
#define REF_FILTER_CONST 3
////////////////////////////////////////////


//Display defines///////////////////////////
//Full scale power in watts
#define FWD_PWR_FS 300
#define REF_PWR_FS 60

//Screen Refresh Rate
#define UPDATE_TIME 250  //In milliseconds
////////////////////////////////////////////

#if _DSP_USE_ADC_NOISE_RED
#include <avr/sleep.h>  // Sleep Modes
EMPTY_INTERRUPT(ADC_vect);
#endif


//Calibration table for FWD Pwr (counts to Watts x10)
const unsigned int LUT_FWD[] PROGMEM= {10, \
                10, 0, \
                88, 5, \
                118, 10, \
                180, 30, \
                213, 40, \
                273, 60, \
                300, 70, \
                350, 90, \
                695, 200, \
                1000, 300
              };

//Calibration table for Ref Pwr (counts to Wattsx10)
const unsigned int LUT_REF[] PROGMEM= {10, \
                10, 00, \
                88, 10, \
                118, 20, \
                180, 60, \
                213, 80, \
                273, 120, \
                300, 140, \
                350, 180, \
                695, 400, \
                1000, 600
              };


//Display object
TinySH1106 display(0x3c);

//char constant buffers
const char text_FWD[] = { 'F', 'W', 'D', 0x20 };
const char text_REF[] = { 'R', 'E', 'F', 0x20 };
const char text_PWR[] = { 'P', 'W', 'R', ':', 0x20, 0 };
const char text_SWR[] = { 'S', 'W', 'R', ':', 0x20, 0 };

//Char buffers for display values
char buf_PWR_val[] = { 0x20, 0x20, 0x20, 0x20, 0 };
char buf_REF_val[] = { 0x20, 0x20, 0x20, 0x20, 0 };
char buf_SWR_val[] = { '1', '.', '0', '0', 0 };

//Instantaneous/filtered ADC reads (in counts)
int fwdPwrInst = 0;
int refPwrInst = 0;
int fwdPwrFilt = 0;
int refPwrFilt = 0;

//Filter buffer - Compile the buffers out to save memory if unused
#if _DSP_USE_MULTI_SAMPLE
int filtBuf[] = { 0, 0, 0, 0, 0 };
#endif

//Windowed Average buffer - Compile the buffers out to save memory if unused
#if _DSP_USE_WINDOW_AVG
int fwdAvgBuf[] = { MIN_VAL, MIN_VAL, MIN_VAL, MIN_VAL, MIN_VAL };
int refAvgBuf[] = { MIN_VAL, MIN_VAL, MIN_VAL, MIN_VAL, MIN_VAL };
uint8_t avgIndx = 0;
#endif

//Display values - integer representation
int swrVal = 100;    //SWR x 100
int fwdDispVal = 0;  //Watts x1
int refDispVal = 0;  //Watts x1

//Timers
long dispUpdateTmr = 0; //Display refresh rate
long ADCTmr = 0; //ADC Sample rate
long FWDPeakHold=0; //Peak hold for FWD power
long REFPeakHold=0; //Peak hold for reflected power.


void setup() {

  analogRead(FWD_PWR_PIN); //Initialize A2D

  //Start display and set up splash screen
  display.begin();
  display.clear();
  splash();
  delay(1000);
  display.clear();

  //Set up static parts of display
  display.setCursor(0, 0);
  display.print(text_FWD);
  display.setCursor(40, 0);
  display.print(text_PWR);
  display.setCursor(0, 2);
  display.print(text_REF);
  display.setCursor(40, 2);
  display.print(text_PWR);
  display.setCursor(16, 6);
  display.print(text_SWR);
}

void loop() {
  //Update display if it is time
  if (millis() - dispUpdateTmr > UPDATE_TIME) {
    dispUpdateTmr = millis(); //Reset timer

    swrVal = getSWR(fwdDispVal, refDispVal); //Ger SWR value from fwd and ref powers


    //If we are in calibration mode, set display value to ADC counts (only last 3 digits will show)
    #if _CALIBRATE_SHOW_COUNTS
    fwdDispVal = fwdPwrFilt;
    refDispVal = refPwrFilt;
    #else
    //If we are in normal mode, convert voltage to power
    voltToPwr();
    #endif

    //Update screen with updated values
    updateScreen();
  }


  //Sample ADC if it is time
  if(micros()-ADCTmr>ADC_TIME)
  {
    ADCTmr=micros();
    readPwrADC();
  }
  
}

void swr2Char(int i, char *buf) {
  
  //Get the character that corresponds with each place in the input
  char ones = i % 10 + 0x30;
  char tens = (i / 10) % 10 + 0x30;
  char hund = (i / 100) % 10 + 0x30;

  //set the text buffers based on how big the SWR is
  if (i < 100) {
    buf[0] = 'E';
    buf[1] = 'R';
    buf[2] = 'R';
    buf[3] = 0x20;
  } else if (i >= 600) {
    buf[0] = 'H';
    buf[1] = 'I';
    buf[2] = 0x20;
    buf[3] = 0x20;
  } else {
    buf[0] = hund;
    buf[1] = '.';
    buf[2] = tens;
    buf[3] = ones;
  }
}

void pwr2Char(int i, char *buf) {
  
  //Get the character that corresponds with each place in the input
  char ones = i % 10 + 0x30;
  char tens = (i / 10) % 10 + 0x30;
  char hund = (i / 100) % 10 + 0x30;


  //set the buffers based on how big the power is
  if (i < 10) {
    buf[0] = ones;
    buf[1] = 'W';
    buf[2] = 0x20;
    buf[3] = 0x20;
  } else if (i < 100) {
    buf[0] = tens;
    buf[1] = ones;
    buf[2] = 'W';
    buf[3] = 0x20;
  } else {
    buf[0] = hund;
    buf[1] = tens;
    buf[2] = ones;
    buf[3] = 'W';
  }
}

void splash() {
  display.drawBitmap(bitmap1, 40, 18);
}

void updateScreen() {
  //Convert values to corresponding characters
  pwr2Char(fwdDispVal, buf_PWR_val);
  pwr2Char(refDispVal, buf_REF_val);
  swr2Char(swrVal, buf_SWR_val);

  //Set print position and print the updated characters
  display.setCursor(81, 2);
  display.print(buf_REF_val);
  display.setCursor(81, 0);
  display.print(buf_PWR_val);
  display.setCursor(52, 6);
  display.print(buf_SWR_val);
}

int getADCReading(byte port) 
{

  //Wrapper function to get ADC readings
  //ATTinycore's newest version allows analogRead to use the onboard ADC noise reduction function
  //If dithering is turned on, function will take two readings, add a bit, then divide by two. This has the affect of helping with single bit noise

//Variable to store first read when using dithering  
int read1=0;

//If ATTIny, use the noise reduction read in newest ATTiny core.
#if (AVR_ARCH==2||AVR_ARCH==25)
  #if _DSP_USE_DITHER
  read1=_analogRead(port, _DSP_USE_ADC_NOISE_RED);
  return (read1+_analogRead(port, _DSP_USE_ADC_NOISE_RED))+1)/2;
  #else
  return _analogRead(port, _DSP_USE_ADC_NOISE_RED);
  #endif
#else
//For Normal arduino, ADC noise reduction not possible
  #if _DSP_USE_DITHER
  read1=analogRead(port);
  return (read1+analogRead(port)+1)/2;
  #else
  return analogRead(port);
  #endif
#endif
 //End _DSP_USE_ADC_NOISE_RED
}

void readPwrADC() {
//function to read in voltages and filter them
#if _DSP_USE_MULTI_SAMPLE

  int maxRead = 0;
  int minRead = 1024;

  for (int i = 0; i < 5; i++) {
    fwdPwrInst = getADCReading(FWD_PWR_PIN);
    filtBuf[i] = fwdPwrInst;

    if (fwdPwrInst > maxRead) {
      maxRead = fwdPwrInst;
    } else if (fwdPwrInst < minRead) {
      minRead = fwdPwrInst;
    }
  }
  fwdPwrInst = ((filtBuf[0] + filtBuf[1] + filtBuf[2] + filtBuf[3] + filtBuf[4]) - minRead - maxRead) / 3;

  //Reset min/max
  maxRead = 0;
  minRead = 1024;
  for (int i = 0; i < 5; i++) {
    refPwrInst = getADCReading(REF_PWR_PIN);
    filtBuf[i] = refPwrInst;

    if (refPwrInst > maxRead) {
      maxRead = refPwrInst;
    } else if (refPwrInst < minRead) {
      minRead = refPwrInst;
    }
  }
  refPwrInst = ((filtBuf[0] + filtBuf[1] + filtBuf[2] + filtBuf[3] + filtBuf[4]) - minRead - maxRead) / 3;


#else  //just take one sample
  fwdPwrInst = getADCReading(FWD_PWR_PIN);
  refPwrInst = getADCReading(REF_PWR_PIN);
#endif

#if _DSP_USE_WINDOW_AVG
//Window average the last 5 samples excluding outliers
  fwdAvgBuf[avgIndx] = fwdPwrInst;
  refAvgBuf[avgIndx] = refPwrInst;

  avgIndx > 4 ? avgIndx = 0 : avgIndx++;

  int minRef = refAvgBuf[0];
  int maxRef = refAvgBuf[0];
  int minFwd = fwdAvgBuf[0];
  int maxFwd = fwdAvgBuf[0];

  int refSum = refAvgBuf[0];
  int fwdSum = fwdAvgBuf[0];

  for (int i = 1; i < 5; i++) {
    maxRef = max(refAvgBuf[i], maxRef);
    minRef = min(refAvgBuf[i], minRef);

    maxFwd = max(fwdAvgBuf[i], maxFwd);
    minFwd = min(fwdAvgBuf[i], minFwd);

    fwdSum += fwdAvgBuf[i];
    refSum += refAvgBuf[i];
  }

  refPwrInst = (refSum - minRef - maxRef) / 3;
  fwdPwrInst = (fwdSum - minFwd - maxFwd) / 3;
#endif // End USE_WINDOW_AVG

#if _DSP_USE_EWMA_FILTER
  //EWMA filter using integer math (alpha x10)
  fwdPwrFilt = (FWD_FILTER_CONST * fwdPwrInst) + ((10 - FWD_FILTER_CONST) * fwdPwrFilt);
  refPwrFilt = (REF_FILTER_CONST * refPwrInst) + ((10 - REF_FILTER_CONST) * refPwrFilt);
  //divide by 10 because we scaled up by 10 for the lines above
  fwdPwrFilt /= 10;
  refPwrFilt /= 10;
#else  //No filtering, just use instantaneous value
  fwdPwrFilt = fwdPwrInst;
  refPwrFilt = refPwrInst;
#endif
}

int voltToPwr()
{
  //Variable to hold lookup table result.
  int tmp=0;
  
  //If filtered power voltage value is over the min voltage, look up voltage in table, otherwise set power to 0
  (refPwrFilt<MIN_VAL)?refDispVal = 0:tmp = lookupTable(LUT_REF,refPwrFilt);


  //Peak and hold function
  if(tmp>refDispVal)//If current reading greater than whats on display
  {
    refDispVal=tmp; //Set display value if current reading is higher
    REFPeakHold=millis(); //Restart timer
  }
  else if(millis()-REFPeakHold>_DSP_PEAK_HOLD_TIME) //If current reading less than display and hold time expires
  {
    refDispVal-=_DSP_PEAK_HOLD_DEC; //Decrement display value by calibratable amount
    refDispVal=max(refDispVal,tmp); //Set display value to max of decremented value or current reading.
  } //Otherwise do nothing (leave display value as is)

  //If filtered power voltage value is over the min voltage, look up voltage in table, otherwise set power to 0
  (fwdPwrFilt<MIN_VAL)?fwdDispVal = 0:tmp = lookupTable(LUT_FWD,fwdPwrFilt);


  //Peak and hold function for Fwd power
  if(tmp>fwdDispVal)
  {
    fwdDispVal=tmp;
    FWDPeakHold=millis();
  }
  else if(millis()-FWDPeakHold>_DSP_PEAK_HOLD_TIME)
  {
    fwdDispVal-=_DSP_PEAK_HOLD_DEC;
    fwdDispVal=max(fwdDispVal,tmp);
  }

}


int getSWR(int fwdPwr, int refPwr) {
  //calculate the SWR without using floats, returns SWR x 100.
  if (refPwr * 2 > fwdPwr) {
    return 600;  //just send 6 if SWR is too high;
  } else if (refPwr == 0) {
    return 100;  //send 1 if no reflected power
  }


  //Calculate reflection coeff - We rescale by 1024 and 100 to get more precision in the sqrt function, since we are using fixed point math
  //Note that 1024 and 100 rescales will change to 32 and 10 after the square root due to distributive property.
  unsigned int tmp = (refPwr * 1024) / fwdPwr;  //rescale by 1024 (32^2)
  tmp = 100 * tmp;                              //rescale by 100 (10^2)
  tmp = intSqrt(tmp);                           // this will give you closest integer sqrt of the rescaled value above
  tmp = tmp * 100;                              //rescale by another 100 (+10*32 from before sqrt)
  tmp = tmp / 32;                               //this gives reflection coef *1000

  //SWR = (1+gamma)/(1-gamma) (x1000 due to scale above)
  unsigned int swrNum = (10 * (1000 + tmp));  //1 +gamma term
  return swrNum / ((1000 - tmp) / 10);        //divde by 1-gamma term
}

unsigned int intSqrt(int s) {
  //quick and dirty sqrt function to avoid floats
  int guess = (s >> 7) + 6;  //pick initial guess based on s

  for (uint8_t i = 0; i < 4; i++) { //4 iterations of newtons method to get approximate square root.
    guess = (guess + (s / guess));  //newtons method
    guess >>= 1;                    //divide by 2
  }

  return guess;
}
