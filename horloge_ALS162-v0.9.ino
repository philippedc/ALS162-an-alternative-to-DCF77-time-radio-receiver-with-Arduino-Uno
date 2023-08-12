/*
 * ALS162 - Allouis (France) time broadcasted radio station decoder
 
  _________________________________________________________________
  |                                                               |
  |       author : Philippe de Craene <dcphilippe@yahoo.fr        |
  |       Free of use - Any feedback is welcome                   |
  _________________________________________________________________

  for ALS162 detail information: https://en.wikipedia.org/wiki/ALS162_time_signal
  
 *
 *
 * The phase modulation pattern.
 * -----------------------------
 *
 * One signal element consists of the following : the phase of the carrier
 * is advanced linearly up to +1 radian in 0.025 second, then retarded
 * linearly up to -1 rad in 0.050 second, then advanced again to reach zero
 * after another 0.025 second. One signal element is always sent at each
 * second between 0 and 58. The epoch is when the down ramp crosses zero.
 * If a '1' bit is to be stransmitted, two signal elements are sent in
 * sequence. Since the phase is the integral of the frequency, to this
 * triangular phase modulation corresponds a square frequency modulation
 * with an amplitude of about + and - 6Hz.
 * 
 *                 binary '0'                      binary '1'
 * 
 * 
 *                 / \                       / \         / \
 *  phase     ___ /   \       ___       ___ /   \       /   \       ___
 *                      \   /                     \   /       \   /
 *                       \ /                       \ /         \ /
 *
 *   (0.025 s    |  |  |  |  |             |  |  |  |  |  |  |  |  |
 *    ticks)           |                         |
 *                   epoch                     epoch
 *
 *                __       __               __       _____       __
 *               |  |     |  |             |  |     |     |     |  |
 * frequency  ___|  |     |  |___       ___|  |     |     |     |  |___
 *                  |     |                   |     |     |     |
 *                  |_____|                   |_____|     |_____|
 *
 * Both the average phase and the average frequency deviation are thus
 * zero. More data is sent by phase modulation during the rest of each
 * second. But the second marker (and data bit) is always preceded by 0.1
 * second without modulation. There is no marker at the beginning of the
 * 59th second, nor any data sent during the entire duration of that
 * second.
 *

Materials:
 - 162kHz radio receiver
 - CD4046 PLL
 - ATmega328p with a 10368.000kHz Crystal Oscillator
 - LCD1602 with I2C 

Compiled with Arduino IDE v1.8.16

 *
 *                               ATmega328p
 * 
 *                    (RESET) PC6  1    28 PC5 (ADC5/SCL) = lcd1602 SCL
 *                      (RXD) PD0  2    27 PC4 (ADC4/SDA) = lcd1602 SDA
 *                      (TXD) PD1  3    26 PC3 (ADC3)
 *                     (INT0) PD2  4    25 PC2 (ADC2)
 *    freqOutPin = 11  (OC2B) PD3  5    24 PC1 (ADC1)
 *                   (XCK/T0) PD4  6    23 PC0 (ADC0)  A0 = dataInPin
 *                            VCC  7    22 AGND
 *                            GND  8    21 AREF 
 *     XTAL1    (XTAL1/TOSC1) PB6  9    20 AVCC
 *     XTAL2    (XTAL2/TOSC2) PB7 10    19 PB5         13 = blink led
 *                       (T1) PD5 11    18 PB4         12 = blink led
 *                     (AIN0) PD6 12    17 PB3 (OC2A) 
 *                     (AIN1) PD7 13    16 PB2         10 = activity led
 *  SecondSynchro = 8  (ICP1) PB0 14    15 PB1 (OC1A)   9 = MilliSynchro
 
 
Code versions
-------------

V0.9       11 Aug 2023     first full working version


 */


// parameters
#define VERSION         0.90     // code version
//#define DEBUG              1     // debug mode only

#define freqOut      162000L     // 162Khz clock generated for radio demodulation
#define freqCPU    10368000L     // note F_CPU still defined as 16000000 by Arduino IDE
#define offset             6     // serial data read offset: define sensibility

// pin definition
#define dataInPin          0     // analog read the instantaneous voltage of serial time data activity

