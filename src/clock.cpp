#ifndef MAIN
#include <Arduino.h>
#include <ezTime.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
//#include "rabbit.h"
#include "cat-paw.h"
#include "cat-watch-face.h"
#include "NotoGiraffe64-flatten.h"
#include "NotoFrog64-flatten.h"
#include "NotoFox64-flatten.h"
#include "secrets.h"


TFT_eSPI lcd = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
TFT_eSprite face = TFT_eSprite(&lcd);

#define HEX_TO_565(c) (((c & 0xf80000) >> 8) + ((c & 0xfc00) >> 5) + ((c & 0xf8) >> 3))

constexpr uint32_t WIDTH = 160;
constexpr uint32_t HEIGHT = 128;
//constexpr uint32_t CLOCK_CENTER_X = 44;
//constexpr uint32_t CLOCK_CENTER_Y = CLOCK_CENTER_X;
constexpr uint32_t CLOCK_RADIUS = 46;

constexpr uint16_t CLOCK_COLOR_FACE = HEX_TO_565(0x004488);

//static void renderFace();
static void renderEdges();
//static void renderNeedles();

#define ALPHA_GAIN 1.3f  // Should be 1.0 but 1.3 looks good on my TFT
                         // Less than 1.0 makes hands look transparent

// Support
#define ipart(X) ((int16_t)(X))
#define iround(X) ((uint16_t)(((float)(X))+0.5))
#define fpart(X) (((float)(X))-(float)ipart(X))
#define rfpart(X) (1.0-fpart(X))

