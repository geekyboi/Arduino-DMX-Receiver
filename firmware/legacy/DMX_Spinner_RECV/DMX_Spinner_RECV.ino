//Libraries
#include <DMXSerial.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SFE_MicroOLED.h>  // Include the SFE_MicroOLED library
#include <OneButton.h>
#include <math.h>


//Define all Arduino Pins
// Output Pins
const int lightPin = 9; //The pin used to control the light
//const int SDa = A4; // Pin connected to I2C
//const int SCl = A5; // Pin connected to I2C

//Input Pins
const int DMxEnable = 3; //DMX Receiver/Driver enable pin for the SN75176A
const int buttonUpPin = A0; //DMX Channel control button
const int buttonDownPin = A1; //DMX Channel control button
//Pin 0 (TX) and 1 (RX) must be used to send and receive DMX

//Declare Buttons
OneButton upButton(buttonUpPin, true);
OneButton downButton(buttonDownPin, true);
unsigned long scrollStart, scrollTime = 20;

//Setup Screen
MicroOLED oled(9, 0); //9 = reset pin, 0 = ??? pin - can't use a larger than 64x48 display as not enough dynamic memory
bool screenState = true;
bool sleepDisplay = false;
unsigned long flashTimer = 0, flashDelay = 600, sleepTimer = 0, sleepDelay = 20000; //Display flashes when connected to DMX

//SetUp DMX
long DMxChannel; //DMX Channel to read - cotrolled by buttons (Stored in EEPROM)
long DMxAddr = 0; //Address to store the DMX Channel in EEPROM
bool DMxConnected = false;

//Set up modes
int mode = 0, noModes = 2;


//Settings
#define noSettings 2
char* settingsName[noSettings] = {"Start1", "End1",};
long settingValues[noSettings];
long settingLimits[noSettings][3] = {{0  , 255, 255  },   //Start Light (Min, Max, Default)
                                     {0  , 255, 0    },   //End Light
                                                    };
int setting = 0; //Setting selected in the settings menu
int settingsPage = 0; //Used to Paginates the menu
int resetStage = 0; //Makes the user confirm they definetly want to reset the device to default values.
                            

void setup () {
  DMXSerial.init(DMXReceiver, DMxEnable);
  pinMode(lightPin, OUTPUT);
  upButton.attachClick(upClick);
  downButton.attachClick(downClick);
  upButton.attachDuringLongPress(upDuringLong);
  downButton.attachDuringLongPress(downDuringLong);
  downButton.attachDoubleClick(downDoubleClick);
  
  DMxChannel = EEPROMReadlong(DMxAddr);
  
  for (int i = 0; i < noSettings; i++) {
    settingValues[i] = EEPROMReadlong((i+1)*4);
    if (settingLimits[i][0] > settingValues[i] > settingLimits[i][1]) { //Reset the value to default if it's outside its limits
      settingValues[i] = settingLimits[i][2];
    }
  }
  
  oled.begin();     // Initialize the OLED
  oled.clear(ALL);  // Clear the library's display buffer
  oled.display(); // display the memory buffer drawn

  mode = 0;
}                      

void loop() {
  int brightness, brightnessTwo; //Fixture brightness (Read from DMX)
  if (millis() >= sleepTimer + sleepDelay) {
    sleepTimer = millis();
    sleepDisplay = true;
  }
  
  if (mode == 0) {
    //PROCESS DMX DATA
    // Calculate how long no data backet was received
    unsigned long lastPacket =  DMXSerial.noDataSince();
  
    if (lastPacket < 1000) { //If last DMX signal was reeived within 1 second
       brightness = DMXSerial.read(DMxChannel);
       flashDisplay();
       DMxConnected = true;
    } else { //DMX is disconnect do something.
        brightness = 0;
        screenState = true;
        DMxConnected = false;
    }
    
    //Update Display
    if (sleepDisplay) { //if display is awake
      oled.invert(false);
      oled.clear(PAGE);
      oled.display();
    } else {
      oled.invert(false);
      updateDMxDisplay();
    }
    
  } else if (mode == 1) {

    if (0 <= setting <= 1) { //if a brightness setting is selected make the light display it
      brightness = settingValues[setting];
    } else {
      brightness = 0;
    }
    
    //Update Display
    if (sleepDisplay) { //if display is awake
      oled.invert(false);
      oled.clear(PAGE);
      oled.display();
    } else {
      oled.invert(true);
      updateMenuDisplay();
    }
  } else {
    mode = 0;
  }
//Output Processed DMX Data
  if (mode == 1) {
    analogWrite(lightPin, constrain(brightness,0 , 255));
  } else {
    analogWrite(lightPin, map(brightness,0,255,settingValues[0],settingValues[1]));
  }
  ////////
//Update Buttons
  upButton.tick();
  downButton.tick();
}

void flashDisplay() {
  if (flashTimer + flashDelay < millis()) {
    flashTimer = millis(); //Reset Timer
    screenState = !screenState;
  }
}

void updateDMxDisplay() {  
  oled.clear(PAGE);
  oled.setCursor(0, 16);
  oled.write(31);
  oled.setCursor(59, 16);
  oled.write(30);
  oled.setFontType(2);
  oled.setCursor(22, 12);
  oled.print(DMxChannel);  
  oled.setFontType(0);
  oled.setCursor(9, 21);
  oled.print("CH");
  oled.setCursor(5, 30);
  oled.setFontType(0);
  if(screenState) {
    if (DMxConnected) {
      oled.print("Connected");
    } else {
      oled.print("...");
    }
  }

  oled.display();
    
}

