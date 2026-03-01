#include <MIDIUSB.h>
#include <MIDIUSB_Defs.h>
#include <SPI.h>

///////////////// CONSTANTS /////////////////

// Total rowInd Cols Count
const int MAX_ROWS = 8;
const int MAX_COLS = 8;

// rowInd input pins Array
const int ROWS_ARRAY[MAX_ROWS] = {2,3,4,5,6,7,8,9};

// 74HC595 pins
const int dataPin = 16;
const int latchPin = 10; // Don't use SPI pin 14 MISO - causing interfence
const int clockPin = 15;

const byte SIPObits[MAX_COLS] = {
  B11111110,
  B11111101,
  B11111011,
  B11110111,
  B11101111,
  B11011111,
  B10111111,
  B01111111
};

const int STARTING_MIDI_NOTE = 24; // C4 = 60

const int sensorMin = 595;
const int sensorMax = 700;

/////////////////////////////////////////////



///////////////// VARIABLES /////////////////

int currentOctave = 1; //default to 1 (no shift 61 Keys) (0,1 2)
int damperValue = 0; // For Pedal
int sostenutoValue = 0;
int substainValue = 0;

// Track Key states in matrix
bool keyState_isPressed[MAX_ROWS][MAX_COLS] = { false };

unsigned long startTime;
unsigned long loopDuration;

/////////////////////////////////////////////


///////////////// FUNCTIONS  /////////////////

void cycleColumns(int colInd) {
  digitalWrite(latchPin, LOW);
  // shiftOut(dataPin, clockPin, MSBFIRST, SIPObits[colInd]); // Select column (active LOW)
  delayMicrosendos 
  SPI.transfer(SIPObits[colInd]);
  digitalWrite(latchPin, HIGH);
}

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

// For Pedals
void sendControlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}


int getKeyNumber(int rowInd, int colInd) { // 0 to 63
  return rowInd * MAX_COLS + colInd;
}

int getMidiNoteCode(int keyNumber) { // 1 to 64
  return STARTING_MIDI_NOTE + (currentOctave*12) + keyNumber;
}

int shiftOctave () {
  currentOctave = (currentOctave + 1) % 3;
  return currentOctave;

}

void checkExtraButtons(int KeyNumber) {
  if (KeyNumber == 61) {
    shiftOctave();
    Serial.print("Key 61 - Octave Shifted to: ");
    Serial.println(currentOctave);


  } else if (KeyNumber == 62) {
    damperValue = (damperValue == 0) ? 127 : 0; // Toggle between 0 and 127
    Serial.println("Key 62 - Damper");
  } else if (KeyNumber == 63) {
    
  }

  sendControlChange(0, 64, damperValue);
  sendControlChange(0, 66, sostenutoValue);
  sendControlChange(0, 67, substainValue);
  MidiUSB.flush();

}

void sendPedalUpdates() {
  int pedalA = analogRead(A0);
  int pedalB = analogRead(A1);
  int pedalD = analogRead(A2);
             // Read analog input (0–1023)

  pedalA = constrain(pedalA, sensorMin, sensorMax);

  // Map the clamped value to MIDI range (1–127)
  int midiValue = map(pedalA, sensorMin, sensorMax, 1, 127);

  // Output the result
  Serial.print("Sensor: ");
  Serial.print(pedalA);
  Serial.print(" → MIDI: ");
  Serial.println(midiValue);
  sendControlChange(0, 67, midiValue);
  MidiUSB.flush();

}


/////////////////////////////////////////////

void setup() {
  Serial.begin(9600);

  // Set up rowInd input pins
  for (int i = 0; i < MAX_ROWS; i++) {
    pinMode(ROWS_ARRAY[i], INPUT);
  }
  
  // SPI and Latch PIN
  SPI.begin();
  pinMode(latchPin, OUTPUT);

  // Set up A0 as input (external pull-up assumed)
  //pinMode(A0, INPUT);


  // Speed up ADC clock
  // ADCSRA &= ~((1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0));
  // ADCSRA |= (1 << ADPS2) | (1 << ADPS1); // prescaler = 64

  // // Use AVcc reference
  // ADMUX = (1 << REFS0);

}

void loop () {
  startTime = micros();

  for (int colInd = 0; colInd < MAX_COLS; colInd++) {
    // Activate one column at a time
    cycleColumns(colInd); 

    for (int rowInd = 0; rowInd < MAX_ROWS; rowInd++) {
      // Check ROWS_ARRAY
      int keyNumber = getKeyNumber(rowInd, colInd);
      // Check button state (Released = HIGH | Pressed == LOW)
      bool isPressed = (digitalRead(ROWS_ARRAY[rowInd]) == LOW);


      //// Press Case ////
      if (isPressed && !keyState_isPressed[rowInd][colInd]) {
        keyState_isPressed[rowInd][colInd] = true;

        if (keyNumber > 60) continue;

        noteOn(0, getMidiNoteCode(keyNumber), 127); // Channel 0, Max Velocity
        MidiUSB.flush();

        Serial.print("Button Pressed: ");
        Serial.println(keyNumber);
        // Serial.print("MIDI CODE:");
        // Serial.println(getMidiNoteCode(keyNumber));


      //// Release Case ////
      } else if (!isPressed && keyState_isPressed[rowInd][colInd]) {
        keyState_isPressed[rowInd][colInd] = false;

        if (keyNumber > 60) {
          checkExtraButtons(keyNumber); 
          continue;
        }

        noteOff(0, getMidiNoteCode(keyNumber), 0); // Channel 0, Velocity 0
        MidiUSB.flush();

        Serial.print("Button Released: ");
        Serial.println(keyNumber);
        // Serial.print("MIDI CODE:");
        // Serial.println(getMidiNoteCode(keyNumber));
      }


    }

  }


  ////// Time Calculator Code //////

  // sendPedalUpdates(); 
  
  // loopDuration = micros() - startTime;  // Calculate elapsed time
  
  // Serial.print("Loop time (microseconds): ");
  // Serial.println(loopDuration);

  // delay(200);

  /////////////////////////////////

}


