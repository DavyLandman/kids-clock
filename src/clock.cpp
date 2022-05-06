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
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("KidsClock");
  WiFi.begin(WIFI_ACCESPOINT, WIFI_PASSWORD);
  currentTime = new Time("Europe/Amsterdam");
  config = new Config();
  display = new Display();
}

static State currentState = Awake;
static float progress = 0;

static void updateState() {
  const auto time = ((uint16_t)currentTime->hour() * 60) + currentTime->minute();
  if (time > config->getSleepTime()) {
    display->setBrightness(80);
    currentState = Sleeping;
    progress = 0;
  }
  else if (time < config->getAwakeTime()) {
    // now we switch to second precision for smoother progress reports
    auto toWait = ((uint32_t)config->getAwakeTime() * 60) - (((uint32_t)time * 60) + currentTime->second());
    auto transition = (float)(config->getAwakeTransition() * 60);
    if (toWait < transition) {
      display->setBrightness(160);
      currentState = WakingUp;
      progress = 1.0 - (toWait / transition);
    }
    else if (toWait < (2 * transition)) {
      display->setBrightness(120);
      // we start with decreasing sleep counter
      // the same time as we do the awake counter
      toWait -= transition;
      currentState = Sleeping;
      progress = 1.0 - (toWait / transition);
    }
    else {
      currentState = Sleeping;
      progress = 0;
      display->setBrightness(80);
    }
  }
  else {
    // we must be awake!
    currentState = Awake;
    progress = 1;
    if (time - config->getAwakeTime() < 30 || config->getSleepTime() - time < 10) {
      display->setBrightness(200);
    }
    else {
      display->setBrightness(80);
    }
  }
}

void loop() {
  config->handle();
  if (currentTime->process()) {
    updateState();
    display->render(currentTime->hour(), currentTime->minute(), currentState, progress);
  }
  delay(200);
}