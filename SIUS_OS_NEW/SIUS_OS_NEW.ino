#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
#include <string.h>
#include <avr/pgmspace.h>

// =====================================================================
// 📑 СТРУКТУРИ ТА ПОПЕРЕДНІ ОГОЛОШЕННЯ
// =====================================================================
struct MenuItem {
  const char* name;
  uint8_t actionId;
};

// Прототипи функцій
void drawMenu(MenuItem* items, uint8_t len, uint8_t selectedIndex, uint8_t menuLevel);
void handleAction(uint8_t actionId);

// =====================================================================
// ⚙️ АПАРАТНІ НАЛАШТУВАННЯ ТА ПІНИ
// =====================================================================
#define LED_PIN         7     // Пін WS2812B
#define NUM_LEDS        8     // Кількість світлодіодів
#define LED_TYPE        WS2812B
#define COLOR_ORDER     GRB

// Піни 5mm RGB Світлодіода (Спільний Катод)
#define RGB_RED_PIN     3     
#define RGB_GREEN_PIN   5     
#define RGB_BLUE_PIN    6     

// Пін бузера
#define BUZZER_PIN      12    

// Піни кнопок управління
#define pinSW           10    // Навігація / Вибір
#define pinStartStop    11    // START / STOP

// Піни виходів мішеней
#define ledPin8         8     // MP5/MP8 Зелений
#define ledPin9         9     // MP5/MP8 Червоний
#define ledPin2         2     // MP10 Зелений
#define ledPin4         4     // MP10 Червоний

CRGB leds[NUM_LEDS];
LiquidCrystal_I2C lcd(0x27, 20, 4);

uint8_t currentWsMode = 0;

// =====================================================================
// 🎶 ЗВУКОВА СИСТЕМА
// =====================================================================
void toneClick() {
  tone(BUZZER_PIN, 1800, 30);
}

void toneSelect() {
  tone(BUZZER_PIN, 2400, 60);
}

void toneStart() {
  tone(BUZZER_PIN, 1000, 150);
  delay(160);
  tone(BUZZER_PIN, 2000, 250);
}

void toneStop() {
  tone(BUZZER_PIN, 600, 400);
}

void toneAlert() {
  tone(BUZZER_PIN, 3000, 80);
}

// =====================================================================
// 🎨 УПРАВЛІННЯ 5mm RGB СВІТЛОДІОДОМ
// =====================================================================
void setRgbColor(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(RGB_RED_PIN,   r);
  analogWrite(RGB_GREEN_PIN, g);
  analogWrite(RGB_BLUE_PIN,  b);
}

// =====================================================================
// 📑 ПЕРЕЛІК ДІЙ ТА РЯДКИ PROGMEM
// =====================================================================
enum {
  ACT_NONE = 0,
  ACT_MP5,
  ACT_MODE_8,
  ACT_MODE_6,
  ACT_MODE_4,
  ACT_MODE_150,
  ACT_MODE_20,
  ACT_MODE_10,
  ACT_INFO,
  ACT_SUB_MP8,
  ACT_SUB_MP10,
  ACT_SUB_SETTINGS,
  ACT_BACK
};

const char str_sport_pistol[] PROGMEM = "Sport Pistol";
const char str_rapid_fire[]   PROGMEM = "Rapid Fire";
const char str_standard[]     PROGMEM = "Standard";
const char str_settings[]     PROGMEM = "Settings";

const char str_8_sec[]        PROGMEM = "8 Second";
const char str_6_sec[]        PROGMEM = "6 Second";
const char str_4_sec[]        PROGMEM = "4 Second";

const char str_150_sec[]      PROGMEM = "150 Second";
const char str_20_sec[]       PROGMEM = "20 Second";
const char str_10_sec[]       PROGMEM = "10 Second";

const char str_sys_info[]     PROGMEM = "System Information";
const char str_return_main[]  PROGMEM = "< Return Main >";

MenuItem mainMenu[] = {
  {str_sport_pistol, ACT_MP5},
  {str_rapid_fire,   ACT_SUB_MP8},
  {str_standard,     ACT_SUB_MP10},
  {str_settings,     ACT_SUB_SETTINGS}
};
uint8_t mainMenuLen = sizeof(mainMenu) / sizeof(MenuItem);

MenuItem mp8Menu[] = {
  {str_8_sec,        ACT_MODE_8},
  {str_6_sec,        ACT_MODE_6},
  {str_4_sec,        ACT_MODE_4},
  {str_return_main,  ACT_BACK}
};
uint8_t mp8MenuLen = sizeof(mp8Menu) / sizeof(MenuItem);

