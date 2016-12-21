/*
 * The MIT License (MIT)

Copyright (c) 2016 Jochen Peters (JotPe, Krefeld)

Permission is hereby granted, free of charge, to any person obtaining 
a copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.
*/

#include <SPI.h>
#include <SFE_MicroOLED.h>
#include <avr/power.h>

// ---------------------------- Configure ! -------------
#define PIN_RESET  9
#define PIN_DC     8
#define PIN_CS    10
#define DC_JUMPER  0

#define BUTTON1     3
// #define LED_RED     6

#define MESSAGEPOS     30
#define MEMOSTR_LIMIT 730

#define CHAR_TIME_REQUEST '~'
#define CHAR_TIME_RESPONSE '#'
#define CHAR_INIT_SETUP '!'
#define CHAR_NOTIFY_HINT '%'
// ------------------------------------------------------


// display 64x48
MicroOLED oled(PIN_RESET, PIN_DC, PIN_CS);

char memoStr[MEMOSTR_LIMIT] = {'\0'};
int  memoStrPos   = MESSAGEPOS;
int  page         = 0;
byte COUNT        = 0;

byte hours = 0;
byte minutes = 0;
byte seconds = 0;
byte tick = 0;

// save power, because sin/cos is to "expensive"
const byte xHour[13] = {29,34,38,39,38,34,29,24,20,19,20,24,29};
const byte yHour[13] = {21,22,26,31,36,40,41,40,36,31,26,22,21};
const byte xMin[60]  = {29,30,32,33,34,35,37,38,39,40,40,41,41,42,42,42,42,42,41,41,40,40,39,38,37,35,34,33,32,30,29,28,26,25,24,22,21,20,19,18,18,17,17,16,16,16,16,16,17,17,18,18,19,20,21,23,24,25,26,28};
const byte yMin[60]  = {18,18,18,19,19,20,20,21,22,23,24,26,27,28,30,31,32,34,35,36,38,39,40,41,42,42,43,43,44,44,44,44,44,43,43,42,42,41,40,39,37,36,35,34,32,31,30,28,27,26,24,23,22,21,20,20,19,19,18,18};

PROGMEM const char comm[][35] = {
  "+++",
  "ATE=0",
  "AT+HWMODELED=BLEUART",
  "AT+GAPDEVNAME=UART Notify Watch",
  "AT+BLEPOWERLEVEL=4",
  "ATZ"
};

void setupBle() {
  for(byte i=0; i<6; ++i) {
    Serial.println(comm[i]); 
    delay(250);  
  }
}

/**
 * because there is a power regulator, it is hard to
 * messure the battery power level.
 */
byte readVcc() {
  int result;
  power_adc_enable();
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(10); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
  result = ADCL; 
  result |= ADCH<<8; 
  result = 1126400L / result;
  power_adc_disable();
  
  // evil hack, if you do not use the BAT power supply and
  // you didn not cahnge the return line to scale 4.2 Volts or
  // something like that ;-D
  if(result>3310) result=3310;
  
  return (result-2700)/26; // scale: 3310 -> 24, 2710 -> 0
}

void filler() {
  for (int i=0; i<MESSAGEPOS; ++i) {
    memoStr[i] = ' ';
  }
}

void setup() {
  pinMode(BUTTON1, INPUT);
  digitalWrite(BUTTON1, HIGH);
  Serial.begin(9600);

  //pinMode(LED_RED, OUTPUT);

  power_timer1_disable();
  power_timer2_disable();
  power_adc_disable();
  power_twi_disable();

  oled.begin();
  oled.clear(ALL); // Clear the display's internal memory logo
  oled.display();
  delay(500);
  filler();
}

/**
 * the hard way to handle with german(?) UTF-8 stuff
 */
char umlReplace(char inChar) {
  if (inChar == -97) {
    inChar = 224; // ß
  } else if (inChar == -92) {
    inChar = 132; // ä
  } else if (inChar == -74) {
    inChar = 148; // ö
  } else if (inChar == -68) {
    inChar = 129; // ü
  } else if (inChar == -124) {
    inChar = 142; // Ä
  } else if (inChar == -106) {
    inChar = 153; // Ö
  } else if (inChar == -100) {
    inChar = 154; // Ü
  } else if (inChar == -85) {
    inChar = 0xAE; // <<
  } else if (inChar == -69) {
    inChar = 0xAF; // >>
  }
  return inChar;  
}

void serialEvent() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();    
    if (inChar == -61) continue; // symbol before utf-8
    if (inChar == -62) continue; // other symbol before utf-8
    if (inChar == '\n') {
      oled.command(DISPLAYON);
      memoStr[memoStrPos] = '\0';
      page = 0;
      continue;
    }
    memoStr[memoStrPos] = umlReplace(inChar);
    memoStrPos++;
  }
}

/**
 * make hours, minutes, seconds from "ticks" and
 * add it to the time (received from App)
 */
