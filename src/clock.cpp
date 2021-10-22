#ifndef MAIN
#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

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



#elif defined(XX)

#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>

#include "NotoSansBold15.h"

TFT_eSPI lcd = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite face = TFT_eSprite(&lcd);
TFT_eSprite hourNeedle = TFT_eSprite(&lcd);
TFT_eSprite minuteNeedle = TFT_eSprite(&lcd);

constexpr uint32_t CLOCK_RADIUS = 128 / 2;
constexpr uint16_t CLOCK_COLOR_FACE = TFT_RED;
constexpr uint16_t CLOCK_COLOR_BG = TFT_WHITE;

constexpr uint32_t HOUR_ANGLE = 360 / 12;
constexpr uint32_t MINUTE_ANGLE = 360 / 60;

static double toRad(uint32_t angle) {
    return DEG_TO_RAD * angle;
}

static void renderFace() {
  face.createSprite(CLOCK_RADIUS * 2, CLOCK_RADIUS * 2);
  face.fillCircle(CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_COLOR_BG);
  face.fillCircle(CLOCK_RADIUS, CLOCK_RADIUS, 3, CLOCK_COLOR_FACE);
  face.setTextDatum(MC_DATUM);
  face.loadFont(NotoSansBold15);
  face.setTextColor(CLOCK_COLOR_FACE); 
  constexpr uint32_t dialOffset = CLOCK_RADIUS - 10;
  for (uint32_t h = 0; h < 12; h++) {
      double x = CLOCK_RADIUS + (dialOffset * cos(toRad(h * HOUR_ANGLE))); 
      double y = CLOCK_RADIUS + (dialOffset * sin(toRad(h * HOUR_ANGLE)));

      uint32_t actualHour = (h + 3) % 12;
      face.drawNumber(actualHour == 0 ? 12 : actualHour, round(x), round(y));
  }
  face.unloadFont();
}

static void renderNeedles() {
  hourNeedle.createSprite(CLOCK_RADIUS / 2, 3);
  hourNeedle.fillSprite(CLOCK_COLOR_FACE);
  hourNeedle.fillRect(0,0, 3, 3, TFT_TRANSPARENT);
  hourNeedle.setPivot(0, 1);
  minuteNeedle.createSprite(CLOCK_RADIUS - 20, 3);
  minuteNeedle.fillSprite(CLOCK_COLOR_FACE);
  minuteNeedle.fillRect(0,0, 3, 3, TFT_TRANSPARENT);
  minuteNeedle.setPivot(0, 1);
}

void setup() {
  Serial.begin(74880); // native to debug output of bootloader
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  renderFace();
  renderNeedles();
  lcd.setPivot(CLOCK_RADIUS, CLOCK_RADIUS);
}


static uint16_t hours = 0;
static uint16_t minutes = 0;
void loop() {
    face.pushSprite(0,0, TFT_TRANSPARENT);
    hourNeedle.pushRotated(hours, TFT_TRANSPARENT);
    minuteNeedle.pushRotated(minutes, TFT_TRANSPARENT);
    hours = hours >= 360 ? HOUR_ANGLE / 10 : hours + (HOUR_ANGLE / 10);
    minutes = minutes >= 360 ? MINUTE_ANGLE : minutes + MINUTE_ANGLE;
    delay(500);
}
#else 

#include <Arduino.h>
#include <TFT_eSPI.h> 
#include <SPI.h>


#define ALPHA_GAIN 1.0f  // Should be 1.0 but 1.3 looks good on my TFT
                         // Less than 1.0 makes hands look transparent

#include "NotoSansBold15.h"

TFT_eSPI lcd = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite face         = TFT_eSprite(&lcd);
TFT_eSprite hourNeedle   = TFT_eSprite(&lcd);
TFT_eSprite minuteNeedle = TFT_eSprite(&lcd);


constexpr uint32_t CLOCK_RADIUS = 128 / 2;
constexpr uint16_t CLOCK_COLOR_FACE = TFT_RED;
constexpr uint16_t CLOCK_COLOR_BG = TFT_WHITE;