MenuItem mp10Menu[] = {
  {str_150_sec,      ACT_MODE_150},
  {str_20_sec,       ACT_MODE_20},
  {str_10_sec,       ACT_MODE_10},
  {str_return_main,  ACT_BACK}
};
uint8_t mp10MenuLen = sizeof(mp10Menu) / sizeof(MenuItem);

MenuItem settingsMenu[] = {
  {str_sys_info,     ACT_INFO},
  {str_return_main,  ACT_BACK}
};
uint8_t settingsMenuLen = sizeof(settingsMenu) / sizeof(MenuItem);

// =====================================================================
// 🔣 СИМВОЛИ ТА UI ФУНКЦІЇ LCD
// =====================================================================
const byte bulletFull[8]    PROGMEM = { 0b00000, 0b00000, 0b01110, 0b11111, 0b11111, 0b01110, 0b00000, 0b00000 };
const byte bulletEmpty[8]   PROGMEM = { 0b00000, 0b00000, 0b01110, 0b10001, 0b10001, 0b01110, 0b00000, 0b00000 };
const byte bulletActive[8]  PROGMEM = { 0b00100, 0b10101, 0b01110, 0b11111, 0b01110, 0b10101, 0b00100, 0b00000 };
const byte progressFull[8]  PROGMEM = { 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111 };
const byte selectLeft[8]    PROGMEM = { 0b00010, 0b00110, 0b01110, 0b11110, 0b01110, 0b00110, 0b00010, 0b00000 };
const byte selectRight[8]   PROGMEM = { 0b01000, 0b01100, 0b01110, 0b01111, 0b01110, 0b01100, 0b01000, 0b00000 };
const byte progressEmpty[8] PROGMEM = { 0b11111, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111 };
const byte borderCorner[8]  PROGMEM = { 0b11111, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000 };

void createCharProgmem(uint8_t index, const byte* data) {
  byte buffer[8];
  for (uint8_t i = 0; i < 8; i++) {
    buffer[i] = pgm_read_byte(data + i);
  }
  lcd.createChar(index, buffer);
}

void initCustomChars() {
  createCharProgmem(0, bulletFull);
  createCharProgmem(1, bulletEmpty);
  createCharProgmem(2, bulletActive);
  createCharProgmem(3, progressFull);
  createCharProgmem(4, selectLeft);
  createCharProgmem(5, selectRight);
  createCharProgmem(6, progressEmpty);
  createCharProgmem(7, borderCorner);
}

void printCentered(uint8_t row, const char* text) {
  char rowBuffer[21];
  memset(rowBuffer, ' ', 20);
  rowBuffer[20] = '\0';

  int len = strlen(text);
  if (len > 20) len = 20;
  int startCol = (20 - len) / 2;

  memcpy(rowBuffer + startCol, text, len);
  
  lcd.setCursor(0, row);
  lcd.print(rowBuffer);
}

void printCentered_F(uint8_t row, const __FlashStringHelper* text) {
  char buffer[21];
  strncpy_P(buffer, (PGM_P)text, 20);
  buffer[20] = '\0';
  printCentered(row, buffer);
}

// =====================================================================
// 💡 СИСТЕМА ЕФЕКТІВ І СВІТЛОДІОДІВ
// =====================================================================
void updateWs2812(uint8_t mode) {
  static unsigned long lastStepTime = 0;
  static uint8_t animStep = 0;
  static uint8_t hueVal = 0;
  static bool toggleState = false;
  unsigned long currentMillis = millis();

  switch (mode) {
    case 0:
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      setRgbColor(0, 0, 0);
      animStep = 0;
      break;

    case 1:
      if (currentMillis - lastStepTime >= 25) {
        lastStepTime = currentMillis;
        fill_rainbow(leds, NUM_LEDS, hueVal++, 30);
        FastLED.show();
        
        uint8_t breath = quadwave8(hueVal * 2);
        setRgbColor(0, breath / 4, breath / 2);
      }
      break;

    case 2:
      if (currentMillis - lastStepTime >= 15) {
        lastStepTime = currentMillis;
        animStep++;
        uint8_t val = quadwave8(animStep);
        fill_solid(leds, NUM_LEDS, CHSV(190, 255, val));
        FastLED.show();
        setRgbColor(val / 2, 0, val);
      }
      break;

    case 3:
      if (currentMillis - lastStepTime >= 200) {
        lastStepTime = currentMillis;
        toggleState = !toggleState;
        if (toggleState) {
          fill_solid(leds, NUM_LEDS, CRGB::Red);
          setRgbColor(255, 0, 0);
        } else {
          fill_solid(leds, NUM_LEDS, CRGB::Black);
          setRgbColor(30, 0, 0);
        }
        FastLED.show();
      }
      break;

    case 4:
      fill_solid(leds, NUM_LEDS, CRGB::Green);
      FastLED.show();
      setRgbColor(0, 255, 0);
      break;

    case 5:
      if (currentMillis - lastStepTime >= 80) {
        lastStepTime = currentMillis;
        animStep++;
        fill_solid(leds, NUM_LEDS, (animStep % 2 == 0) ? CRGB::Yellow : CRGB::Green);
        FastLED.show();
        setRgbColor((animStep % 2 == 0) ? 255 : 0, 255, 0);
      }
      break;
  }
}

