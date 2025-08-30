/*
  Single Mode Traffic Light with Adjustable Timings
  - Red, Green, Yellow durations can be set (1–999 s)
  - Use SET to cycle editable phases (Red → Green → Yellow)
  - Use UP/DOWN to change the value (hold for faster change)
  - After Yellow is set, values are saved to EEPROM and program runs
  - On power-up, durations are loaded from EEPROM
  - Buttons are active-LOW using INPUT_PULLUP; wire each button to GND
*/

#include <EEPROM.h>

// ========= EEPROM =========
#define MARKER_ADDR   100
#define MARKER_VALUE  0xA5
#define RED_ADDR        0   // using int; on AVR it's 2 bytes (gapped to 4 for simplicity)
#define GREEN_ADDR      4
#define YELLOW_ADDR     8

// ========= Button Pins (active-low with pullups) =========
#define setPin   19
#define upPin    18
#define downPin  17

// ========= 7-Segment Display Pins =========
int a = 12, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7;
int d1 = 14, d2 = 16, d3 = 15;  // digits

// ========= LED Pins =========
int redLight    = 11;
int yellowLight = 10;
int greenLight  = 9;

// ========= Program State =========
enum State { RUN, SET_RED, SET_GREEN, SET_YELLOW };
State state = RUN;

int redSec = 2;
int greenSec = 2;
int yellowSec = 1;

bool upPrev=false, downPrev=false, setPrev=false;
unsigned long upHoldStart=0, upLastInc=0;
unsigned long downHoldStart=0, downLastInc=0;

// ========= Forward Declarations =========
void loadFromEEPROM();
void saveToEEPROM();
bool buttonPressed(int pin, bool &prev);
void updateValueWithHold(int &v, int delta, unsigned long &holdStart, unsigned long &lastInc);
void displayBlinkingNumber(int num);
void displayNumber(int num);
void displayRemaining(unsigned long tStart, int totalSec);
void pickDigit(int digit);
void displayDigit(int num);
void clearSeg();
void seg0();void seg1();void seg2();void seg3();void seg4();
void seg5();void seg6();void seg7();void seg8();void seg9();

void setup() {
  // Buttons: use internal pull-ups; wire to GND
  pinMode(setPin,   INPUT_PULLUP);
  pinMode(upPin,    INPUT_PULLUP);
  pinMode(downPin,  INPUT_PULLUP);

  pinMode(d1, OUTPUT); pinMode(d2, OUTPUT); pinMode(d3, OUTPUT);
  pinMode(a, OUTPUT); pinMode(b, OUTPUT); pinMode(c, OUTPUT);
  pinMode(d, OUTPUT); pinMode(e, OUTPUT); pinMode(f, OUTPUT); pinMode(g, OUTPUT);

  pinMode(redLight, OUTPUT);
  pinMode(yellowLight, OUTPUT);
  pinMode(greenLight, OUTPUT);

  clearSeg();

  // Load from EEPROM or initialize defaults
  if (EEPROM.read(MARKER_ADDR) == MARKER_VALUE) {
    loadFromEEPROM();
  } else {
    redSec = 2; greenSec = 2; yellowSec = 1;
    saveToEEPROM();
  }
}

