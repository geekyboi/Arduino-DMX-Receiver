/*
  Arduino Nano DMX Shield - 4 channel DMX receiver
  https://github.com/geekyboi/Arduino-DMX-Receiver

  Hardware: "Arduino Nano DMX Shield" PCB V4 (EasyEDA project "Arduino Nano DMX Spinner")
    - MAX481 RS-485 transceiver on D0 (RX) / D1 (TX), driver/receiver enable on D3
    - 4 PWM outputs through 2N3904 transistors: OUT1 D5, OUT2 D9, OUT3 D10, OUT4 D11
    - UP button A0, DOWN button A1 (to GND, internal pull-ups)
    - 0.96" 128x64 SSD1306 I2C OLED on A4 (SDA) / A5 (SCL), address 0x3C

  The receiver occupies 4 consecutive DMX channels starting at the address you set:
    OUT1 = address, OUT2 = address + 1, OUT3 = address + 2, OUT4 = address + 3

  Buttons
    Home screen   UP / DOWN click      address +1 / -1
                  UP / DOWN hold       address scrolls (speeds up the longer you hold)
                  DOWN double-click    open settings
    Settings      DOWN click           next setting
                  UP click             value +1 (or confirm reset)
                  UP / DOWN hold       value up / down
                  DOWN double-click    back to home screen
    Any press while the screen is asleep just wakes it.

  Libraries (Arduino Library Manager):
    DMXSerial (Matthias Hertel), OneButton (Matthias Hertel), SSD1306Ascii (Bill Greiman)

  NOTE: the MAX481 drives the Nano's RX pin, which blocks USB uploads.
        Lift the Nano out of the shield to upload, then plug it back in.

  MIT License - Oliver Sherratt
*/

#include <DMXSerial.h>
#include <EEPROM.h>
#include <OneButton.h>
#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <avr/wdt.h>

// ---------------------------------------------------------------- pins
const uint8_t PIN_DMX_ENABLE = 3;
const uint8_t PIN_BTN_UP     = A0;
const uint8_t PIN_BTN_DOWN   = A1;
const uint8_t NUM_OUTPUTS    = 4;
const uint8_t OUTPUT_PINS[NUM_OUTPUTS] = {5, 9, 10, 11};

const uint8_t OLED_ADDRESS = 0x3C;  // most 0.96" modules; some are 0x3D

// ---------------------------------------------------------------- timing
const unsigned long DMX_TIMEOUT_MS     = 1000;   // no packet for this long = signal lost
const unsigned long SCREEN_SLEEP_MS    = 30000;  // blank the OLED after this long idle
const unsigned long MENU_TIMEOUT_MS    = 30000;  // leave settings after this long idle
const unsigned long SAVE_DELAY_MS      = 2000;   // write EEPROM once buttons go quiet
const unsigned long BLINK_MS           = 600;
const unsigned long LEVEL_REFRESH_MS   = 200;

// ---------------------------------------------------------------- settings
const uint16_t DMX_MAX_ADDRESS = 512 - NUM_OUTPUTS + 1;  // 509: last address where all 4 fit
const uint16_t SETTINGS_MAGIC  = 0xD4C5;
const uint8_t  SETTINGS_VERSION = 3;

enum LossMode : uint8_t { LOSS_ZERO = 0, LOSS_HOLD = 1, LOSS_MODES };

struct Settings {
  uint16_t magic;
  uint8_t  version;
  uint16_t address;
  uint8_t  minLevel[NUM_OUTPUTS];  // output level at DMX 0
  uint8_t  maxLevel[NUM_OUTPUTS];  // output level at DMX 255 (min > max inverts)
  uint8_t  ledCurve[NUM_OUTPUTS];  // 0 = linear, 1 = LED curve (smoother dimming)
  uint8_t  lossMode;
  uint8_t  checksum;
};

Settings settings;

// Menu items: Min/Max for each output, Curve for each output, then "No signal", then "Reset".
const uint8_t MENU_LEVEL_ITEMS = NUM_OUTPUTS * 2;
const uint8_t MENU_CURVE_ITEMS = MENU_LEVEL_ITEMS + NUM_OUTPUTS;  // first item after the curves
const uint8_t MENU_LOSS_ITEM   = MENU_CURVE_ITEMS;
const uint8_t MENU_RESET_ITEM  = MENU_CURVE_ITEMS + 1;
const uint8_t MENU_ITEMS       = MENU_CURVE_ITEMS + 2;
const uint8_t MENU_ROWS        = 6;  // rows visible below the title

