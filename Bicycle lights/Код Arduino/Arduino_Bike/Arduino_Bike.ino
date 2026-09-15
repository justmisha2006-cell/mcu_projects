#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Adafruit_SH1106.h>
#include "RTClib.h"
#include <EEPROM.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
//#define OLED_RESET     12 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
//#define i2c_Address 0x3c
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//Adafruit_SH1106 display = Adafruit_SH1106(OLED_RESET);

RTC_DS3231 rtc;

float wheelCircumference = 2.00; //meters (can be set from the settings page of the device)
const float speedUpdateInterval = 5000; //calculate speed every N milliseconds

const long turnSignalBlink = 500; //speed at which the turn signals blink in milliseconds
const int turnSignalMaxCycles = 12; //number of turn signal blinks before they turn off automatically

const int wheelSensor = 2; //pin to which the wheel hall effect sensor is connected

const int brakeSensor = 7; //pin to which the brake hall effect sensor is connected
const bool invertBrakeSensor = false; //true: brake detected when the magnet is far from the sensor

const int LEFTbutton = 3; //pins to which the three buttons are connected
const int RIGHTbutton = 4;
const int SETbutton = 5;

const int LEFTled = 9; //pins to which the lights are connected
const int RIGHTled = 10;
const int REDled = 11;
const int WHITEled = 6;

float totalKm = 0; //variable that stores total distance
float speed = 0; //variable that stores current speed
float maxSpeed = 0; //variable that stores maximum speed
float avgSpeed = 0; //variable that stores average speed
long activeTime = 0; //variable that stores active time

float speedMillis = 0; //milliseconds from the last time speed was calculated

bool frontLights = false; //state of front lights
bool backLights = true; //state of back lights
bool brakeLights = false; //state of brake lights
bool left = false; //state of left turn signal
bool right = false; //state of right turn signal
bool turnSignalState = true;
bool lastLeftState = true;
bool lastRightState = true;

bool leftReleased = true;
bool rightReleased = true;
bool setReleased = true;

int lightsCycle = 0;
int turnSignalCycle = 0;
long previousMillis = 0;
long releaseMillis = 0;
long lightsMillis = 0;

int displayPage = 0; //0: home screen - 1: info screen - 2: control screen
int menuSelection = 0;
bool pageUpdated = false;

int batteryState = 0;
const int batteryReadingDeadband = 10;

int pulses = 0;