void loop() {
  // Handle SET button (edge + debounce)
  if (buttonPressed(setPin, setPrev)) {
    if (state == RUN) {
      state = SET_RED;
      digitalWrite(redLight, HIGH);
      digitalWrite(greenLight, LOW);
      digitalWrite(yellowLight, LOW);
    } else if (state == SET_RED) {
      state = SET_GREEN;
      digitalWrite(redLight, LOW);
      digitalWrite(greenLight, HIGH);
      digitalWrite(yellowLight, LOW);
    } else if (state == SET_GREEN) {
      state = SET_YELLOW;
      digitalWrite(redLight, LOW);
      digitalWrite(greenLight, LOW);
      digitalWrite(yellowLight, HIGH);
    } else if (state == SET_YELLOW) {
      saveToEEPROM();
      state = RUN;
    }
  }

  // --- SET MODE ---
  if (state == SET_RED || state == SET_GREEN || state == SET_YELLOW) {
    if (digitalRead(upPin) == LOW)
      updateValueWithHold((state==SET_RED?redSec:(state==SET_GREEN?greenSec:yellowSec)), +1, upHoldStart, upLastInc);
    else { upHoldStart=0; upLastInc=0; }

    if (digitalRead(downPin) == LOW)
      updateValueWithHold((state==SET_RED?redSec:(state==SET_GREEN?greenSec:yellowSec)), -1, downHoldStart, downLastInc);
    else { downHoldStart=0; downLastInc=0; }

    if (redSec<1) redSec=1;     if (redSec>999) redSec=999;
    if (greenSec<1) greenSec=1; if (greenSec>999) greenSec=999;
    if (yellowSec<1) yellowSec=1; if (yellowSec>999) yellowSec=999;

    int showVal = (state==SET_RED?redSec:(state==SET_GREEN?greenSec:yellowSec));
    displayBlinkingNumber(showVal);
    delay(1);
    return;
  }

  // --- RUN MODE ---
  unsigned long tStart;

  // Red phase
  tStart = millis();
  while (millis() - tStart < (unsigned long)redSec*1000UL) {
    digitalWrite(redLight, HIGH);
    digitalWrite(greenLight, LOW);
    digitalWrite(yellowLight, LOW);
    displayRemaining(tStart, redSec);
    if (buttonPressed(setPin, setPrev)) { state = SET_RED; return; }
  }

  // Green phase
  tStart = millis();
  while (millis() - tStart < (unsigned long)greenSec*1000UL) {
    digitalWrite(redLight, LOW);
    digitalWrite(greenLight, HIGH);
    digitalWrite(yellowLight, LOW);
    displayRemaining(tStart, greenSec);
    if (buttonPressed(setPin, setPrev)) { state = SET_GREEN; return; }
  }

  // Yellow phase
  tStart = millis();
  while (millis() - tStart < (unsigned long)yellowSec*1000UL) {
    digitalWrite(redLight, LOW);
    digitalWrite(greenLight, LOW);
    digitalWrite(yellowLight, HIGH);
    displayRemaining(tStart, yellowSec);
    if (buttonPressed(setPin, setPrev)) { state = SET_YELLOW; return; }
  }
}

/* =================== EEPROM =================== */
void loadFromEEPROM() {
  EEPROM.get(RED_ADDR,   redSec);
  EEPROM.get(GREEN_ADDR, greenSec);
  EEPROM.get(YELLOW_ADDR,yellowSec);
  if (redSec<1) redSec=2;
  if (greenSec<1) greenSec=2;
  if (yellowSec<1) yellowSec=1;
}

void saveToEEPROM() {
  EEPROM.put(RED_ADDR,   redSec);
  EEPROM.put(GREEN_ADDR, greenSec);
  EEPROM.put(YELLOW_ADDR,yellowSec);
  EEPROM.write(MARKER_ADDR, MARKER_VALUE);
}

/* ========== Button & Value Update ========== */
bool buttonPressed(int pin, bool &prev) {
  bool pressed = (digitalRead(pin) == LOW); // active-low
  if (pressed && !prev) { prev = true; delay(150); return true; } // debounce
  if (!pressed) prev = false;
  return false;
}

// Auto-increment with hold
void updateValueWithHold(int &v, int delta, unsigned long &holdStart, unsigned long &lastInc) {
  unsigned long now = millis();
  if (holdStart == 0) {
    holdStart = now;
    v += delta;
    if (v < 1) v = 1; if (v > 999) v = 999;
    lastInc = now;
  } else {
    if (now - holdStart >= 1000) {  // after 1s, accelerate
      if (now - lastInc >= 100) {
        v += delta;
        if (v < 1) v = 1; if (v > 999) v = 999;
        lastInc = now;
      }
    } else {
      if (now - lastInc >= 300) {  // before 1s, slower
        v += delta;
        if (v < 1) v = 1; if (v > 999) v = 999;
        lastInc = now;
      }
    }
  }
}