// ---------------------------------------------------------------- state
SSD1306AsciiWire oled;
OneButton upButton(PIN_BTN_UP, true, true);
OneButton downButton(PIN_BTN_DOWN, true, true);

enum Screen : uint8_t { SCREEN_HOME, SCREEN_MENU };
Screen screen = SCREEN_HOME;

uint8_t menuItem   = 0;
uint8_t menuTop    = 0;
bool    resetArmed = false;

bool screenAsleep  = false;
bool needsRedraw   = true;
bool settingsDirty = false;
bool dmxConnected  = false;
bool blinkOn       = true;

unsigned long lastInputMs     = 0;
unsigned long lastBlinkMs     = 0;
unsigned long lastLevelDrawMs = 0;
unsigned long holdStartMs     = 0;
unsigned long lastRepeatMs    = 0;

uint8_t outputLevel[NUM_OUTPUTS];  // what is on the pins right now

// ================================================================ settings
void saveSettings();

uint8_t settingsChecksum(const Settings &s) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&s);
  uint8_t sum = 0;
  for (size_t i = 0; i < offsetof(Settings, checksum); i++) sum += p[i];
  return sum ^ 0x5A;
}

void setDefaults() {
  settings.magic   = SETTINGS_MAGIC;
  settings.version = SETTINGS_VERSION;
  settings.address = 1;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    settings.minLevel[i] = 0;
    settings.maxLevel[i] = 255;
    settings.ledCurve[i] = 0;
  }
  settings.lossMode = LOSS_ZERO;
}

void loadSettings() {
  EEPROM.get(0, settings);
  bool valid = settings.magic == SETTINGS_MAGIC &&
               settings.version == SETTINGS_VERSION &&
               settings.checksum == settingsChecksum(settings) &&
               settings.address >= 1 && settings.address <= DMX_MAX_ADDRESS &&
               settings.lossMode < LOSS_MODES;
  if (!valid) {  // blank chip, old firmware layout or corrupted: start clean
    setDefaults();
    saveSettings();
  }
}

void saveSettings() {
  settings.checksum = settingsChecksum(settings);
  EEPROM.put(0, settings);  // put() uses update(), so unchanged bytes are not rewritten
  settingsDirty = false;
}

void markChanged() {
  settingsDirty = true;
  needsRedraw = true;
}

// ================================================================ outputs
uint8_t scaleLevel(uint8_t dmx, uint8_t lo, uint8_t hi) {
  // Linear map 0..255 -> lo..hi, works for lo > hi (inverted) too.
  // 32-bit maths: (hi - lo) * dmx can reach 65025, which overflows a 16-bit int on AVR.
  return (uint8_t)(lo + ((int32_t)hi - (int32_t)lo) * (int32_t)dmx / 255);
}

void writeOutputs(const uint8_t levels[NUM_OUTPUTS]) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    outputLevel[i] = levels[i];
    analogWrite(OUTPUT_PINS[i], levels[i]);
  }
}

void updateOutputs() {
  uint8_t levels[NUM_OUTPUTS];

  if (screen == SCREEN_MENU && menuItem < MENU_LEVEL_ITEMS) {
    // Preview the min/max value being edited on its own output; others off.
    uint8_t out = menuItem / 2;
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) levels[i] = 0;
    levels[out] = (menuItem % 2 == 0) ? settings.minLevel[out] : settings.maxLevel[out];
    writeOutputs(levels);
    return;
  }

  bool connected = DMXSerial.noDataSince() < DMX_TIMEOUT_MS;
  if (connected != dmxConnected) {
    dmxConnected = connected;
    needsRedraw = true;
  }

  // DMXSerial keeps the last frame it received, so "hold" just keeps reading it.
  // This also restores the held levels after leaving the menu's output preview.
  bool useDmx = connected || settings.lossMode == LOSS_HOLD;

  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (!useDmx) {
      levels[i] = 0;  // signal lost: output off, even if it is inverted
      continue;
    }
    uint8_t dmx = DMXSerial.read(settings.address + i);
    if (settings.ledCurve[i]) dmx = (uint16_t)dmx * dmx / 255;  // squared: finer steps at the low end
    levels[i] = scaleLevel(dmx, settings.minLevel[i], settings.maxLevel[i]);
  }
  writeOutputs(levels);
}