// libraries call
#include <Wire.h>                // i2c communication
#include <LiquidCrystal_I2C.h>   // https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
LiquidCrystal_I2C lcd(0x27, 16, 2);

// variables
int dataIn = 512, memo_dataIn = 512;
unsigned int counter10ms = 0;         // 10ms counter during one minute
unsigned int counter100ms = 0;        // 100ms counter during one second
unsigned int noActivityCounter = 1;   // no activity 10ms counter to find the 59th second synchro
unsigned int secondsCounter = 0;      // simple counter for seconds
bool activity = true;                 // flag to detect the 59th second
bool memo_timeBit, timeBit = false;   // binary data
volatile bool synchro = false;        // 59th second detected
volatile bool bitHigh = LOW;          // flag HIGH data during the 2nd 100ms of each second    
volatile bool displayEnable = false;  // flag to enable update display
bool blinkLed = false;                // blinking led
bool runOnce = true;
byte minuteU = 0, minuteD = 0, hourU = 0, hourD = 0;
byte wday = 0, dayU = 0, dayD = 0, monthU = 0, monthD = 0, yearU = 0, yearD = 0;
String weekDay[] = { "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim" };
String months[] = { "Janv", "Fevr", "Mars", "Avri", "Mai ", "Juin",
                    "Juil", "Aout", "Sept", "Octo", "Nove", "Dece" };
bool holiday = false, summer = false;
bool parity1 = true, parity2 = true, parity3 = true;
byte bitHighCounter = 0;
bool testFullMinute = 0;
bool displayReady = false;

// special characters for LCD
uint8_t low[8]  = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1};   // single dot bottom right
uint8_t high[8]  = {0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};  // single dot top right


#ifdef DEBUG
 volatile unsigned int memo_counter10ms = 0;
 volatile unsigned int memo_secondsCounter = 0;
 volatile unsigned int minutesCounter = 0;
#endif

// port access definition
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


//
// setup
//____________________________________________________________________________________________

void setup() {

  DDRB = 0xFF;    // set all portB as output = PB0 to PB5, I/O 8 to 13, pins 14 to 19
  DDRD = 0xFF;    // set all portD as output = PD0 to PD5

// Set the timers 
//---------------------------------------------------------------------

  noInterrupts(); 

// set timer 1 to interrupt every 10ms (100Hz) used for ADC sampling rate
// Clear Timer on Compare Match (CTC) Mode
// precaler setting : freqCPU/8, N = 8
  TCCR1A = 0;
  TCCR1B = 0;
  TCCR1B = bit(WGM12) | bit(CS11);
  OCR1A  = ( freqCPU / 8L / 100L ) - 1;
  TIMSK1 = bit(OCIE1A);

// set timer 2 to get 162kHz clock on output 3
// from exemplers: http://www.gammon.com.au/timers 
  TCCR2A = 0;
  TCCR2B = 0;
  TCCR2A = bit(WGM20) | bit(WGM21) | bit(COM2B1);  // fast PWM, clear OC2A on compare
  TCCR2B = bit(WGM22) | bit(CS20);                 // fast PWM, no prescaler
  OCR2A = (freqCPU / freqOut) - 1;          // set the output signal frequency 
  OCR2B = ((OCR2A + 1) / 2) - 1;            // 50% duty cycle

  interrupts();

// change the ADC prescaler to 16 instead of 128 by default : 16x faster
// from examples: https://zestedesavoir.com/billets/2068/arduino-accelerer-analogread/
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);

// start the LCD display
  Wire.begin();
  Wire.setClock(400000);        // Change clock speed from 100k(default) to 400kHz
  lcd.begin();
  lcd.clear();
  lcd.createChar(0, low);       // special character declaration
  lcd.createChar(1, high);
  lcd.setCursor(0, 0);
  lcd.print("horloge ALS162");
  lcd.setCursor(0, 1);
  lcd.print("version v");
  lcd.print(VERSION);
  delay(1000);
  lcd.clear();
}

//
// loop
//____________________________________________________________________________________________