// =====================================================================
// 🚀 СТАБІЛЬНИЙ ЕКРАН ЗАВАНТАЖЕННЯ БЕЗ ГЛЮКІВ ТА МЕРЕХТІННЯ
// =====================================================================
void showBootBarAnimation() {
  lcd.clear();
  
  lcd.setCursor(0, 0); lcd.print(F("===================="));
  lcd.setCursor(0, 1); lcd.print(F("|                  |"));
  lcd.setCursor(0, 2); lcd.print(F("|                  |"));
  lcd.setCursor(0, 3); lcd.print(F("===================="));

  delay(100);
  
  lcd.setCursor(1, 1);
  lcd.print(F(" SIUS 25M PRO v3.2"));

  const int barWidth = 10; 
  const int startCol = 2;

  int lastPercent = -1;

  for (int percent = 0; percent <= 100; percent += 2) {
    if (percent != lastPercent) {
      lastPercent = percent;
      
      lcd.setCursor(startCol, 2);
      int filledBlocks = map(percent, 0, 100, 0, barWidth);
      
      lcd.print("[");
      for (int b = 0; b < barWidth; b++) {
        if (b < filledBlocks) lcd.write(byte(3));
        else lcd.write(byte(6));
      }
      lcd.print("]");

      if (percent < 10) lcd.print(" ");
      if (percent < 100) lcd.print(" ");
      lcd.print(percent);
      lcd.print("%");
    }

    uint8_t activeLeds = map(percent, 0, 100, 0, NUM_LEDS);
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      if (i < activeLeds) {
        uint8_t hue = map(i, 0, NUM_LEDS - 1, 160, 96); 
        leds[i] = CHSV(hue, 255, 255);
      } else {
        leds[i] = CRGB::Black;
      }
    }
    FastLED.show();

    uint8_t rgbHue = map(percent, 0, 100, 160, 96);
    CRGB rgbTmp = CHSV(rgbHue, 255, 255);
    setRgbColor(rgbTmp.r, rgbTmp.g, rgbTmp.b);

    if (percent % 20 == 0) toneClick();

    delay(15);
  }

  fill_solid(leds, NUM_LEDS, CRGB::White);
  FastLED.show();
  setRgbColor(255, 255, 255);
  toneStart();
  
  lcd.setCursor(1, 1);
  lcd.print(F("   SYSTEM READY!  "));
  delay(400);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  setRgbColor(0, 0, 0);
  
  lcd.clear();
}

// =====================================================================
// 🎯 ВІДОБРАЖЕННЯ ПРОГРЕСУ ТА СЕРІЙ
// =====================================================================
void drawSeriesCounter(uint8_t row, uint8_t totalRounds, uint8_t currentRound, bool isBlinking) {
  int totalWidth = totalRounds * 2 - 1;
  int startCol = (20 - totalWidth) / 2;
  if (startCol < 0) startCol = 0;

  lcd.setCursor(0, row);
  for (int i = 0; i < startCol; i++) lcd.print(" ");

  for (uint8_t i = 0; i < totalRounds; i++) {
    if (i < currentRound) {
      lcd.write(byte(1));
    } else if (i == currentRound) {
      if (isBlinking) lcd.write(byte(2));
      else lcd.write(byte(0));
    } else {
      lcd.write(byte(0));
    }
    if (i < totalRounds - 1) lcd.print(" ");
  }

  int writtenCols = startCol + totalWidth;
  for (int i = writtenCols; i < 20; i++) lcd.print(" ");
}