// ================================================================ display
void printPadded(uint16_t value, uint8_t width) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%0*u", width, value);
  oled.print(buf);
}

void drawLevels() {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    oled.setCursor((i % 2) * 64, 6 + i / 2);
    oled.print(F("O"));
    oled.print(i + 1);
    oled.print(F(" "));
    printPadded(outputLevel[i], 3);
  }
}

void drawHome() {
  oled.setInvertMode(false);
  oled.set1X();
  oled.setCursor(0, 0);
  oled.print(F("DMX ADDRESS"));
  oled.clearToEOL();

  oled.set2X();
  oled.setCursor(10, 2);
  printPadded(settings.address, 3);
  oled.print(F("-"));
  printPadded(settings.address + NUM_OUTPUTS - 1, 3);
  oled.clearToEOL();
  oled.set1X();

  oled.setCursor(0, 4);
  if (dmxConnected) {
    oled.print(blinkOn ? F("* SIGNAL OK") : F("  SIGNAL OK"));
  } else {
    oled.print(blinkOn ? F("NO SIGNAL") : F(""));
  }
  oled.clearToEOL();

  drawLevels();
}

void menuLabel(uint8_t item) {
  if (item < MENU_LEVEL_ITEMS) {
    oled.print(F("Out"));
    oled.print(item / 2 + 1);
    oled.print(item % 2 == 0 ? F(" min") : F(" max"));
  } else if (item < MENU_CURVE_ITEMS) {
    oled.print(F("Out"));
    oled.print(item - MENU_LEVEL_ITEMS + 1);
    oled.print(F(" curve"));
  } else if (item == MENU_LOSS_ITEM) {
    oled.print(F("No signal"));
  } else {
    oled.print(resetArmed ? F("Sure? UP=yes") : F("Factory reset"));
  }
}

void menuValue(uint8_t item) {
  oled.setCol(96);
  if (item < MENU_LEVEL_ITEMS) {
    uint8_t out = item / 2;
    printPadded(item % 2 == 0 ? settings.minLevel[out] : settings.maxLevel[out], 3);
  } else if (item < MENU_CURVE_ITEMS) {
    oled.print(settings.ledCurve[item - MENU_LEVEL_ITEMS] ? F("LED") : F("lin"));
  } else if (item == MENU_LOSS_ITEM) {
    oled.print(settings.lossMode == LOSS_HOLD ? F("hold") : F("zero"));
  }
}

void drawMenu() {
  oled.set1X();
  oled.setInvertMode(true);
  oled.setCursor(0, 0);
  oled.print(F(" SETTINGS"));
  oled.clearToEOL();
  oled.setInvertMode(false);

  if (menuItem < menuTop) menuTop = menuItem;
  if (menuItem >= menuTop + MENU_ROWS) menuTop = menuItem - MENU_ROWS + 1;

  for (uint8_t row = 0; row < MENU_ROWS; row++) {
    uint8_t item = menuTop + row;
    oled.setCursor(0, row + 2);
    if (item < MENU_ITEMS) {
      oled.print(item == menuItem ? F(">") : F(" "));
      menuLabel(item);
      oled.clearToEOL();
      menuValue(item);
    }
    oled.clearToEOL();
  }
}

void redraw() {
  if (screenAsleep) return;
  if (screen == SCREEN_HOME) drawHome(); else drawMenu();
  needsRedraw = false;
}

void sleepScreen() {
  screenAsleep = true;
  oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
}

void wakeScreen() {
  screenAsleep = false;
  oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
  needsRedraw = true;
}

void showScreen(Screen s) {
  screen = s;
  resetArmed = false;
  oled.clear();
  needsRedraw = true;
}

// ================================================================ editing
void stepAddress(int8_t dir) {
  int16_t a = (int16_t)settings.address + dir;
  if (a > (int16_t)DMX_MAX_ADDRESS) a = 1;
  if (a < 1) a = DMX_MAX_ADDRESS;
  settings.address = a;
  markChanged();
}