void ticking() {
  tick++;
  if (tick > 9) {
    seconds += tick/10;
    tick = tick % 10;
    if (seconds > 59) {
      minutes += seconds / 60;
      seconds  = seconds % 60;
    }
    if (minutes > 59) {
      hours  += minutes / 60;
      minutes = minutes % 60;
    }
    if (hours > 23) {
      hours = hours % 24;
    }
  }
}

void wakeUpIcon() {
  oled.clear(PAGE);
  oled.circle(32, 23, 10);
  oled.pixel(31, 20);
  oled.pixel(35, 19);
  oled.line (29, 27, 35, 27);
  oled.display();
  delay(350);
  oled.rect(29, 19, 3, 3);
  oled.rect(35, 19, 3, 3);
  oled.pixel(35, 26);
  oled.display();  
  delay(350);
  tick += 7; 
}

void digitalClock() {
  oled.setCursor(8, 5);
  if (hours<10) oled.print("0");
  oled.print(hours);
  oled.print(":");
  if (minutes<10) oled.print("0");
  oled.print(minutes);
  oled.print(":");
  if (seconds<10) oled.print("0");
  oled.print(seconds);  
}

void analogClock() {
  oled.circle(29, 31, 13);
  byte hour = hours;
  if (hour>12) hour-=12;
  oled.line(29, 31, xMin[minutes], yMin[minutes]);
  oled.line(29, 31, xHour[hour], yHour[hour]);  
}

void batteryIcon() {
  byte vccVal = readVcc();
  oled.pixel(61, 23);
  oled.pixel(62, 23);
  oled.rect(60, 24, 4, 24);
  oled.rectFill(60, 48-vccVal, 4, vccVal);  
}

void loop() {
  delay(93); // is < 100 : makes the seconds a bit faster!

  if (digitalRead(BUTTON1) == LOW) {
    delay(300);  
    tick += 3;
    if (digitalRead(BUTTON1) == LOW) {
      
      // ok: You pressed the button1 more than 300ms
      
      oled.command(DISPLAYON);
      oled.clear(PAGE);
      digitalClock();
      analogClock();
      batteryIcon();
      oled.display();
            
      delay(1000);
      tick += 10;
      oled.command(DISPLAYOFF);
      
      if (digitalRead(BUTTON1) == LOW) {
        
        // ok: You pressed the button1 more than 300ms + 1000ms
        
        oled.command(DISPLAYON);
        wakeUpIcon();
        oled.command(DISPLAYOFF);
        // "remove" old chars from buffer
        // print ignores everyting behind \0
        memoStr[MESSAGEPOS] = '\0';
        memoStrPos = MESSAGEPOS;
        COUNT = 0;
        Serial.println( CHAR_TIME_REQUEST );
      }
    }
  }

  /**
   * React, if there is a new message
   */
  if (memoStrPos > MESSAGEPOS && page == 0) {

    /**
     * Check at the message beginning for some special chars
     */
    if (memoStr[MESSAGEPOS] == CHAR_TIME_RESPONSE) {
      
      // extract the time -------------------------
      
      memoStr[MESSAGEPOS] = ' ';
      char dummyChr[] = {
        memoStr[MESSAGEPOS+1],
        memoStr[MESSAGEPOS+2],
        memoStr[MESSAGEPOS+4],
        memoStr[MESSAGEPOS+5],
        memoStr[MESSAGEPOS+7],
        memoStr[MESSAGEPOS+8]
      };
      String dummy(dummyChr);
      hours = dummy.substring(0,2).toInt();
      minutes = dummy.substring(2,4).toInt();
      seconds = dummy.substring(4,6).toInt();

    } else if (memoStr[MESSAGEPOS] == CHAR_NOTIFY_HINT) {
      
      // there is a new message (or a message is deleted)
      
      COUNT = (unsigned char) memoStr[MESSAGEPOS+1];
      if (COUNT > 0) {
        // switch a LED on ------------------- start
        //digitalWrite(LED_RED, HIGH);
        //page = memoStrPos; // makes a clear and display off
        // switch a LED on ------------------- end

        // alternative to a LED --- works only < 10 ;-D
        memoStr[MESSAGEPOS+1] = ' ';
        memoStr[MESSAGEPOS] = '0' + (char) COUNT;
        
      } else {
        //digitalWrite(LED_RED, LOW);
        memoStr[MESSAGEPOS] = '\0';
      }   
      
    } else if (memoStr[MESSAGEPOS] == CHAR_INIT_SETUP) {
      
      // initialize the DIY Smartwatch + BLE module --------
      
      setup();
      setupBle();
    }
  }

  /**
   * Scrolling message through display
   */
  if (memoStrPos > MESSAGEPOS && page <= memoStrPos) {
    oled.clear(PAGE);
    oled.setCursor(0, 0);
    oled.print(&(memoStr[page]));
    oled.display();
  }

  /// Safe power and switch display off, if message is at the end
  if (page == memoStrPos) {
    oled.command(DISPLAYOFF);
    // "remove" old chars from buffer
    // print ignores everyting behind \0
    memoStr[MESSAGEPOS] = '\0';
    memoStrPos = MESSAGEPOS;
  }
  
  page++;
  ticking();
}