void drawProgressBar(uint8_t row, unsigned long elapsed, unsigned long total) {
  if (total == 0) return;
  uint8_t blocks = (elapsed * 20) / total;
  if (blocks > 20) blocks = 20;
  
  lcd.setCursor(0, row);
  for (uint8_t i = 0; i < 20; i++) {
    if (i < blocks) lcd.write(byte(3));
    else lcd.print(" ");
  }
}

void turnOffAllLeds() {
  digitalWrite(ledPin8, LOW);
  digitalWrite(ledPin9, LOW);
  digitalWrite(ledPin2, LOW);
  digitalWrite(ledPin4, LOW);
  setRgbColor(0, 0, 0);
}

// =====================================================================
// 🕹️ ЧИТАННЯ КНОПОК
// =====================================================================
int readMenuButton() {
  static bool rawPressed = false;
  static bool btnPressed = false;
  static unsigned long lastDebounceTime = 0;
  static unsigned long pressStartTime = 0;

  bool currentRead = (digitalRead(pinSW) == LOW);

  if (currentRead != rawPressed) {
    lastDebounceTime = millis();
    rawPressed = currentRead;
  }

  if ((millis() - lastDebounceTime) > 30) {
    if (rawPressed && !btnPressed) {
      btnPressed = true;
      pressStartTime = millis();
    } else if (!rawPressed && btnPressed) {
      unsigned long duration = millis() - pressStartTime;
      btnPressed = false;
      if (duration >= 30 && duration < 400) {
        toneClick();
        return 1; 
      } else if (duration >= 400) {
        toneSelect();
        return 2; 
      }
    }
  }
  return 0;
}

bool readStartStopButton() {
  static bool rawPressed = false;
  static bool btnPressed = false;
  static unsigned long lastDebounceTime = 0;
  static unsigned long pressStartTime = 0;

  bool currentRead = (digitalRead(pinStartStop) == LOW);

  if (currentRead != rawPressed) {
    lastDebounceTime = millis();
    rawPressed = currentRead;
  }

  if ((millis() - lastDebounceTime) > 30) {
    if (currentRead && !btnPressed) {
      btnPressed = true;
      pressStartTime = millis();
    } else if (!currentRead && btnPressed) {
      unsigned long duration = millis() - pressStartTime;
      btnPressed = false;
      if (duration >= 30) {
        toneClick();
        return true; 
      }
    }
  }
  return false;
}

bool checkAnyTrigger() {
  if (readStartStopButton()) return true;
  if (readMenuButton() != 0) return true;
  return false;
}

// =====================================================================
// ⏱️ ТАЙМЕР З ПЕРЕРЕВАШДЕННЯМ ТА ПЕРЕВІРКОЮ
// =====================================================================
bool waitWithAbort(unsigned long ms, uint8_t totalRounds, uint8_t currentRound) {
  unsigned long start = millis();
  unsigned long lastAnimTime = 0;
  bool blinkState = false;

  while (true) {
    unsigned long now = millis();
    unsigned long elapsed = now - start;

    if (elapsed >= ms) break;

    updateWs2812(currentWsMode);

    if (now - lastAnimTime >= 100) {
      lastAnimTime = now;
      blinkState = !blinkState;
      if (totalRounds > 0) {
        drawSeriesCounter(1, totalRounds, currentRound, blinkState);
      }
      drawProgressBar(2, elapsed, ms);
    }

    if (checkAnyTrigger()) { 
      toneStop();
      turnOffAllLeds();
      return false; 
    }
  }
  return true; 
}

// =====================================================================
// 🎯 РЕЖИМИ СТРІЛЬБИ
// =====================================================================
void runSeries(const char* title, int redPin, int greenPin, unsigned long greenMs) {
  while (true) {
    turnOffAllLeds();
    lcd.clear();
    printCentered(0, title);
    printCentered_F(2, F("BTN 10/11: START"));
    printCentered_F(3, F("Hold SW: BACK"));

    currentWsMode = 2;
    while (true) {
      updateWs2812(currentWsMode);
      
      if (readStartStopButton()) break;
      int btn = readMenuButton();
      if (btn == 1) break; 
      if (btn == 2) return; 
    }

    lcd.clear();
    printCentered(0, title);
    printCentered_F(3, F("BTN 10/11: STOP"));

    bool aborted = false;

    // 1. Червоний
    turnOffAllLeds();
    currentWsMode = 3;
    toneAlert();
    digitalWrite(redPin, HIGH);
    if (!waitWithAbort(7000, 0, 0)) aborted = true;
    if (aborted) {
      turnOffAllLeds();
      continue;
    }

    // 2. Зелений (перемикання напряму без паузи)
    turnOffAllLeds();
    digitalWrite(greenPin, HIGH);
    currentWsMode = 4;
    toneStart();
    if (!waitWithAbort(greenMs, 0, 0)) aborted = true;
    if (aborted) {
      turnOffAllLeds();
      continue;
    }

    // 3. Фінальний червоний
    turnOffAllLeds();
    digitalWrite(redPin, HIGH);
    currentWsMode = 3;
    toneStop();
    if (!waitWithAbort(7000, 0, 0)) aborted = true;
    turnOffAllLeds();
  }
}