void setup() {
  pinMode(wheelSensor, INPUT);
  pinMode(brakeSensor, INPUT);
  pinMode(LEFTbutton, INPUT_PULLUP);
  pinMode(RIGHTbutton, INPUT_PULLUP);
  pinMode(SETbutton, INPUT_PULLUP);
  pinMode(LEFTled,  OUTPUT);
  pinMode(RIGHTled, OUTPUT);
  pinMode(REDled, OUTPUT);
  pinMode(WHITEled, OUTPUT);

  analogReference(INTERNAL);

  //Serial.begin(9600);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  //display.begin(SH1106_SWITCHCAPVCC, 0x3C);

  rtc.begin();

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  for(int i = -20; i <= 24; i++) {
    display.fillRect(0, 0, 128, 64, BLACK);
    display.setCursor(28, i);
    display.print("Hello!");
    display.display();
    delay(2);
  }
  delay(500);

  display.clearDisplay();
  delay(200);
  
  bool settings = false;
  bool settingsChanged = false;
  int settingsPage = 0;
  int selection = 0;

  DateTime now = rtc.now();
  int h = now.hour();
  int m = now.minute();

  EEPROM.get(0, wheelCircumference);

  //enter settings when the SET button is pressed during startup
  if(digitalRead(SETbutton) == 0 || isnan(wheelCircumference)) {
    setReleased = false;
    delay(100);
    display.clearDisplay();
    settings = true;
    //remain into the settings
    while(settings == true) {
      //detect if the SET button has been released after being pressed
      if(digitalRead(SETbutton) == 1 && setReleased == false) {
        setReleased = true;
        delay(100);
      }

      //main settings menu
      if(settingsPage == 0) {
        display.fillRect(0, 0, 128, 64, BLACK);

        //move between the settings using the LEFT and RIGHT buttons
        if(digitalRead(RIGHTbutton) == 0) {
          selection++;
          if(selection > 3) selection = 0;
          delay(200);
        }

        if(digitalRead(LEFTbutton) == 0) {
          selection--;
          if(selection < 0) selection = 3;
          delay(200);
        }

        display.setTextColor(WHITE);
        display.setTextSize(2);
        display.setCursor(15, 0);
        display.print("Settings");

        display.drawLine(0, 28, 127, 28, WHITE);
        display.drawLine(0, 52, 127, 52, WHITE);
        display.setCursor(0, 33);
        display.print("<");
        display.setCursor(116, 33);
        display.print(">");

        if(selection == 0) {
          display.setCursor(18, 33);
          display.print("Exit");

          //exit from the settings if "Exit" is selected
          if(digitalRead(SETbutton) == 0 && setReleased == true) {
            settings = false;
            delay(200);
          }
        }

        if(selection == 1) {
          display.setCursor(18, 33);
          display.print("Time");
        }

        if(selection == 2) {
          display.setCursor(18, 33);
          display.print("Wheel");
        }

        if(selection == 3) {
          display.setCursor(18, 33);
          display.print("Sensors");

        }

        //if SET is pressed, go into the menu that has been chosen
        if(digitalRead(SETbutton) == 0 && setReleased == true && selection != 0) {
          settingsPage = selection;
          selection = 0;
          setReleased = false;
          display.clearDisplay();
          delay(200);
        }
        //display.display();
      }

      display.display();
    
      //settings page for entering the current time
      if(settingsPage == 1) {
        display.fillRect(0, 0, 128, 64, BLACK);

        //move between the settings using the SET button
        if(digitalRead(SETbutton) == 0 && setReleased == true) {
          selection++;
          if(selection > 2) selection = 0;
          setReleased = false;

          delay(200);

          //Serial.println(selection);
        }
        
        if(selection == 0) {
          display.fillRect(0, 0, 128, 18, WHITE);
          display.setTextColor(BLACK);

          if(digitalRead(RIGHTbutton) == 0 || digitalRead(LEFTbutton) == 0) {
            settingsPage = 0;
            if(settingsChanged == true) {
              rtc.adjust(DateTime(2024, 1, 1, h, m, 0)); //set date-time manually: yr, mo, dy, hr, mn, sec
              settingsChanged = false;
              settingsConfirmation();
              delay(1200);
              display.clearDisplay();
            }
            delay(200);
          }
        }

        else {
          display.fillRect(0, 0, 128, 18, BLACK);
          display.setTextColor(WHITE);
        }
        
        display.setTextSize(2);
        display.setCursor(0, 2);
        display.print("<");
        display.setCursor(18, 2);
        display.print("Set time");

        display.setTextSize(3);

        if(selection == 1) {
          display.fillRect(14, 29, 41, 25, WHITE);
          display.setTextColor(BLACK);

          if(digitalRead(RIGHTbutton) == 0) {
            h++;
            if(h > 23) h = 0;
            settingsChanged = true;
            delay(200);
          }

          if(digitalRead(LEFTbutton) == 0) {
            h--;
            if(h < 0) h = 23;
            settingsChanged = true;
            delay(200);
          }
        }

        else {
          display.fillRect(14, 29, 41, 25, BLACK);
          display.setTextColor(WHITE);
        }

        display.setCursor(18, 31);
        if(h < 10) display.print(0);
        display.print(h);

        display.setTextColor(WHITE);
        display.print(":");

        if(selection == 2) {
          display.fillRect(68, 29, 41, 25, WHITE);
          display.setTextColor(BLACK);

          if(digitalRead(RIGHTbutton) == 0) {
            m++;
            if(m > 59) m = 0;
            settingsChanged = true;
            delay(200);
          }

          if(digitalRead(LEFTbutton) == 0) {
            m--;
            if(m < 0) m = 59;
            settingsChanged = true;
            delay(200);
          }
        }

        else {
          display.fillRect(68, 29, 41, 25, BLACK);
          display.setTextColor(WHITE);
        }

        if(m < 10) display.print(0);
        display.print(m);

        //display.display();
      }

      if(settingsPage == 2) {
        display.fillRect(0, 0, 128, 64, BLACK);

        if(isnan(wheelCircumference)) wheelCircumference = 2.0;

        if(digitalRead(SETbutton) == 0 && setReleased == true) {
          selection++;
          if(selection > 1) selection = 0;
          setReleased = false;

          delay(200);
        }
        
        if(selection == 0) {
          display.fillRect(0, 0, 128, 18, WHITE);
          display.setTextColor(BLACK);

          if(digitalRead(RIGHTbutton) == 0 || digitalRead(LEFTbutton) == 0) {
            settingsPage = 0;
            if(settingsChanged == true) {
              EEPROM.put(0, wheelCircumference);
              settingsChanged = false;
              settingsConfirmation();
              delay(1200);
              display.clearDisplay();
            }
            delay(200);
          }
        }

        else {
          display.fillRect(0, 0, 128, 18, BLACK);
          display.setTextColor(WHITE);
        }
        
        display.setTextSize(2);
        display.setCursor(0, 2);
        display.print("<");
        display.setCursor(18, 2);
        display.print("Set wheel");

        display.setTextSize(3);

        if(selection == 1) {
          display.fillRect(6, 27, 113, 29, WHITE);
          display.setTextColor(BLACK);

          if(digitalRead(RIGHTbutton) == 0) {
            wheelCircumference += 0.01;
            if(wheelCircumference > 5.00) wheelCircumference = 5.00;
            settingsChanged = true;
            delay(150);
          }

          if(digitalRead(LEFTbutton) == 0) {
            wheelCircumference -= 0.01;
            if(wheelCircumference < 0.10) wheelCircumference = 0.10;
            settingsChanged = true;
            delay(150);
          }
        }

        else {
          display.fillRect(6, 27, 113, 29, BLACK);
          display.setTextColor(WHITE);
        }

        display.setCursor(10, 31);
        display.print(wheelCircumference);
        display.setTextSize(2);
        display.setCursor(93, 38);
        display.print("mt");

       // display.display();
      }

      if(settingsPage == 3) {
        display.fillRect(0, 0, 128, 64, BLACK);
        
        if(digitalRead(RIGHTbutton) == 0 || digitalRead(LEFTbutton) == 0) {
          settingsPage = 0;
          delay(200);
        }

        display.fillRect(0, 0, 128, 18, WHITE);
        display.setTextColor(BLACK);
        display.setTextSize(2);
        display.setCursor(0, 2);
        display.print("<");
        display.setCursor(18, 2);
        display.print("Sensors");

        display.setTextColor(WHITE);
        display.setCursor(0, 26);
        display.print("Wheel: ");
        if(digitalRead(wheelSensor) == 0) {
          display.print("ON");
        }
        else {
          display.print("OFF");
        }
        display.setCursor(0, 48);
        display.print("Brake: ");
        if(digitalRead(brakeSensor) == 0) {
          display.print("ON");
        }
        else {
          display.print("OFF");
        }
      }  
    }
  }

  EEPROM.get(0, wheelCircumference);

  //Serial.println(wheelCircumference);

  attachInterrupt(digitalPinToInterrupt(wheelSensor), interruptFunction, RISING);
}