/* ========== 7-Segment Display ========== */
void displayRemaining(unsigned long tStart, int totalSec){
  unsigned long elapsed = (millis() - tStart) / 1000UL;
  int remaining = totalSec - (int)elapsed;
  if (remaining < 0) remaining = 0;
  displayNumber(remaining);
}

void displayBlinkingNumber(int num) {
  static unsigned long lastBlink=0;
  static bool on=true;
  if (millis() - lastBlink >= 500) { lastBlink = millis(); on = !on; }
  if (!on) { clearSeg(); return; }
  displayNumber(num);
}

void displayNumber(int num) {
  int ones = num % 10;
  int tens = (num/10) % 10;
  int hund = (num/100) % 10;

  pickDigit(1);
  if (hund==0) clearSeg(); else displayDigit(hund);
  delay(2);

  pickDigit(2);
  if (hund==0 && tens==0) clearSeg(); else displayDigit(tens);
  delay(2);

  pickDigit(3);
  displayDigit(ones);
  delay(2);

  clearSeg();
}

void pickDigit(int digit) {
  digitalWrite(d1, LOW);
  digitalWrite(d2, LOW);
  digitalWrite(d3, LOW);
  if (digit==1) digitalWrite(d1, HIGH);
  if (digit==2) digitalWrite(d2, HIGH);
  if (digit==3) digitalWrite(d3, HIGH);
}

void clearSeg() {
  digitalWrite(a, HIGH); digitalWrite(b, HIGH); digitalWrite(c, HIGH);
  digitalWrite(d, HIGH); digitalWrite(e, HIGH); digitalWrite(f, HIGH); digitalWrite(g, HIGH);
}

void displayDigit(int num) {
  switch (num) {
    case 0: seg0(); break; case 1: seg1(); break; case 2: seg2(); break;
    case 3: seg3(); break; case 4: seg4(); break; case 5: seg5(); break;
    case 6: seg6(); break; case 7: seg7(); break; case 8: seg8(); break;
    case 9: seg9(); break; default: clearSeg(); break;
  }
}

// Segment patterns (LOW = ON, HIGH = OFF)
void seg0(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,LOW); digitalWrite(f,LOW); digitalWrite(g,HIGH); }
void seg1(){ digitalWrite(a,HIGH);digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,HIGH);digitalWrite(e,HIGH);digitalWrite(f,HIGH);digitalWrite(g,HIGH); }
void seg2(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,HIGH);digitalWrite(d,LOW); digitalWrite(e,LOW); digitalWrite(f,HIGH);digitalWrite(g,LOW); }
void seg3(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,HIGH);digitalWrite(f,HIGH);digitalWrite(g,LOW); }
void seg4(){ digitalWrite(a,HIGH);digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,HIGH);digitalWrite(e,HIGH);digitalWrite(f,LOW); digitalWrite(g,LOW); }
void seg5(){ digitalWrite(a,LOW); digitalWrite(b,HIGH);digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,HIGH);digitalWrite(f,LOW); digitalWrite(g,LOW); }
void seg6(){ digitalWrite(a,LOW); digitalWrite(b,HIGH);digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,LOW); digitalWrite(f,LOW); digitalWrite(g,LOW); }
void seg7(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,HIGH);digitalWrite(e,HIGH);digitalWrite(f,HIGH);digitalWrite(g,HIGH); }
void seg8(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,LOW); digitalWrite(f,LOW); digitalWrite(g,LOW); }
void seg9(){ digitalWrite(a,LOW); digitalWrite(b,LOW); digitalWrite(c,LOW); digitalWrite(d,LOW); digitalWrite(e,HIGH);digitalWrite(f,LOW); digitalWrite(g,LOW); }