void runMP5() {
  while (true) {
    turnOffAllLeds();
    lcd.clear();
    printCentered_F(0, F("Sport Pistol"));
    printCentered_F(2, F("BTN 10/11: START"));
    printCentered_F(3, F("Hold SW: BACK"));

    currentWsMode = 2;
    while (true) {
      updateWs2812(currentWsMode);
      if (readStartStopButton()) break;
      int btn = readMenuButton();
      if (btn == 1) break;
      if (btn == 2) return;
    }

    lcd.clear();
    printCentered_F(0, F("MP5 RUNNING..."));
    printCentered_F(3, F("BTN 10/11: STOP"));

    turnOffAllLeds();

    const uint8_t TOTAL_SERIES = 5;
    bool aborted = false;

    for (int i = 0; i < TOTAL_SERIES; i++) {
      turnOffAllLeds();
      digitalWrite(ledPin9, HIGH);
      currentWsMode = 3;
      if (i == 0) toneAlert();
      
      if (!waitWithAbort(7000, TOTAL_SERIES, i)) { aborted = true; break; }

      turnOffAllLeds();
      digitalWrite(ledPin8, HIGH);
      currentWsMode = 4;
      toneStart();
      
      if (!waitWithAbort(3000, TOTAL_SERIES, i)) { aborted = true; break; }
      turnOffAllLeds();
    }

    if (aborted) {
      turnOffAllLeds();
      continue;
    }

    turnOffAllLeds();
    digitalWrite(ledPin9, HIGH);
    currentWsMode = 3;
    toneStop();
    
    if (!waitWithAbort(7000, TOTAL_SERIES, TOTAL_SERIES - 1)) {
      turnOffAllLeds();
      continue;
    }
    turnOffAllLeds();

    currentWsMode = 5;
    toneStart();
    drawSeriesCounter(1, TOTAL_SERIES, TOTAL_SERIES, false);
    printCentered_F(2, F("SERIES FINISHED!"));
    
    while (!checkAnyTrigger()) {
      updateWs2812(currentWsMode);
    }
    turnOffAllLeds();
  }
}

// =====================================================================
// ℹ️ ІНФОРМАЦІЙНИЙ ЕКРАН
// =====================================================================
void showInfo() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("+------------------+"));
  lcd.setCursor(0, 3); lcd.print(F("+------------------+"));

  lcd.setCursor(1, 1); lcd.print(F("SIUS 25M PRO v3.2 "));
  lcd.setCursor(1, 2); lcd.print(F("RGB+BUZZER HI-PREC "));
  
  currentWsMode = 1;
  
  while(true) {
    updateWs2812(currentWsMode);
    if (checkAnyTrigger()) {
      toneClick();
      break;
    }
  }
}