uint16_t fg = CLOCK_COLOR_BG;


#include "AA_functions.hpp"

constexpr float NUMBER_ANGLE = 360.0 / 12.0;

// Calculate 1 second increment angle and multiply by 10.0
// at normal time rate hands angle changes every 10 seconds so we
// see smooth sub-pixel AA movement of hands, can reduce angle to 1 second
// or less later when clock runs at real time speed.
constexpr float HOUR_ANGLE   = (360.0 / 12.0 / 60.0 / 60.0) * 10.0;
constexpr float MINUTE_ANGLE = (360.0 / 60.0 / 60.0)        * 10.0;

// Add one so sprite has a central pixel
#define W CLOCK_RADIUS * 2 + 1
#define H CLOCK_RADIUS * 2 + 1


static float h_angle = 0;
static float m_angle = 0;

static double toRad(uint32_t angle) {
    return DEG_TO_RAD * angle;
}

// =========================================================================
// Get coordinates of end of a vector, pivot at x,y, length r, angle a
// =========================================================================
// Coordinates are returned to caller via the xp and yp pointers
#define DEG2RAD 0.0174532925
void getCoord(int x, int y, float *xp, float *yp, int r, float a)
{
  float sx1 = cos( -a * DEG2RAD );
  float sy1 = sin( -a * DEG2RAD );
  *xp =  sx1 * r + x;
  *yp =  sy1 * r + y;
}


static void renderFace() {
  face.fillSprite(TFT_BLACK);

  fillCircleAA(CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_RADIUS, CLOCK_COLOR_BG);
  fillCircleAA(CLOCK_RADIUS, CLOCK_RADIUS, 4, CLOCK_COLOR_FACE);

  face.setTextDatum(MC_DATUM);
  face.setTextColor(CLOCK_COLOR_FACE);

  constexpr uint32_t dialOffset = CLOCK_RADIUS - 10;
  for (uint32_t h = 0; h < 12; h++) {
      float x = CLOCK_RADIUS + (dialOffset * cos(toRad(h * NUMBER_ANGLE))); 
      float y = CLOCK_RADIUS + (dialOffset * sin(toRad(h * NUMBER_ANGLE)));

      uint32_t actualHour = (h + 3) % 12;
      face.drawNumber(actualHour == 0 ? 12 : actualHour, ceilf(x), ceilf(2+y));
  }

  face.setTextColor(TFT_MAROON);
  face.drawString("TFT_eSPI", CLOCK_RADIUS, CLOCK_RADIUS * 1.3);

  float xp, yp; // Use float pixel position for smooth AA motion

  getCoord(CLOCK_RADIUS, CLOCK_RADIUS, &xp, &yp, CLOCK_RADIUS - 20.0, 90.0 - m_angle);
  drawWideLineAA(CLOCK_RADIUS, CLOCK_RADIUS, xp, yp, 2.2, CLOCK_COLOR_FACE);

  getCoord(CLOCK_RADIUS, CLOCK_RADIUS, &xp, &yp, CLOCK_RADIUS - 30.0, 90.0 - h_angle);
  drawWideLineAA(CLOCK_RADIUS, CLOCK_RADIUS, xp, yp, 2.2, CLOCK_COLOR_FACE);

  face.pushSprite(0,0, TFT_TRANSPARENT);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting...");

  lcd.init();
  lcd.fillScreen(TFT_BLACK);
  lcd.setRotation(1); // Orientation for good viewing angle range on my TFT

  face.createSprite(W, H);
  face.loadFont(NotoSansBold15); // Can remain loaded
}

void loop() {
    // All graphics are drawn in sprite to stop flicker
    renderFace();

    h_angle >= 360.0 ? h_angle = 0.0 : h_angle += (HOUR_ANGLE  );
    m_angle >= 360.0 ? m_angle = 0.0 : m_angle += (MINUTE_ANGLE);

    //delay(10000);  // Real time would move every 10 seconds...
}

#endif