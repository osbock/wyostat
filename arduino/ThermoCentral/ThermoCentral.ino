/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 by Daniel Eichhorn
 * Copyright (c) 2016 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "config.h"
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "RTClib.h"
#include <EEPROM.h>
#include "SparkFunTMP102.h"
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`

#define INCR 15
#define DECR 12
#define MODE 14
#define ENTER 13
#define CH1 26
#define CH2 25
#define CH3 16

RTC_DS3231 rtc;

// set up feeds
AdafruitIO_Feed *livingroomtemp = io.feed("LivingRoomTemp");
AdafruitIO_Feed *bedroomtemp = io.feed("BedroomTemp");
AdafruitIO_Feed *setTemp = io.feed("setTemp");
//AdafruitIO_Feed *furnace = io.feed("furnaceOnOff");

int living = 0;
int bedroom =0;
int settemperature = 55;
 // is the furnace on or off
int furnacestate = 0;
// temp gap for hysteresis control
float  tempgap = 2.0L;
 //time
 DateTime now;
 void handleButtons();
#define furnaceSCR CH1
 
// Include the UI lib
#include "OLEDDisplayUi.h"

// Include custom images
#include "images.h"

// Use the corresponding display class:


// Initialize the OLED display using Wire library
SSD1306  display(0x3c, 5, 4);
TMP102 sensor0(0x48);

OLEDDisplayUi ui     ( &display );
String livingroomtitle = String("LivingRoom");
String bedroomtitle = String("Time");
String currentRoom = bedroomtitle;
void roomOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->setFont(ArialMT_Plain_10);
  display->drawString(128, 0, currentRoom);
}

void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  int minute = now.minute();
  int hour = now.hour();
  String ampmstr;
  String minutestr;
  if (hour >12){
    hour = hour - 12;
    ampmstr = " PM";
  }else
    ampmstr = " AM";
  if (minute < 10){
    minutestr = String("0")+String(minute);
  }else
  minutestr = String(minute);
  String timestring = String(hour) + String(":")+minutestr+ampmstr;
  String tempstring = String(living) + String("° F");
  currentRoom = bedroomtitle;
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(60 + x, 25 + y, timestring);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0+x,10+y,tempstring);
}

void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  String livingroomstring = String(living) + String("° F");
  String setstring = String(settemperature) + String("° F");
  currentRoom = livingroomtitle;
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(80 + x, 25 + y, livingroomstring);
  display->setFont(ArialMT_Plain_16);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0+x,10+y,setstring);
}



void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {

}

// This array keeps function pointers to all frames
// frames are the single views that slide in
FrameCallback frames[] = { drawFrame1, drawFrame2 };

// how many frames are there?
int frameCount = 2;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { roomOverlay};
int overlaysCount = 1;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  pinMode(furnaceSCR,OUTPUT);
  digitalWrite(furnaceSCR,LOW);
  sensor0.begin();
  // set the Conversion Rate (how quickly the sensor gets a new reading)
  //0-3: 0:0.25Hz, 1:1Hz, 2:4Hz, 3:8Hz
  sensor0.setConversionRate(2);
  //set Extended Mode.
  //0:12-bit Temperature(-55C to +128C) 1:13-bit Temperature(-55C to +150C)
  sensor0.setExtendedMode(0);
	// The ESP is capable of rendering 60fps in 80Mhz mode
	// but that won't give you much time for anything else
	// run it in 160Mhz mode or just set it to 30 fps
  ui.setTargetFPS(30);
  ui.setTimePerFrame(20000);
  ui.transitionToFrame(0);

	// Customize the active and inactive symbol
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);

  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_UP);

  // Add frames
  ui.setFrames(frames, frameCount);

  // Add overlays
  ui.setOverlays(overlays, overlaysCount);

  // Initialising the UI will init the display too.
  ui.init();

  display.flipScreenVertically();
  // connect to adafruit.io
  io.connect();
  bedroomtemp->onMessage(handleBedroom);
  livingroomtemp->onMessage(handleLivingroom);
  setTemp->onMessage(handleSetTemp);
  //furnace->onMessage(handleFurnace);
    // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  EEPROM.begin(512);
  loadSettings();
   if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}
const uint8_t DEFAULT_SET_TEMP = 70;
const uint8_t SET_TEMP_ADDR = 0;
void loadSettings()
{
  settemperature = EEPROM.read(SET_TEMP_ADDR);
  if(settemperature == 255){
    settemperature = DEFAULT_SET_TEMP;
  }
    
}
void handleBedroom(AdafruitIO_Data *message){
    char *data = (char*)message->value();
  Serial.print("received <- ");
  Serial.println(data);

  int dataLen = strlen(data);
  Serial.print("Got: ");
  Serial.println(data);
  if (dataLen < 1) {
  // Stop processing if not enough data was received.
     return;
  }
  String dataStr = String(data);
  bedroom = (int)dataStr.toInt();
    Serial.print(message->feedName());
  Serial.print(" ");

  // print out the received count or counter-two value
  Serial.println(message->value());
}
void handleLivingroom(AdafruitIO_Data *message){
    char *data = (char*)message->value();
  Serial.print("received <- ");
  Serial.println(data);

  int dataLen = strlen(data);
  Serial.print("Got: ");
  Serial.println(data);
  if (dataLen < 1) {
  // Stop processing if not enough data was received.
     return;
  }
  String dataStr = String(data);
  living = (int)dataStr.toInt();
    Serial.print(message->feedName());
  Serial.print(" ");

  // print out the received count or counter-two value
  Serial.println(message->value());
}
void handleFurnace(AdafruitIO_Data *message){
  char *data = (char*)message->value();
  Serial.print("received <- ");
  Serial.println(data);

  int dataLen = strlen(data);
  Serial.print("Got: ");
  Serial.println(data);
  if (dataLen < 1) {
  // Stop processing if not enough data was received.
     return;
  }
  String dataStr = String(data);
  if (dataStr.equals("OFF"))
    digitalWrite(furnaceSCR, LOW);
  if (dataStr.equals("ON"))
    digitalWrite(furnaceSCR,HIGH);
    Serial.print(message->feedName());
  Serial.print(" ");

  // print out the received count or counter-two value
  Serial.println(message->value());
}
void handleSetTemp(AdafruitIO_Data *message){
    char *data = (char*)message->value();
  Serial.print("received <- ");
  Serial.println(data);

  int dataLen = strlen(data);
  Serial.print("Got: ");
  Serial.println(data);
  if (dataLen < 1) {
  // Stop processing if not enough data was received.
     return;
  }
  String dataStr = String(data);
  int new_settemperature = (int)dataStr.toInt();
  if(new_settemperature != settemperature){
    EEPROM.write(SET_TEMP_ADDR, new_settemperature);
    EEPROM.commit();
    settemperature = new_settemperature;
    Serial.print("EEPROM SET TEMP: ");
    Serial.println(EEPROM.read(SET_TEMP_ADDR));
  }

  Serial.print(message->feedName());
  Serial.print(" ");

  // print out the received count or counter-two value
  Serial.println(message->value());
}
// 1 sec temperature update
long lastUpdate;
#define TEMPINTERVAL 1000
#define RECORDUPDATE 120000
long lastrecord;
float localtemperature;
void loop() {
  int remainingTimeBudget = ui.update();
  io.run();
  if (millis()-lastUpdate> TEMPINTERVAL)
  {
    now = rtc.now();
    sensor0.wakeup();
    localtemperature = sensor0.readTempF();
    sensor0.sleep();
    living = (int)localtemperature;
    //Adafruit IO stores a limited amount of data, if we record
    // every second, it wraps too quickly
    if (millis()-lastrecord > RECORDUPDATE){
      //livingroomtemp->save(localtemperature);
      lastrecord = millis();
      Serial.println("updated localtemp");
    }
    lastUpdate = millis();
  }
  //heater control
  if (!furnacestate) // furnace is off
  {
    if (localtemperature < (settemperature-tempgap)){
      furnacestate = 1;
      Serial.println("Turning on furnace");
      digitalWrite(furnaceSCR,HIGH);  
    }
  }
  else //furnace is on
  {
    if (localtemperature >= settemperature)
    {
      furnacestate =0;
      Serial.println("Turning off furnace");
      digitalWrite(furnaceSCR,LOW);
    }
  }
  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    handleButtons();
    delay(remainingTimeBudget);
  }

}
  int lastInc,lastDec,lastMode,lastENT;
  int screen = 0;
  void handleButtons(){
    int modeb = digitalRead(MODE);
    if (modeb != lastMode){
      lastMode = modeb;
      if (modeb == LOW){
        screen = screen++ > 1?0:screen;
        ui.switchToFrame(screen);
      }
    }
    int incb = digitalRead(INCR);
    if (incb != lastInc){
      lastInc = incb;
      if (incb == LOW){
        settemperature++;
      }
    }
   int decb = digitalRead(DECR);
    if (decb != lastDec){
      lastDec = decb;
      if (decb == LOW){
        settemperature--;
      }
    }
  }
  