// =====================================================================
// 🌟 СИСТЕМА МЕНЮ
// =====================================================================
void drawMenu(MenuItem* items, uint8_t len, uint8_t selectedIndex, uint8_t menuLevel) {
  if (menuLevel == 0) {
    printCentered_F(0, F("=== MAIN ==="));
  } else if (menuLevel == 1) {
    printCentered_F(0, F("-- RAPID FIRE --"));
  } else if (menuLevel == 2) {
    printCentered_F(0, F("-- STANDARD --"));
  } else if (menuLevel == 3) {
    printCentered_F(0, F("-- SETTINGS --"));
  } else {
    printCentered_F(0, F("=== MENU ==="));
  }
  
  uint8_t startIdx = 0;
  if (selectedIndex >= 3) {
    startIdx = selectedIndex - 2;
  }

  for (uint8_t i = 0; i < 3; i++) {
    uint8_t itemIdx = startIdx + i;
    uint8_t row = i + 1;
    
    if (itemIdx < len) {
      char itemName[17];
      strncpy_P(itemName, (PGM_P)items[itemIdx].name, 16);
      itemName[16] = '\0';
      int nameLen = strlen(itemName);

      char rowBuffer[21];
      memset(rowBuffer, ' ', 20);
      rowBuffer[20] = '\0';

      if (itemIdx == selectedIndex) {
        int totalBlockLen = nameLen + 4;
        int startCol = (20 - totalBlockLen) / 2;
        if (startCol < 0) startCol = 0;

        rowBuffer[startCol] = byte(4);
        memcpy(rowBuffer + startCol + 2, itemName, nameLen);
        rowBuffer[startCol + 2 + nameLen + 1] = byte(5);
      } else {
        int startCol = (20 - nameLen) / 2;
        if (startCol < 0) startCol = 0;

        memcpy(rowBuffer + startCol, itemName, nameLen);
      }

      lcd.setCursor(0, row);
      lcd.print(rowBuffer);
    } else {
      lcd.setCursor(0, row);
      lcd.print(F("                    "));
    }
  }
}

void handleAction(uint8_t actionId) {
  switch(actionId) {
    case ACT_MP5:     
      runMP5(); 
      break;
    case ACT_MODE_8:  
      runSeries("Mode: 8 sec", ledPin9, ledPin8, 8000); 
      break;
    case ACT_MODE_6:  
      runSeries("Mode: 6 sec", ledPin9, ledPin8, 6000); 
      break;
    case ACT_MODE_4:  
      runSeries("Mode: 4 sec", ledPin9, ledPin8, 4000); 
      break;
    case ACT_MODE_150: 
      runSeries("Mode: 150 sec", ledPin4, ledPin2, 150000); 
      break;
    case ACT_MODE_20:  
      runSeries("Mode: 20 sec", ledPin4, ledPin2, 20000); 
      break;
    case ACT_MODE_10:  
      runSeries("Mode: 10 sec", ledPin4, ledPin2, 10000); 
      break;
    case ACT_INFO:    
      showInfo(); 
      break;
    default: 
      break;
  }
}

void runMenuSystem() {
  uint8_t currentSubMenu = 0; 
  uint8_t selectedIndex = 0;
  bool needRedraw = true;
  
  while (true) {
    MenuItem* currentItems;
    uint8_t currentLen;

    if (currentSubMenu == 0) {
      currentItems = mainMenu;
      currentLen = mainMenuLen;
    } else if (currentSubMenu == 1) {
      currentItems = mp8Menu;
      currentLen = mp8MenuLen;
    } else if (currentSubMenu == 2) {
      currentItems = mp10Menu;
      currentLen = mp10MenuLen;
    } else {
      currentItems = settingsMenu;
      currentLen = settingsMenuLen;
    }

    if (selectedIndex >= currentLen) selectedIndex = 0;
    
    if (needRedraw) {
      drawMenu(currentItems, currentLen, selectedIndex, currentSubMenu);
      needRedraw = false;
    }

    currentWsMode = 1;

    while (true) {
      updateWs2812(currentWsMode);

      int btn = readMenuButton();
      
      if (btn == 1) { 
        selectedIndex = (selectedIndex + 1) % currentLen;
        needRedraw = true;
        break;
      } 
      else if (btn == 2) { 
        uint8_t action = currentItems[selectedIndex].actionId;
        
        if (action == ACT_BACK) {
          currentSubMenu = 0;
          selectedIndex = 0;
        } else if (action == ACT_SUB_MP8) {
          currentSubMenu = 1;
          selectedIndex = 0;
        } else if (action == ACT_SUB_MP10) {
          currentSubMenu = 2;
          selectedIndex = 0;
        } else if (action == ACT_SUB_SETTINGS) {
          currentSubMenu = 3;
          selectedIndex = 0;
        } else {
          handleAction(action);
        }
        needRedraw = true;
        break;
      }
    }
  }
}

// =====================================================================
// ⚙️ ОСНОВНИЙ SETUP ТА LOOP
// =====================================================================
void setup() {
  lcd.init();
  lcd.backlight();
  initCustomChars();

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(60);

  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinStartStop, INPUT_PULLUP);

  pinMode(ledPin8, OUTPUT);
  pinMode(ledPin9, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin4, OUTPUT);

  turnOffAllLeds();

  showBootBarAnimation();
}

void loop() {
  runMenuSystem();
}