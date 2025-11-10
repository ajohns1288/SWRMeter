/*
  Project: Digital SWR/Power Meter
  LUT.cpp: Lookup table processing, takes input and table and provides interpolated output

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


#include <Arduino.h>

int lookupTable(int tblPtr[], int lookup)
{

  unsigned int cnt;
  unsigned int xIndex; 
  int finalVal;           

  int yUpper;        
  int yLower;            
  unsigned int DeltaYInput;

  unsigned int delBPInput;  
  unsigned int delBP;       
  int xval;       

  //get the count from first entry in array
  cnt = pgm_read_word(&tblPtr[0]);

  //index to the last X entry
  xIndex = (cnt * 2) - 1;

  //If we are greater than the last X value, return the last Y value
  if (lookup >= pgm_read_word(&tblPtr[xIndex])) {
    finalVal = pgm_read_word(&tblPtr[xIndex+1]);
  }
  //If we are less than the first X value, return the first Y value
  else if (lookup <= pgm_read_word(&tblPtr[1])) {
    finalVal = pgm_read_word(&tblPtr[2]);
  }

  //We need to interpolate
  else {
    //move index to n-1 entry
    xIndex -= 2;

    //search through breakpoints to find where lookup's value is between
    for (; lookup <= pgm_read_word(&tblPtr[xIndex]); xIndex -= 2);
    xval = pgm_read_word(&tblPtr[xIndex]);
    
    //Grab the Y values on both sides
    yLower = pgm_read_word(&tblPtr[xIndex + 1]);
    yUpper = pgm_read_word(&tblPtr[xIndex + 3]);

    //If we are exactly on a breakpoint
    if (lookup == xval)
    {
      finalVal = yLower;
    } 
    //Otherwise determine what % we are between the breakpoints and take the corresponding % of the Y values
    else {
      // Determine the difference between the lookup value and lower side
      delBPInput = (lookup - xval);

      // Determine the difference between the X values on either side
      delBP = (pgm_read_word(&tblPtr[xIndex + 2]) - xval);

      if (yUpper > yLower) {
        DeltaYInput = (long)(((yUpper - yLower) * delBPInput)) / delBP;
        finalVal = yLower + DeltaYInput;
      } else {
        DeltaYInput = (long)(((yLower - yUpper) * delBPInput)) / delBP;
        finalVal = yLower - DeltaYInput;
      }
    }
  }

  return (finalVal);
}