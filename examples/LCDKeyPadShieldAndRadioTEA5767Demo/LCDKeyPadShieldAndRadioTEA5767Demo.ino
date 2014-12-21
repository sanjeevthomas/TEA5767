/*
   Copyright 2014 Marcos R. Oliveira

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <LiquidCrystal.h>
#include <TEA5767N.h>
#include <Wire.h>
#include <stdlib.h>
#include <EEPROM.h>

// Init radio object
TEA5767N radio = TEA5767N();
// Init LCD display object
LiquidCrystal lcd(8, 9, 4, 5, 6, 7); 

// LCD key that was pressed
int lcd_key = 0;

// Buttons codes
#define btnRIGHT    0
#define btnUP       1
#define btnDOWN     2
#define btnLEFT     3
#define btnSELECT   4
#define btnNONE     5

// Delay between clock transitions
#define DELAY_VOLUME_TRANSITION 50

// Definition of the 3-dimension array
#define MENU_DEPTH 3
#define MENU_LINES 8
#define MENU_TEXT  16

// Arduino pins
int backLightPin = 10;
int upDownPin = 11;
int incPin = 12;

// Predefined stations array
float defaultStations[16] = {88.1, 89.1, 89.7, 91.3, 92.5, 93.7, 94.7, 95.3, 96.1, 98.5, 100.1, 100.9, 101.7, 102.1, 102.9, 103.3};
// Initialized station arrays
float stations[16] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
// Keep the current station index
byte stationIndex = 0;

byte applicationState = 0;

byte selectedMenuItem = 0;
float selectedStation;
boolean isStandByOn = false;
boolean wasReleased = true;

char menu[MENU_DEPTH][MENU_LINES][MENU_TEXT] = {
                       {{" Mute"}, {" Search"}, {" Fine search"}, {" Register statn"}, {" Configuration"}, {" Stand by"}, {" Load deflt stn"}, {" Exit"}},
                       {{" Search level"}, {" Backlit inten."}, {" Exit"}},
                       {{" Low"}, {" Medium"}, {" High"}, {"Exit"}}
                      };

byte searchLevel;
byte backlightIntensity;

void loadDefaultStations() {
  for (int i=0 ; i < 16 ; i++) {
     //TODO Do this using pointers
     stations[i] = defaultStations[i];
  }
}

int read_LCD_buttons(){
  int adc_key_in = analogRead(0);
  if (adc_key_in > 1000) return btnNONE; 
  if (adc_key_in < 50) return btnRIGHT; 
  if (adc_key_in < 195) return btnUP; 
  if (adc_key_in < 380) return btnDOWN; 
  if (adc_key_in < 555) return btnLEFT; 
  if (adc_key_in < 790) return btnSELECT; 
  return btnNONE;
}

byte findSelectedStationIndex(float station) {
  for (int i = 0 ; i < 16 ; i++) {
    if (stations[i] == station) {
      return i;
    }
  }
  return 0;
}

void loadStation() {
  byte intStation, floatStation;
  float station;
  
  intStation = EEPROM.read(0);
  floatStation = EEPROM.read(1);
  
  if (intStation != 0xFF) {
    station = (intStation * 1.0) +  (floatStation * .1);
    radio.selectFrequency(station);
    printSelectedFrequency(radio.readFrequencyInMHz());
    stationIndex = findSelectedStationIndex(station);
  } else {
    radio.selectFrequency(stations[stationIndex]);
  }
}

void loadSearchLevel() {
  searchLevel = EEPROM.read(2);
  if (searchLevel == 0xFF) {
    searchLevel = MID_STOP_LEVEL;
  }
  switch(searchLevel) {
    case LOW_STOP_LEVEL: {
      radio.setSearchLowStopLevel();
      break;
    }
    case MID_STOP_LEVEL: {
      radio.setSearchMidStopLevel();
      break; 
    }
    case HIGH_STOP_LEVEL: {
      radio.setSearchHighStopLevel();
      break; 
    }
  }
}

void loadBacklightIntensity() {
  backlightIntensity = EEPROM.read(3);
  analogWrite(backLightPin, backlightIntensity);
}

void setupVolume() {
  // Lowest volume
  digitalWrite(upDownPin, LOW);
  for (int i = 0 ; i < 100 ; i++) {
    digitalWrite(incPin, LOW);
    delay(1);
    digitalWrite(incPin, HIGH);
    delay(1);
  }
  // Pleasant level
  digitalWrite(upDownPin, HIGH);
  for (int i = 0 ; i < 15 ; i++) {
    digitalWrite(incPin, LOW);
    delay(1);
    digitalWrite(incPin, HIGH);
    delay(1);
  }
}

void loadConfiguration() {
  radio.mute();
  loadDefaultStations();
  loadStation();
  loadSearchLevel();
  loadBacklightIntensity();
  setupVolume();
  radio.turnTheSoundBackOn();
}

void saveStation(float station) {
  float aux;
  byte byteValue;
  
  byteValue = (byte) floor(station);
  EEPROM.write(0, byteValue);
  aux = station - floor(station);
  aux *= 10.0;
  if (aux > 5.0) {
    byteValue = byte(aux);
    if ((((float)byteValue) - aux) > 0.1) {
      byteValue--;
    } else if ((aux - ((float)byteValue)) > 0.1) {
      byteValue++;
    }
  } else {
    byteValue = (int) ceil(aux);
  }
  EEPROM.write(1, byteValue);
}

void saveSearchLevel(byte searchLevel) {
  EEPROM.write(2, searchLevel);
}

void setup(){
  pinMode(backLightPin, OUTPUT);  
  pinMode(upDownPin, OUTPUT);
  pinMode(incPin, OUTPUT);
  
  digitalWrite(upDownPin, HIGH);
  digitalWrite(incPin, HIGH);
  analogWrite(backLightPin, 255);
  
  lcd.begin(16, 2); 
  lcd.setCursor(6,0);
  lcd.print("MHz");
  
  // Loads configuration
  loadConfiguration();
  
  //From application_note_tea5767-8.pdf, limit the amount of noise energy.
  radio.setSoftMuteOn();
  //From application_note_tea5767-8.pdf, cut high frequencies from the audio signal.
  radio.setHighCutControlOn();
}

void printSelectedFrequency(float frequency) {
  printSelectedFrequency(frequency, 0, 0);
}

void printSelectedFrequency(float frequency, byte col, byte row) {
  char str_freq[5];
  dtostrf(frequency, 1, 1, str_freq);
  
  lcd.setCursor(col, row);
  lcd.print("     ");
  lcd.setCursor(col, row);
  lcd.print(str_freq);  
}

void printMuteStatus() {
  lcd.setCursor(12, 0);
  if (radio.isMuted()) {
    lcd.print("M");
  } else {
    lcd.print(" ");
  }
}

void printStereoStatus() {
  lcd.setCursor(10, 0);
  if (radio.isStereo()) {
    lcd.print("S");
  } else {
    lcd.print("M");
  }
}

void markSelectedMenuItem(char *firstLine, char *secondLine) {
  lcd.setCursor(0,0);
  lcd.print(firstLine);
  lcd.setCursor(0,1);
  lcd.print(secondLine);
}

void updateLevelIndicator() {
  byte x, y, sl;
  char barGraph[17];
  
  lcd.setCursor(0,1);
  sl = radio.getSignalLevel();
  for (x=0 ; x<sl ; x++) {
    barGraph[x] = 255;
  }
  for (y=x ; y<16 ; y++) {
    barGraph[y] = 32;
  }
  barGraph[y] = '\0';
  lcd.print(barGraph);
}

void loop(){
  lcd_key = read_LCD_buttons();
  
  //Any button turns the radio on again
  if ((lcd_key != btnNONE) && isStandByOn && wasReleased) {
    wasReleased = false;
    applicationState = 0;
    isStandByOn = false;
    
    //Necessary to elliminate noise while turning the radio back on
    radio.mute();
    radio.setStandByOff();
    //Necessary to elliminate noise while turning the radio back on
    delay(150);
    radio.turnTheSoundBackOn();
    analogWrite(backLightPin, backlightIntensity);
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("      MHz");
    
    // Starts station
    printSelectedFrequency(radio.readFrequencyInMHz());
  }
  
  switch (lcd_key) {
    case btnRIGHT: {
      if (wasReleased) {
        wasReleased = false;
        switch (applicationState) {
          case 0: {
            Serial.println(stationIndex);
            Serial.println(stations[stationIndex]);
            Serial.println(stations[stationIndex+1]);
            if ((stationIndex < 15) && (stations[stationIndex+1] != 0.0)) {
              stationIndex++;
            } else {
              stationIndex = 0;
            }
            Serial.println(stationIndex);
            Serial.println(stations[stationIndex]);
            radio.mute();
            radio.selectFrequency(stations[stationIndex]);
            radio.turnTheSoundBackOn();
            printSelectedFrequency(radio.readFrequencyInMHz());
            saveStation(stations[stationIndex]);
            break;
          }
          case 5: {
            byte isBandLimitReached;
            if (selectedMenuItem == 1) {
              radio.setSearchUp();
              isBandLimitReached = radio.searchNextMuting();
              if (isBandLimitReached) {
                radio.setSearchDown();
                isBandLimitReached = radio.searchNextMuting();
              }
              printSelectedFrequency(radio.readFrequencyInMHz(), 2, 0);
            }
            break; 
          }
          // Increment fine search station
          case 6: {
            if (selectedStation < 108.0) {
              selectedStation += .1;
              radio.selectFrequency(selectedStation);
              printSelectedFrequency(selectedStation, 2, 0);
              saveStation(selectedStation);
            }
            break;
          }
          // Increment backlight intensity
          case 7: {
            if ((backlightIntensity + 10) <= 255) {
              backlightIntensity += 10;
              analogWrite(backLightPin, backlightIntensity);
              EEPROM.write(3, backlightIntensity);
            }
            break;
          }
        }
      }
      break;
    }
    case btnLEFT: {
      if (wasReleased) {
        wasReleased = false;
        switch (applicationState) {
          case 0: {
            Serial.println(stationIndex);
            if (stationIndex > 0) {
              stationIndex--;
            } else {
              stationIndex = 15;
              while((stations[stationIndex] == 0.0) && (stationIndex > 0)) {
                stationIndex--;
              }
            }
            Serial.println(stationIndex);
            radio.mute();
            radio.selectFrequency(stations[stationIndex]);
            radio.turnTheSoundBackOn();
            printSelectedFrequency(radio.readFrequencyInMHz());
            saveStation(stations[stationIndex]);
            break;
          }
          case 5: {
            byte isBandLimitReached;
            if (selectedMenuItem == 1) {
              radio.setSearchDown();
              isBandLimitReached = radio.searchNextMuting();
              if (isBandLimitReached) {
                radio.setSearchUp();
                isBandLimitReached = radio.searchNextMuting();
              }
              printSelectedFrequency(radio.readFrequencyInMHz(), 2, 0);
            }
            break; 
          }
          // Decrement fine search station
          case 6: {
            if (selectedStation > 88.0) {
              selectedStation -= .1;
              radio.selectFrequency(selectedStation);
              printSelectedFrequency(selectedStation, 2, 0);
              saveStation(selectedStation);
            }
            break;
          }
          // Decrement backlight intensity
          case 7: {
            if ((backlightIntensity - 10) >= 20) {
              backlightIntensity -= 10;
              analogWrite(backLightPin, backlightIntensity);
              EEPROM.write(3, backlightIntensity);
            }
            break;
          }
        }
      }
      break;
    } 
    case btnUP: {
      //For up and down volume it´s ok let it execute
      //continuously
      if (wasReleased || (applicationState == 0)) {
        wasReleased = false;
        switch (applicationState) {
          //Volume UP
          case 0: {
            digitalWrite(upDownPin, HIGH);
            digitalWrite(incPin, LOW);
            delay(DELAY_VOLUME_TRANSITION);
            digitalWrite(incPin, HIGH);
            delay(DELAY_VOLUME_TRANSITION);
            break;
          }
          case 1: {
            if (selectedMenuItem > 0) {
              selectedMenuItem--;
              
              if (selectedMenuItem != 3) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[0][selectedMenuItem]);
                lcd.setCursor(0,1);
                lcd.print(menu[0][selectedMenuItem+1]);
              }
            }
            markSelectedMenuItem(">", " ");
            break;
          }
          // Selecting on of the two menu items: "Search level", "Backlit inten." or "Exit"
          case 3: {
            if (selectedMenuItem > 0) {
              selectedMenuItem--;
              
              if (selectedMenuItem != 3) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[1][selectedMenuItem]);
                lcd.setCursor(0,1);
                lcd.print(menu[1][selectedMenuItem+1]);
              }
            }
            markSelectedMenuItem(">", " ");
            break;
          }
          case 4: {
            if (selectedMenuItem > 0) {
              selectedMenuItem--;
              
              if (selectedMenuItem != 2) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[2][selectedMenuItem]);
                lcd.setCursor(0,1);
                lcd.print(menu[2][selectedMenuItem+1]);
              }
            }
            markSelectedMenuItem(">", " ");
            break; 
          }
        }
      }
      break;
    }
    case btnDOWN: {
      //For up and down volume it´s ok let it execute
      //continuously
      if (wasReleased || (applicationState == 0)) {
        wasReleased = false;
        switch (applicationState) {
          //Volume DOWN
          case 0: {
            digitalWrite(upDownPin, LOW);
            digitalWrite(incPin, LOW);
            delay(DELAY_VOLUME_TRANSITION);
            digitalWrite(incPin, HIGH);
            delay(DELAY_VOLUME_TRANSITION);
            break;
          }
          case 1: {
            if (selectedMenuItem < (MENU_LINES-1)) {
              selectedMenuItem++;
              
              if (selectedMenuItem != 1) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[0][selectedMenuItem-1]);
                lcd.setCursor(0,1);
                lcd.print(menu[0][selectedMenuItem]);
              }
            }
            markSelectedMenuItem(" ", ">");
            break;
          }
          case 3:
            if (selectedMenuItem < 2) {
              selectedMenuItem++;
              
              if (selectedMenuItem != 1) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[1][selectedMenuItem-1]);
                lcd.setCursor(0,1);
                lcd.print(menu[1][selectedMenuItem]);
              }
            }
            markSelectedMenuItem(" ", ">");
            break;
          case 4: {
            if (selectedMenuItem < 2) {
              selectedMenuItem++;
              if (selectedMenuItem != 1) {
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[2][selectedMenuItem-1]);
                lcd.setCursor(0,1);
                lcd.print(menu[2][selectedMenuItem]);
              }
            }
            markSelectedMenuItem(" ", ">");
            break; 
          }
        }
      }
      break;
    }
    case btnSELECT: {
      if (wasReleased) {
        wasReleased = false;
        switch (applicationState) {
          // Show SELECT menu items
          // >Mute
          //  Search
          case 0: {
            applicationState = 1;
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print(menu[0][0]);
            lcd.setCursor(0,1);
            lcd.print(menu[0][1]);
            markSelectedMenuItem(">", " ");
            selectedMenuItem = 0;
            break;
          }
          //Execute first level menu selection
          case 1: {
            switch(selectedMenuItem) {
              //Mute function
              case 0: {
                applicationState = 0;
                if (!radio.isMuted()) {
                  radio.mute();
                } else {
                  radio.turnTheSoundBackOn();
                }
                
                lcd.setCursor(0,0);
                lcd.print("      MHz        ");
                lcd.setCursor(0,1);
                lcd.print("                 ");
                printMuteStatus();
                
                // Starts station
                printSelectedFrequency(radio.readFrequencyInMHz());
                break; 
              }
              //Search
              case 1: {
                applicationState = 5;
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("<");
                lcd.setCursor(15, 0);
                lcd.print(">");
                printSelectedFrequency(radio.readFrequencyInMHz(), 2, 0);
                break; 
              }
              //Fine search
              case 2: {
                applicationState = 6;
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("<<");
                lcd.setCursor(14, 0);
                lcd.print(">>");
                selectedStation = radio.readFrequencyInMHz();
                printSelectedFrequency(selectedStation, 2, 0);
                break; 
              }
              //Register station
              case 3: {
                byte isBandLimitReached = 0;
                byte progress = 0;
                applicationState = 0;
                lcd.clear();
                radio.mute();
                radio.selectFrequency(88.0);
                radio.setSearchUp();
                while(!isBandLimitReached && (progress < 16)) {
                  isBandLimitReached = radio.searchNext();
                  stations[progress] = radio.readFrequencyInMHz();
                  lcd.setCursor(progress, 1);
                  lcd.print((char)255);
                  progress++;
                }
                radio.turnTheSoundBackOn();
                
                lcd.setCursor(0,0);
                lcd.print("      MHz        ");
                lcd.setCursor(0,1);
                lcd.print("                 ");
                
                // Starts station
                stationIndex = 0;
                saveStation(stations[stationIndex]);
                printSelectedFrequency(stations[stationIndex]);
                
                break;
              }
              //Configuration
              case 4: {
                applicationState = 3;
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print(menu[1][0]);
                lcd.setCursor(0,1);
                lcd.print(menu[1][1]);
                markSelectedMenuItem(">", " ");
                selectedMenuItem = 0;
                break;
              }
              //Puts the radio in standy by mode and turns off
              //the display backlight
              case 5: {
                applicationState = 2;
                lcd.clear();
                
                radio.setStandByOn();
                analogWrite(backLightPin, 0);
                
                isStandByOn = true;
                break; 
              }
              //Load default station
              case 6: {
                applicationState = 0;
                
                lcd.setCursor(0,0);
                lcd.print("      MHz        ");
                lcd.setCursor(0,1);
                lcd.print("                 ");
                
                loadDefaultStations();
                stationIndex = 0;
                
                radio.mute();
                radio.selectFrequency(stations[stationIndex]);
                radio.turnTheSoundBackOn();
                
                printSelectedFrequency(radio.readFrequencyInMHz());
                saveStation(stations[stationIndex]);
                break;
              }
              //Exit
              case 7: {
                applicationState = 0;
                
                lcd.setCursor(0,0);
                lcd.print("      MHz        ");
                lcd.setCursor(0,1);
                lcd.print("                 ");
                
                // Starts station
                printSelectedFrequency(radio.readFrequencyInMHz());
                break;
              }
            }
            break;
          }
          // One of the 3 itens selected: "Search level", "Backlit intensity" or "Exit"
          case 3: {
            switch(selectedMenuItem) {
              // Level item selected
              case 0: {
                applicationState = 4;
                lcd.clear();
                if (searchLevel < 3) {
                  lcd.setCursor(0,0);
                  lcd.print(menu[2][0]);
                  lcd.setCursor(0,1);
                  lcd.print(menu[2][1]);
                } else {
                  lcd.setCursor(0,0);
                  lcd.print(menu[2][1]);
                  lcd.setCursor(0,1);
                  lcd.print(menu[2][2]);
                }
                if (searchLevel == 1) {
                  markSelectedMenuItem(">", " ");
                } else {
                  markSelectedMenuItem(" ", ">");
                }
                selectedMenuItem = searchLevel - 1;
                break;
              }
              // Backlight intensity selected
              case 1: {
                applicationState = 7;
            
                lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Backlight int.");
                break;
              }
              // Exit
              case 2: {
                applicationState = 0;
            
                lcd.setCursor(0,0);
                lcd.print("      MHz        ");
                lcd.setCursor(0,1);
                lcd.print("                 ");
                
                // Starts station
                printSelectedFrequency(radio.readFrequencyInMHz());
                break; 
              }
            }
            break; 
          }
          // One of the levels selected
          case 4: {
            applicationState = 0;
            switch(selectedMenuItem+1) {
              case LOW_STOP_LEVEL: {
                radio.setSearchLowStopLevel();
                saveSearchLevel(LOW_STOP_LEVEL);
                break;
              }
              case MID_STOP_LEVEL: {
                radio.setSearchMidStopLevel();
                saveSearchLevel(MID_STOP_LEVEL);
                break; 
              }
              case HIGH_STOP_LEVEL: {
                radio.setSearchHighStopLevel();
                saveSearchLevel(HIGH_STOP_LEVEL);
                break; 
              }
            }
            
            lcd.setCursor(0,0);
            lcd.print("      MHz        ");
            lcd.setCursor(0,1);
            lcd.print("                 ");
            
            // Starts station
            printSelectedFrequency(radio.readFrequencyInMHz());
            break; 
          }
          case 5: {
            applicationState = 0;
            
            lcd.setCursor(0,0);
            lcd.print("      MHz        ");
            lcd.setCursor(0,1);
            lcd.print("                 ");
            
            // Starts station
            printSelectedFrequency(radio.readFrequencyInMHz());
            break; 
          }
          // Exit 
          case 6:
          case 7: {
            applicationState = 0;
            
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("      MHz        ");
            lcd.setCursor(0,1);
            lcd.print("                 ");
            
            // Starts station
            printSelectedFrequency(radio.readFrequencyInMHz());
            break; 
          }
        }
      }
      break;
    }
    case btnNONE: {
      if (!wasReleased) {
        wasReleased = true;
      }
      switch (applicationState) {
        case 0: {
          if (!isStandByOn) {
            printStereoStatus();
          }
          updateLevelIndicator();
          break;
        }
        case 5: {
          updateLevelIndicator(); 
          break;
        }
      }
      break;
    }
  }
  delay(10);
}