// Calculate distance of px,py to closest part of line ax,ay .... bx,by
// ESP32 has FPU so floats are quite quick
float lineDistance(float px, float py, float ax, float ay, float bx, float by) {
  float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
  float h = fmaxf(fminf((pax * bax + pay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
  float dx = pax - bax * h, dy = pay - bay * h;
  return sqrtf(dx * dx + dy * dy);
}

// Alphablend specified color with image background pixel and plot at x,y
void plotPixel(int16_t x, int16_t y, float alpha, uint16_t color) {
  // Adjust alpha, 1.3 gain is good for my display
  alpha = alpha * ALPHA_GAIN * 255; // 1.3 alpha gain is good for my display gamma curve
  // Clip alpha
  if (alpha > 255) alpha = 255;
  //Blend color with background and plot
  face.drawPixel(x, y, face.alphaBlend((uint8_t)alpha, color, face.readPixel(x, y)));
}

// Anti aliased line ax,ay to bx,by, (2 * r) wide with rounded ends
// Note: not optimised to minimise sampling zone
// Using floats for line coordinates allows for sub-pixel positioning
void drawWideLineAA(float ax, float ay, float bx, float by, float r, uint16_t color) {
  int16_t x0 = (int16_t)floorf(fminf(ax, bx) - r);
  int16_t x1 = (int16_t) ceilf(fmaxf(ax, bx) + r);
  int16_t y0 = (int16_t)floorf(fminf(ay, by) - r);
  int16_t y1 = (int16_t) ceilf(fmaxf(ay, by) + r);

  for (int y = y0; y <= y1; y++) {
    for (int x = x0; x <= x1; x++) {
      plotPixel(x, y, fmaxf(fminf(0.5f - (lineDistance(x, y, ax, ay, bx, by) - r), 1.0f), 0.0f), color);
    }
  }
}

Timezone *amsterdam;

ESP8266WebServer config(80);

static void renderConfigPage();
static void handleConfigChange();


void setup() {
  Serial.begin(74880); // native to debug output of bootloader
  Serial.println("Starting clock");
  WiFi.hostname("ESP-Clock");
  WiFi.begin(WIFI_ACCESPOINT, WIFI_PASSWORD);
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  renderEdges();
  lcd.loadFont(NotoSansBold15);
  face.loadFont(NotoSansBold15);
  face.createSprite(CLOCK_RADIUS * 2, CLOCK_RADIUS * 2);
  pinMode(D1, OUTPUT);
  analogWrite(D1, 200);
  //waitForSync();

  //Serial.println("UTC: " + UTC.dateTime());
  //Timezone ams;
  //setInterval(10);
  setDebug(INFO);
  //Serial.println("Amsterdam: " + ams.dateTime());
  config.on("/", HTTP_GET, renderConfigPage);
  config.on("/set", HTTP_POST, handleConfigChange);
  config.onNotFound([] () { config.send(404, "text/plain", "404 Not Found");});
  config.begin();
}

static uint16_t sleepTime = 19 * 60;
static uint16_t awakeTime = 7 * 60;
static uint16_t awakeTransition = 5;

static void renderConfigPage() {
  char buffer[2048];
  int generated = sprintf(buffer, "<!DOCTYPE html>"
    "<html lang=\"nl\"><head><title>Clock</title>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/normalize/8.0.1/normalize.css\">"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.14.0/css/all.css\">"
    "<style>"
      "@import url('https://fonts.googleapis.com/css2?family=Noto+Sans&display=swap');"
      "body{ font-family: 'Noto Sans', sans-serif; margin: 1em; line-height: 1.2; }"
      ".entry{ display: block; margin: 0.5em; }"
    "</style></head>"
    "<body><h1>Configure Clock</h1>"
    "<form action=\"/set\" method=\"POST\">"
    "<span class=\"entry\"><label for=\"sleep\">Sleep</label><input id=\"sleep\" name=\"sleep\" type=\"time\" value=\"%02d:%02d\"/></span>"
    "<span class=\"entry\"><label for=\"awakeTransition\">Awake transition</label><input id=\"awakeTransition\" name=\"awakeTransition\" type=\"number\" style=\"width:3em\" value=\"%d\"/> minutes</span>"
    "<span class=\"entry\"><label for=\"awake\">Awake</label><input id=\"awake\" name=\"awake\" type=\"time\" value=\"%02d:%02d\"/></span>"
    "<input type=\"submit\" value=\"Change\" style=\"display:block\">"
    "</form>"
    "</body></html>"
  , sleepTime / 60, sleepTime % 60, awakeTransition, awakeTime / 60, awakeTime % 60);
  config.send(200, "text/html", buffer, generated);
}

static long parseTime(const String & arg) {
  Serial.println(arg);
  return (arg.substring(0, 2).toInt() * 60) + arg.substring(3).toInt();
}

static void handleConfigChange() {
  //Serial.println(config.header());
  sleepTime = parseTime(config.arg("sleep"));
  awakeTime = parseTime(config.arg("awake"));
  awakeTransition = config.arg("awakeTransition").toInt();
  config.sendHeader("Location", String("/"), true);
  config.send(302, "text/plain", "");
}


void renderEdges() {
    lcd.loadFont(NotoSansBold36);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.setCursor(0, (CLOCK_RADIUS * 2) + 2);
    lcd.setTextDatum(BL_DATUM);
    lcd.println("Tom");
    lcd.unloadFont();
}

constexpr uint32_t HOUR_ANGLE = 360 / 12;
constexpr uint32_t MINUTE_ANGLE = 360 / 60;

static double toRad(uint32_t angle) {
    return DEG_TO_RAD * angle;
}

uint16_t lookupColor(uint16_t x, uint16_t y) {
    //Serial.printf("Lookup: %d , %d: %x\n", x, y, face.readPixel(x, y));
    //return TFT_RED;
    return face.readPixel(x, y);
}

static void drawNeedle(float angle, uint16_t length) {
    double x = CLOCK_RADIUS + (length * cos(toRad(angle))); 
    double y = CLOCK_RADIUS + (length * sin(toRad(angle)));
    drawWideLineAA(CLOCK_RADIUS, CLOCK_RADIUS, x, y, 2., CLOCK_COLOR_FACE);
}

static void renderFace(float hourAngle, float minuteAngle) {
  face.setSwapBytes(true);
  face.pushImage(0,0, 92, 92, CAT_WATCH_FACE);
  face.setSwapBytes(false);


  //face.fillCircle(CLOCK_RADIUS, CLOCK_RADIUS, 3, CLOCK_COLOR_FACE);
  face.setTextDatum(MC_DATUM);
  face.setTextColor(CLOCK_COLOR_FACE); 
  constexpr uint32_t dialOffset = CLOCK_RADIUS - 7;
  for (uint32_t h = 0; h < 12; h++) {
      double x = CLOCK_RADIUS + (dialOffset * cos(toRad(h * HOUR_ANGLE))); 
      double y = CLOCK_RADIUS + (dialOffset * sin(toRad(h * HOUR_ANGLE)));

      uint32_t actualHour = (h + 3) % 12;
      face.drawNumber(actualHour == 0 ? 12 : actualHour, round(x), round(y));
  }
  //face.unloadFont();
  drawNeedle(hourAngle, CLOCK_RADIUS / 3);
  drawNeedle(minuteAngle, CLOCK_RADIUS - 16);
  face.pushSprite(0,0, TFT_TRANSPARENT);
}









constexpr uint32_t STATUS_BOX_WIDTH = 64;
constexpr uint32_t STATUS_BOX_HEIGHT = 64;
constexpr uint32_t STATUS_BOX_X = WIDTH - STATUS_BOX_WIDTH;

constexpr uint32_t AREA_AROUND_CAT = 18;


void updateStatus(const uint16_t* img, const String &txt, uint16_t color) {
    lcd.fillRect(STATUS_BOX_X, 0, STATUS_BOX_WIDTH, STATUS_BOX_HEIGHT, TFT_BLACK);
    lcd.setTextColor(color, TFT_BLACK);
    lcd.drawCentreString(txt, STATUS_BOX_X + (STATUS_BOX_WIDTH /  2), 0, 1);
    lcd.setSwapBytes(true);
    if (img != NotoFrog64) {
        for (uint32_t x = 0; x < 3; x++) {
            for (uint32_t y = 0; y < 3; y++) {
                lcd.pushImage(STATUS_BOX_X + 7 + (x * 18), 15 + y * 18, 16, 16, cat_paw);
            }
        }
    }
    lcd.pushImage(WIDTH - 64, HEIGHT-64, 64, 64, img);
    lcd.setSwapBytes(false);
}

static float min(float a, float b) {
    return a > b ? b : a;
}

static void progressLine(uint32_t line_y, float hide_line) {
    lcd.fillRect(STATUS_BOX_X + 7, line_y, round(AREA_AROUND_CAT * 3 * min(hide_line, 1.0)), AREA_AROUND_CAT, TFT_BLACK);
}
static void updateProgress(float progress) {
    uint32_t line_y = 15;
    for (int i = 0; i < 3; i++) {
        progressLine(line_y, progress / 0.33);
        line_y += AREA_AROUND_CAT;
        progress -= 0.33;
    }
}


static float fixPosition(float x) {
  if (x <= 90) {
    return x + 270;
  }
  return x - 90;
}

static void showTime(uint8_t hour, uint8_t minute) {
  if (hour >= 12) {
    hour -= 12;
  }
  float hourPosition = (360 / 12.0) * (hour + (minute / 60.0));

  renderFace(fixPosition(hourPosition), fixPosition((360 / 60.0) * minute));
}

static uint16_t angle = 0;
static uint16_t angle2 = 0;
void loop() {
    //renderFace(270, 60);
    /*
    face.pushSprite(0,0, TFT_TRANSPARENT);
    hourNeedle.pushRotated(angle, TFT_TRANSPARENT);
    minuteNeedle.pushRotated(angle2, TFT_TRANSPARENT);
    */
    angle = angle >= 360 ? HOUR_ANGLE / 10 : angle + (HOUR_ANGLE / 10);
    angle2 = angle2 >= 360 ? MINUTE_ANGLE : angle2 + MINUTE_ANGLE;
    switch (angle2) {
        case 0:
        case 360: updateStatus(NotoFox64, "Slapen", TFT_RED); break;
        case 120: updateStatus(NotoGiraffe64, "Rustig", TFT_YELLOW); break;
        case 240: updateStatus(NotoFrog64, "Wakker", TFT_DARKGREEN); break;
    }
    updateProgress((angle2 % 120) / 120.0);
    
    config.handleClient();
    events();
    if (timeStatus() == timeSet && minuteChanged()) {
      if (amsterdam == nullptr) {
        amsterdam = new Timezone();
        amsterdam->setLocation("Europe/Amsterdam");
        Serial.println("Amsterdam: " + amsterdam->dateTime());
      }
      showTime(amsterdam->hour(), amsterdam->minute());
    }
    delay(100);
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