void loop() {
  //update the display every 250ms
  if(millis() % 250 == 0) {
    update();
  }

  if(millis() - lightsMillis >= 180) {

    lightsMillis = millis();

    lightsCycle++; //0 to 1: red bright - 2 to 5: red dim
    if(lightsCycle > 5) lightsCycle = 0;
  }

  if(millis() - speedMillis >= speedUpdateInterval) {

    calculateSpeed();

    speedMillis = millis();
  }

  if(frontLights == true) {    
    if(left == true || right == true) {
      analogWrite(WHITEled, 20);
    }
    else {
      analogWrite(WHITEled, 180);
    }
  }

  if(frontLights == false) {
    analogWrite(WHITEled, 0);
  }

  //blinking red headlight
  if(backLights == true) {
    if(lightsCycle < 1 && left == false && right == false && brakeLights == false) {
      analogWrite(REDled, 200);
    }
    if((lightsCycle >= 1 || (lightsCycle < 1 && (left == true || right == true))) && brakeLights == false) {
      analogWrite(REDled, 20);
    }
    
    if(brakeLights == true) {
      analogWrite(REDled, 255);
    }
  }

  if(backLights == false) {
    analogWrite(REDled, 0);
  }

  if(digitalRead(brakeSensor) == invertBrakeSensor) {
    brakeLights = true;
  }
  else {
    brakeLights = false;
  }
  
  //turn LEFT turn signal ON
  if(digitalRead(LEFTbutton) == 0 && left == false && right == false && displayPage == 0 && leftReleased == true) {
    left = true;
    turnSignalState = true;
    turnSignalCycle = 0;

    leftReleased = false;
    releaseMillis = millis();
  }

  //turn RIGHT turn signal ON
  if(digitalRead(RIGHTbutton) == 0 && right == false && left == false && displayPage == 0 && rightReleased == true) {
    right = true;
    turnSignalState = true;
    turnSignalCycle = 0;

    rightReleased = false;
    releaseMillis = millis();
  }

  //turn signals OFF when LEFT button is pressed
  if(digitalRead(LEFTbutton) == 0 && leftReleased == true && displayPage == 0) {
    if(left == true) {
      left = false;
      digitalWrite(LEFTled, LOW);
    }

    if(right == true) {
      right = false;
      digitalWrite(RIGHTled, LOW);
    }

    leftReleased = false;
    releaseMillis = millis();
  }

  //turn signals OFF when RIGHT button is pressed
  if(digitalRead(RIGHTbutton) == 0 && rightReleased == true && displayPage == 0) {
    if(left == true) {
      left = false;
      digitalWrite(LEFTled, LOW);
    }

    if(right == true) {
      right = false;
      digitalWrite(RIGHTled, LOW);
    }

    rightReleased = false;
    releaseMillis = millis();
  }

  //turn signals OFF when SET button is pressed
  if(digitalRead(SETbutton) == 0 && (left == true || right == true) && setReleased == true) {
    if(left == true) {
      left = false;
      digitalWrite(LEFTled, LOW);
    }

    if(right == true) {
      right = false;
      digitalWrite(RIGHTled, LOW);
    }

    setReleased = false;
    releaseMillis = millis();
  }

  if(digitalRead(SETbutton) == 0 && left == false && right == false && setReleased == true) {
    displayPage++;

    if(displayPage > 2) displayPage = 0;

    menuSelection = 0;

    pageUpdated = false;

    setReleased = false;
    releaseMillis = millis();
  }

  if(digitalRead(LEFTbutton) == 0 && displayPage == 2 && leftReleased == true) {
    menuSelection++;
    
    if(menuSelection > 2) menuSelection = 0;

    leftReleased = false;
    releaseMillis = millis();
  }

  if(digitalRead(RIGHTbutton) == 0 && displayPage == 2 && rightReleased == true) {
    bool clicked = false;

    if(menuSelection == 0) {
      if(frontLights == false && backLights == false && clicked == false) {
        frontLights = true;
        backLights = true;

        clicked = true;
      }
      
      if((frontLights == true || backLights == true) && clicked == false) {
        frontLights = false;
        backLights = false;

        clicked = true;
      }
    }

    if(menuSelection == 1) {
      frontLights = !frontLights;
    }

    if(menuSelection == 2) {
      backLights = !backLights;
    }

    rightReleased = false;
    releaseMillis = millis();
  }

  if(millis() - releaseMillis > 10000 && displayPage != 0) {
    displayPage = 0;
  }

  //detect if the buttons have been released
  if(digitalRead(SETbutton) == 1 && millis() - releaseMillis >= 200) {
    setReleased = true;
  }

  if(digitalRead(RIGHTbutton) == 1 && millis() - releaseMillis >= 200) {
    rightReleased = true;
  }

  if(digitalRead(LEFTbutton) == 1 && millis() - releaseMillis >= 200) {
    leftReleased = true;
  }

  //LEFT turn signal
  if(left == true) {

    if (millis() - previousMillis >= turnSignalBlink) {
      // save the last time you blinked the LED
      previousMillis = millis();

      digitalWrite(LEFTled, turnSignalState);

      turnSignalState = !turnSignalState;
      turnSignalCycle++;
    }

    if(turnSignalCycle > (turnSignalMaxCycles*2 - 1)) {
      left = false;
      digitalWrite(LEFTled, LOW);
    }
  }

  //RIGHT turn signal
  if(right == true) {

    if (millis() - previousMillis >= turnSignalBlink) {
      // save the last time you blinked the LED
      previousMillis = millis();

      digitalWrite(RIGHTled, turnSignalState);

      turnSignalState = !turnSignalState;
      turnSignalCycle++;
    }

    if(turnSignalCycle > (turnSignalMaxCycles*2 - 1)) {
      right = false;
      digitalWrite(RIGHTled, LOW);
    }
  }

  //delay(1);
}