void loop() {

// once per second: display sequence
  if( displayEnable ) {
    if( runOnce ) {
      runOnce = false;    
      #ifdef DEBUG
       SecondSynchro(HIGH);              // make a debug signal each second I/O 8 pin 14
       lcd.setCursor(0, 0);
       lcd.print("                ");    // very much faster than lcd.clear()
       lcd.setCursor(0, 0);
       lcd.print(secondsCounter);
       lcd.setCursor(4, 0);
       lcd.print(bitHigh);
       lcd.setCursor(10, 0);
       lcd.print(memo_secondsCounter);
       lcd.setCursor(0, 1);
       lcd.print("                ");
       lcd.setCursor(0, 1);
       lcd.print(minutesCounter);
       lcd.setCursor(10, 1);
       lcd.print(memo_counter10ms);
      #else
       lcd.setCursor(6, 0);
       if( secondsCounter < 10 ) lcd.print("0");
       lcd.print(secondsCounter);
       lcd.setCursor(8, 0);
       if(bitHigh) lcd.write(1);
       else        lcd.write(0);
       switch( secondsCounter ) {
        case  0:
          if(displayReady) {
            lcd.setCursor(0, 0);
            if(parity2) { lcd.print(hourD); lcd.print(hourU); }
            else          lcd.print("--");
            lcd.print(":");
            if(parity1) { lcd.print(minuteD); lcd.print(minuteU); }
            else          lcd.print("--");
            lcd.print(":");
            lcd.setCursor(9, 0);
            if( summer )  lcd.print(" ete   ");
            else          lcd.print(" hiver ");
            lcd.setCursor(13, 0);
            if( holiday ) lcd.print("/fe");
            lcd.setCursor(0, 1);
            lcd.print("                ");
            lcd.setCursor(0, 1);
            if(parity3) {
              if( --wday >= 0 ) lcd.print(weekDay[wday]); lcd.print(" ");
              lcd.print((dayD *10) + dayU); lcd.print(" ");
              int monthN = (monthD *10) + monthU;
              if( --monthN >= 0 ) lcd.print(months[monthN]); lcd.print(" ");
              lcd.print((yearD *10) + yearU + 2000);
              if( dayD == 0 ) lcd.print(" ");  // to insure to cover the 16 characters of the line
            }
          }
          else {
            lcd.setCursor(0, 0);
            lcd.print("      00        ");
            lcd.print(secondsCounter);
            lcd.setCursor(0, 1);
            lcd.print("                ");
          }
          minuteU = 0; minuteD = 0; hourU = 0; hourD = 0;
          wday = 0; dayU = 0; dayD = 0; monthU = 0; monthD = 0; yearU = 0; yearD = 0;
          parity1 = true; parity2 = true; parity3 = true;
          bitHighCounter = 0;
          break;
        case  1:
          if( synchro ) testFullMinute = true;
          else          testFullMinute = false;
          break;
        case 14:
          holiday = bitHigh;
          break;
        case 17:
          summer = bitHigh;
          break;
        case 21:
        case 22:
        case 23:
        case 24:
          minuteU = minuteU | bitHigh << ( secondsCounter - 21 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 25:
        case 26:
        case 27:
          minuteD = minuteD | bitHigh << ( secondsCounter - 25 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 28:
          if((bitHighCounter %2) != bitHigh ) parity1 = false;
          bitHighCounter = 0;
          break;
        case 29:
        case 30:
        case 31:
        case 32:
          hourU = hourU | bitHigh << ( secondsCounter - 29 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 33:
        case 34:
          hourD = hourD | bitHigh << ( secondsCounter - 33 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 35:
          if((bitHighCounter %2) != bitHigh ) parity2 = false;
          bitHighCounter = 0;
          break;
        case 36:
        case 37:
        case 38:
        case 39:
          dayU = dayU | bitHigh << ( secondsCounter - 36 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 40:
        case 41:
          dayD = dayD | bitHigh << ( secondsCounter - 40 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 42:
        case 43:
        case 44:
          wday = wday | bitHigh << ( secondsCounter - 42 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 45:
        case 46:
        case 47:
        case 48:
          monthU = monthU | bitHigh << ( secondsCounter - 45 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 49:
          monthD = bitHigh;
          if( bitHigh ) bitHighCounter++;
          break;
        case 50:
        case 51:
        case 52:
        case 53:
          yearU = yearU | bitHigh << ( secondsCounter - 50 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 54:
        case 55:
        case 56:
        case 57:
          yearD = yearD | bitHigh << ( secondsCounter - 54 );
          if( bitHigh ) bitHighCounter++;
          break;
        case 58:
          if(( bitHighCounter %2) != bitHigh) parity3 = false;
          break;
        case 59:
          synchro = false;
          if( testFullMinute ) displayReady = true;
          else                 displayReady = false;
          break;
       }  // end of switch
      #endif
      
      secondsCounter++;
      blinkLed = !blinkLed;
      BlinkingLed( blinkLed, synchro );
    }    // end test runOnce
  }      // end test displayEnable

  else  {
    runOnce = true;
    #ifdef DEBUG
     SecondSynchro(LOW);
    #endif
  }
}        // end of loop

//============================================================================================
// list of functions
//============================================================================================

//
// Interrupt function run every 10ms
//____________________________________________________________________________________________

ISR(TIMER1_COMPA_vect) {

// set times counters
  counter10ms ++;                          // count from 0 to ~6040 during 1 minute
  counter100ms = (counter10ms / 10) % 10;  // count from 0 to 9 for each second

// check demodulated radio signal
  memo_timeBit = timeBit;
  memo_dataIn = dataIn;  
  dataIn = analogRead( dataInPin );
  if(dataIn < memo_dataIn - offset) {
    timeBit = HIGH;
    noActivityCounter = 0;
  }
  else {
    timeBit = LOW;
    noActivityCounter++;
  }

// translate signal to bits
  if( counter100ms == 0 ) {
    if( timeBit ) { 
      ActivityLed(HIGH); 
    }
    else ActivityLed(LOW);
  }
  else if( counter100ms == 1 ) {
    if( timeBit ) { 
      ActivityLed(HIGH); 
      if( !memo_timeBit ) bitHigh = HIGH;  // only here there is the interesting data
    }
    else ActivityLed(LOW);
  } 

// update display enable moment
  else if( counter100ms == 2 ) displayEnable = true;
  else {
    bitHigh = LOW;
    displayEnable = false;
  }

// detect a full second with no activity = 59th second to start teh synchro
  if( noActivityCounter > 99 ) activity = false;
  if( !activity && (noActivityCounter == 0)) {
    activity = true;
    synchro = true;   
    #ifdef DEBUG
     memo_counter10ms = counter10ms;
     memo_secondsCounter = secondsCounter;
     minutesCounter++;
    #endif
    counter10ms = 0;
    secondsCounter = 0;
  }    // end of test !activity

// make a debug signal each 10ms I/O 9 pin 15
  #ifdef DEBUG
   static bool half = false;
   half = !half;
   if( half ) MilliSynchro(HIGH);
   else       MilliSynchro(LOW);
  #endif
}

//
// ActivityLed() = function to drive activity led
//____________________________________________________________________________________________

void ActivityLed( bool s ) {
  if( s ) PORTB |= B000100;    // output 10 to HIGH
  else    PORTB &= B111011;    // output 10 to LOW
}

//
// SecondSynchro() = gives second syncho for debugging
//____________________________________________________________________________________________

void SecondSynchro( bool s ) {
  if( s ) PORTB |= B000001;    // output 8 to HIGH
  else    PORTB &= B111110;    // output 8 to LOW
}

//
// MilliSynchro() = gives synchro every 20ms
//____________________________________________________________________________________________

void MilliSynchro( bool s ) {
  if( s ) PORTB |= B000010;    // output 9 to HIGH
  else    PORTB &= B111101;    // output 9 to LOW
}

//
// BlinkingLed() = function to drive blinking led
//____________________________________________________________________________________________

void BlinkingLed( bool s, bool c ) {
  if( !s ) {
    PORTB &= B011111;      // output 13 to LOW
    PORTB &= B101111;      // output 12 to LOW
  }
  else {
    if( c ) {
      PORTB |= B100000;    // output 13 to HIGH
      PORTB &= B101111;    // output 12 to LOW
    }
    else {
      PORTB |= B010000;    // output 12 to HIGH
      PORTB &= B011111;    // output 13 to LOW
    }
  } 
}