void stepMenuValue(int8_t dir) {
  if (menuItem < MENU_LEVEL_ITEMS) {
    uint8_t out = menuItem / 2;
    uint8_t &v = (menuItem % 2 == 0) ? settings.minLevel[out] : settings.maxLevel[out];
    v = (uint8_t)(v + dir);  // wraps 255 <-> 0
  } else if (menuItem < MENU_CURVE_ITEMS) {
    uint8_t &c = settings.ledCurve[menuItem - MENU_LEVEL_ITEMS];
    c = !c;
  } else if (menuItem == MENU_LOSS_ITEM) {
    settings.lossMode = (settings.lossMode + 1) % LOSS_MODES;
  } else {
    return;  // reset is handled on click only
  }
  markChanged();
}

// Returns true if the press was swallowed to wake the display.
bool wakeOnPress() {
  lastInputMs = millis();
  if (screenAsleep) {
    wakeScreen();
    return true;
  }
  return false;
}

void onUpClick() {
  if (wakeOnPress()) return;
  if (screen == SCREEN_HOME) {
    stepAddress(+1);
  } else if (menuItem == MENU_RESET_ITEM) {
    if (!resetArmed) {
      resetArmed = true;
      needsRedraw = true;
    } else {
      setDefaults();
      saveSettings();
      showScreen(SCREEN_HOME);
    }
  } else {
    stepMenuValue(+1);
  }
}

void onDownClick() {
  if (wakeOnPress()) return;
  if (screen == SCREEN_HOME) {
    stepAddress(-1);
  } else {
    resetArmed = false;
    menuItem = (menuItem + 1) % MENU_ITEMS;
    needsRedraw = true;
  }
}

void onDownDoubleClick() {
  if (wakeOnPress()) return;
  showScreen(screen == SCREEN_HOME ? SCREEN_MENU : SCREEN_HOME);
  menuItem = 0;
  menuTop = 0;
}

void onLongPressStart() {
  holdStartMs = millis();
  lastRepeatMs = 0;
}

void holdRepeat(int8_t dir) {
  if (wakeOnPress()) return;
  unsigned long now = millis();
  unsigned long held = now - holdStartMs;
  unsigned long interval = held < 1500 ? 150 : (held < 4000 ? 40 : 10);
  if (now - lastRepeatMs < interval) return;
  lastRepeatMs = now;
  if (screen == SCREEN_HOME) {
    stepAddress(dir);
  } else if (menuItem < MENU_LEVEL_ITEMS) {
    stepMenuValue(dir);
  }
}

void onUpHold()   { holdRepeat(+1); }
void onDownHold() { holdRepeat(-1); }

// ================================================================ setup / loop
void setup() {
  MCUSR = 0;      // clear the reset flags so the watchdog can be turned off
  wdt_disable();  // in case we got here from a watchdog reset

  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
    analogWrite(OUTPUT_PINS[i], 0);
    outputLevel[i] = 0;
  }

  loadSettings();
  DMXSerial.init(DMXReceiver, PIN_DMX_ENABLE);

  upButton.attachClick(onUpClick);
  upButton.attachLongPressStart(onLongPressStart);
  upButton.attachDuringLongPress(onUpHold);
  downButton.attachClick(onDownClick);
  downButton.attachDoubleClick(onDownDoubleClick);
  downButton.attachLongPressStart(onLongPressStart);
  downButton.attachDuringLongPress(onDownHold);

  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(System5x7);
  oled.clear();

  lastInputMs = millis();

  // Restart automatically if the loop ever hangs for more than 2 seconds.
  wdt_enable(WDTO_2S);
}

void loop() {
  unsigned long now = millis();
  wdt_reset();

  upButton.tick();
  downButton.tick();

  updateOutputs();

  if (settingsDirty && now - lastInputMs > SAVE_DELAY_MS) saveSettings();

  if (screen == SCREEN_MENU && now - lastInputMs > MENU_TIMEOUT_MS) {
    showScreen(SCREEN_HOME);
  }
  if (!screenAsleep && now - lastInputMs > SCREEN_SLEEP_MS) sleepScreen();

  if (now - lastBlinkMs > BLINK_MS) {
    lastBlinkMs = now;
    blinkOn = !blinkOn;
    if (screen == SCREEN_HOME) needsRedraw = true;
  }

  if (needsRedraw) {
    redraw();
  } else if (!screenAsleep && screen == SCREEN_HOME && now - lastLevelDrawMs > LEVEL_REFRESH_MS) {
    lastLevelDrawMs = now;
    drawLevels();
  }
}