void updateMenuDisplay() {
  oled.clear(PAGE);
  oled.setCursor(1,1);
  oled.setFontType(0);
  oled.print("Settings");
  oled.line(0,10,64,10);
  for (int i = 0; i < 4; i++) {
    oled.setCursor(1,13+i*9);
    if (i + settingsPage*4 == setting) { //Check if this is the currently selected setting and add a marker at the start
      oled.setCursor(0,13+i*9);
      oled.write(16);
      oled.setCursor(7,13+i*9);
    }
    if (i + settingsPage*4 > noSettings -1) { //if there are no more settings to print, print "rest all?"
      if (resetStage == 0) {
        oled.print("Reset?");
      } else if (resetStage == 1) {
        oled.print("R U Sure?");
      }
      break; //Exit loop if there are no more settings to print
    } else { 
      oled.print(settingsName[i + settingsPage*4]);
    }
    oled.setCursor(46,13+i*9);
    oled.print(settingValues[i + settingsPage*4]);
  }
  oled.display();
}

void upClick() {
  sleepTimer = millis();
  if (sleepDisplay) {
    sleepDisplay = false;
  } else {
    if (mode == 0) {
      DMxChannel++;
      if (DMxChannel > 512) DMxChannel = 1;
      EEPROMWritelong(DMxAddr, DMxChannel);
    } else if (mode == 1) {
      if (setting == noSettings) {
        //Reset has been selected (run for loop to set all settings)
        if (resetStage == 0) {
          resetStage = 1;
        } else if (resetStage == 1){
          for (int i = 0; i < noSettings; i++) {
            settingValues[i] = settingLimits[i][2];
            EEPROMWritelong((i+1)*4, settingValues[i]);
          }
          DMxChannel = 1;
          EEPROMWritelong(DMxAddr, DMxChannel);
          setting = 0;
          settingsPage = 0;
          resetStage = 0;
          mode = 0;
        }
      } else {
        settingValues[setting]++;
        if (settingValues[setting] > settingLimits[setting][1]) {
          settingValues[setting] = settingLimits[setting][0];
        }
        EEPROMWritelong((setting+1)*4, settingValues[setting]);
      }
    }
  }
}

void downClick() {
  sleepTimer = millis();
  if (sleepDisplay) {
    sleepDisplay = false;
  } else {
    if (mode == 0) {
      DMxChannel--;
      if (DMxChannel < 1) DMxChannel = 512;
      EEPROMWritelong(DMxAddr, DMxChannel);
    } else if (mode == 1) {
      resetStage = 0; //Reset the reset
      setting++;
      settingsPage = floor(setting/4);
      if(setting > noSettings) {
        setting = 0;
        settingsPage = 0;
      }
    }
  }
}

void downDuringLong() {
  sleepTimer = millis();
  if (sleepDisplay) {
    sleepDisplay = false;
  } else {
    if (mode == 0) {
      if (millis() >= scrollTime+scrollStart) {
        scrollStart = millis();
        DMxChannel--;
        if (DMxChannel < 1) DMxChannel = 512;
        EEPROMWritelong(DMxAddr, DMxChannel);
      }
    } else if (mode == 1) {
      if (millis() >= scrollTime+scrollStart) {
        scrollStart = millis();
        settingValues[setting]--;
        if (settingValues[setting] < settingLimits[setting][0]) {
          settingValues[setting] = settingLimits[setting][1];
        }
       EEPROMWritelong((setting+1)*4, settingValues[setting]);
      }
    }
  }
}

void upDuringLong() {
  sleepTimer = millis();
  if (sleepDisplay) {
    sleepDisplay = false;
  } else {
    if (mode == 0) {
      if (millis() >= scrollTime+scrollStart) {
        scrollStart = millis();
        DMxChannel++;
        if (DMxChannel > 512) DMxChannel = 1;
        EEPROMWritelong(DMxAddr, DMxChannel);
      }
    } else if (mode == 1) {
      if (millis() >= scrollTime+scrollStart) {
        scrollStart = millis();
        settingValues[setting]++;
        if (settingValues[setting] > settingLimits[setting][1]) {
          settingValues[setting] = settingLimits[setting][0];
        }
       EEPROMWritelong((setting+1)*4, settingValues[setting]);
      }
    }
  }
}

void downDoubleClick() {
  sleepTimer = millis();
  if (sleepDisplay) {
    sleepDisplay = false;
  } else {
    mode++;
    if (mode > noModes-1) {
      mode = 0;
    }
  }
}

void EEPROMWritelong(int address, long value) {
      //Decomposition from a long to 4 bytes by using bitshift.
      //One = Most significant -> Four = Least significant byte
      byte four = (value & 0xFF);
      byte three = ((value >> 8) & 0xFF);
      byte two = ((value >> 16) & 0xFF);
      byte one = ((value >> 24) & 0xFF);

      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(address, four);
      EEPROM.write(address + 1, three);
      EEPROM.write(address + 2, two);
      EEPROM.write(address + 3, one);
      }

long EEPROMReadlong(long address)
      {
      //Read the 4 bytes from the eeprom memory.
      long four = EEPROM.read(address);
      long three = EEPROM.read(address + 1);
      long two = EEPROM.read(address + 2);
      long one = EEPROM.read(address + 3);

      //Return the recomposed long by using bitshift.
      return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

