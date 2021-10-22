#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "secrets.h"
#include "time.hpp"
#include "config.hpp"
#include "display.hpp"

Time* currentTime;
Config* config;
Display* display;

void setup() {
  Serial.begin(74880); // native to debug output of bootloader
  SPIFFS.begin();
  Serial.println("Starting clock");
  WiFi.hostname("KidsClock");
  WiFi.begin(WIFI_ACCESPOINT, WIFI_PASSWORD);
  currentTime = new Time("Europe/Amsterdam");
  config = new Config();
  display = new Display();
}

int counter = 0;
State currentState = Awake;
float progress = 0;

void loop() {
  config->handle();
  if (counter == 25) {
    switch(currentState) {
      case Awake: currentState = Sleeping; break;
      case Sleeping: currentState = WakingUp; break;
      case WakingUp: currentState = Awake; break;
    }
    progress = 0;
    counter = 0;
  }
  else {
    counter++;
    progress += 1.0 / 25;
  }
  if (currentTime->process()) {
    display->render(currentTime->hour(), currentTime->minute(), currentState, progress);
  }
  delay(200);
}