void interruptFunction() {
  pulses++;
}

void calculateSpeed() {
  float staticPulses = pulses;
  pulses = 0;
  
  totalKm = totalKm + (staticPulses * wheelCircumference) / 1000.0;
  
  speed = (staticPulses * wheelCircumference * 3.6) / (speedUpdateInterval / 1000.0);

  if(speed > 2) activeTime += speedUpdateInterval;

  if(speed > maxSpeed) maxSpeed = speed;

  //Serial.println(speed);
}

void update() {
  //detachInterrupt(2);

  DateTime now = rtc.now();

  display.clearDisplay();
  
  if(displayPage == 0) {
    //display speed
    display.setTextSize(3);
    display.setCursor(1, 0);
    if(speed < 10) display.print(0);
    display.print(speed, 0);
    display.setTextSize(2);
    display.print(" Km/h");

    //display time
    display.setTextSize(2);
    display.setCursor(1, 29);
    if(now.hour() < 10) display.print(0);
    display.print(now.hour());
    display.print(":");
    if(now.minute() < 10) display.print(0);
    display.print(now.minute());
    
    //display total space
    display.setCursor(1, 50);
    if(totalKm < 10) display.print(0);
    display.print(totalKm);
    display.print(" Km");

    //display turn signals icons
    if(left == true && turnSignalState == true) {
      display.fillRect(110, 27, 18, 11, WHITE);
      display.fillTriangle(110, 20, 110, 44, 98, 32, WHITE);
    }
    if(right == true && turnSignalState == true) {
      display.fillRect(98, 27, 18, 11, WHITE);
      display.fillTriangle(115, 20, 115, 44, 127, 32, WHITE);
    }

    checkBattery();

    display.display();
  }

  if(displayPage == 1 && pageUpdated == false) {
    pageUpdated = true;

    display.drawLine(0, 31, 127, 31, WHITE);
    display.drawLine(63, 0, 63, 63, WHITE);
    
    //display total time
    display.setTextSize(1);
    display.setCursor(1, 0);
    display.print("TOTAL");
    display.setCursor(0, 12);
    display.setTextSize(2);
    if((millis()/60000)/60 < 10) display.print(0);
    display.print((millis()/60000)/60);
    display.print(":");
    if((millis()/60000)%60 < 10) display.print(0);
    display.print((millis()/60000)%60);

    //display active time
    display.setTextSize(1);
    display.setCursor(69, 0);
    display.print("ACTIVE");
    display.setCursor(68, 12);
    display.setTextSize(2);
    if((activeTime/60000)/60 < 10) display.print(0);
    display.print((activeTime/60000)/60);
    display.print(":");
    if((activeTime/60000)%60 < 10) display.print(0);
    display.print((activeTime/60000)%60);

    //display max speed
    display.setTextSize(1);
    display.setCursor(1, 35);
    display.print("MAX");
    display.setCursor(30, 52);
    display.print("Km/h");
    display.setTextSize(2);
    display.setCursor(0, 46);    
    if(maxSpeed < 10) display.print(0);
    display.print(maxSpeed, 0);

    //display average speed
    display.setTextSize(1);
    display.setCursor(69, 35);
    display.print("AVERAGE");
    display.setCursor(98, 52);
    display.print("Km/h");
    display.setTextSize(2);    
    display.setCursor(68, 46);
    if(totalKm == 0.0 || activeTime == 0) {
      display.print(0); display.print(0);
    }
    else {
      if(totalKm/(activeTime/3600000.0) < 10) display.print(0);
      display.print(totalKm/(activeTime/3600000.0), 0);  
    }
    

    display.display();
  }

  if(displayPage == 2) {
    display.setTextSize(2);
    
    if(menuSelection == 0) {
      display.setCursor(0, 2);
      display.print(">");
    }

    if(menuSelection == 1) {
      display.setCursor(0, 24);
      display.print(">");
    }

    if(menuSelection == 2) {
      display.setCursor(0, 46);
      display.print(">");
    }
    
    display.setCursor(16, 2);
    display.print("All");
    display.setCursor(16, 24);
    display.print("Front");
    display.setCursor(16, 46);
    display.print("Back");
    
    if(frontLights == true || backLights == true) {
      display.fillRect(87, 0, 39, 18, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(95, 2);
      display.print("ON");
    }

    if(frontLights == false && backLights == false) {
      display.setTextColor(WHITE);
      display.setCursor(89, 2);
      display.print("OFF");
    }

    if(frontLights == true) {
      display.fillRect(87, 22, 39, 18, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(95, 24);
      display.print("ON");
    }

    if(frontLights == false) {
      display.setTextColor(WHITE);
      display.setCursor(89, 24);
      display.print("OFF");
    }

    if(backLights == true) {
      display.fillRect(87, 44, 39, 18, WHITE);
      display.setTextColor(BLACK);
      display.setCursor(95, 46);
      display.print("ON");
    }

    if(backLights == false) {
      display.setTextColor(WHITE);
      display.setCursor(89, 46);
      display.print("OFF");
    }
    
    display.setTextColor(WHITE);
    display.display();
  }

  //attachInterrupt(digitalPinToInterrupt(wheelSensor), calculateSpeed, RISING);
}

void checkBattery() {
  int batValue = analogRead(A0);

  //value above maximum threshold
  if ((605 + batteryReadingDeadband) < batValue) {  // more than 3.9V (605)
    batteryState = 3;
  }

  //value between two thresholds (middle region)
  if (((559 + batteryReadingDeadband) < batValue) && (batValue <= (605 - batteryReadingDeadband))) {  // 3.6V (559) to 3.9V (605)
    batteryState = 2;
  }

  //value between two thresholds (middle region)
  if (((513 + batteryReadingDeadband) < batValue) && (batValue <= (559 - batteryReadingDeadband))) {  // 3.3V (513) to 3.6V (559)       
    batteryState = 1;
  }

  //value below minimum threshold
  if (batValue < (513 - batteryReadingDeadband)) {  // less than 3.3V (513)
    batteryState = 0;    
  }

  display.drawLine(112, 55, 126, 55, WHITE);
  display.drawLine(112, 63, 126, 63, WHITE);
  display.drawLine(112, 56, 112, 62, WHITE);
  display.drawLine(126, 56, 126, 57, WHITE);
  display.drawLine(126, 61, 126, 62, WHITE);
  display.drawLine(127, 57, 127, 61, WHITE);
  
  if(batteryState == 0) {  // less than 3.3V
    display.fillRect(114, 57, 3, 5, BLACK);
    display.fillRect(118, 57, 3, 5, BLACK);
    display.fillRect(122, 57, 3, 5, BLACK);
  } 
  if(batteryState == 1) { // 3.3V to 3.6V
    display.fillRect(114, 57, 3, 5, WHITE);
    display.fillRect(118, 57, 3, 5, BLACK);
    display.fillRect(122, 57, 3, 5, BLACK);
  }
  if(batteryState == 2) { // 3.6V to 3.9V
    display.fillRect(114, 57, 3, 5, WHITE);
    display.fillRect(118, 57, 3, 5, WHITE);
    display.fillRect(122, 57, 3, 5, BLACK);
  }
  if(batteryState == 3) { // more than 3.9V
    display.fillRect(114, 57, 3, 5, WHITE);
    display.fillRect(118, 57, 3, 5, WHITE);
    display.fillRect(122, 57, 3, 5, WHITE);
  }

  //Serial.println(batValue);
}

void settingsConfirmation() {
  //display.fillTriangle(58, 40, 48, 30, 127, 0, WHITE);
  display.clearDisplay();
  display.drawRect(11, 5, 107, 53, WHITE);
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(23, 14);
  display.print("Changes");
  display.setCursor(31, 35);
  display.print("saved!");
  display.display();
}