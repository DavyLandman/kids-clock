#include "display.hpp"
#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
//#include "rabbit.h"
#include "cat-paw.h"
#include "cat-watch-face.h"
#include "NotoGiraffe64-flatten.h"
#include "NotoFrog64-flatten.h"
#include "NotoFox64-flatten.h"

#define HEX_TO_565(c) (((c & 0xf80000) >> 8) + ((c & 0xfc00) >> 5) + ((c & 0xf8) >> 3))

constexpr uint32_t WIDTH = 160;
constexpr uint32_t HEIGHT = 128;
//constexpr uint32_t CLOCK_CENTER_X = 44;
//constexpr uint32_t CLOCK_CENTER_Y = CLOCK_CENTER_X;
constexpr uint32_t CLOCK_RADIUS = 46;

constexpr uint16_t CLOCK_COLOR_FACE = HEX_TO_565(0x004488);

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
void Display::plotPixel(int16_t x, int16_t y, float alpha, uint16_t color) {
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
void Display::drawWideLineAA(float ax, float ay, float bx, float by, float r, uint16_t color) {
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

Display::Display() {
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BLACK);
  renderEdges();
  lcd.loadFont(NotoSansBold15);
  face.loadFont(NotoSansBold15);
  face.createSprite(CLOCK_RADIUS * 2, CLOCK_RADIUS * 2);
  pinMode(D1, OUTPUT);
  setBrightness(200);
}

void Display::setBrightness(uint8_t brightness) {
  analogWrite(D1, brightness);
}

void Display::render(uint8_t hour, uint8_t minute, State state, float progress) {
    showTime(hour, minute);
    if (state != currentState) {
        currentState = state;
        updateStatus(state);
    }
    if (state != Awake) {
        updateProgress(progress);
    }
}

void Display::renderEdges() {
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

uint16_t Display::lookupColor(uint16_t x, uint16_t y) {
    //Serial.printf("Lookup: %d , %d: %x\n", x, y, face.readPixel(x, y));
    //return TFT_RED;
    return face.readPixel(x, y);
}

void Display::drawNeedle(float angle, uint16_t length) {
    double x = CLOCK_RADIUS + (length * cos(toRad(angle))); 
    double y = CLOCK_RADIUS + (length * sin(toRad(angle)));
    drawWideLineAA(CLOCK_RADIUS, CLOCK_RADIUS, x, y, 2., CLOCK_COLOR_FACE);
}

void Display::renderFace(float hourAngle, float minuteAngle) {
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


void Display::updateStatus(State newState) {
    switch(newState) {
        case Sleeping: updateStatus(NotoFox64, "Slapen", TFT_RED); return;
        case WakingUp: updateStatus(NotoGiraffe64, "Rustig", TFT_YELLOW); return;
        case Awake: updateStatus(NotoFrog64, "Wakker", TFT_DARKGREEN); return;
    }
}
void Display::updateStatus(const uint16_t* img, const String &txt, uint16_t color) {
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

void Display::progressLine(uint32_t line_y, float hide_line) {
    lcd.fillRect(STATUS_BOX_X + 7, line_y, round(AREA_AROUND_CAT * 3 * min(hide_line, 1.0)), AREA_AROUND_CAT, TFT_BLACK);
}
void Display::updateProgress(float progress) {
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


void Display::showTime(uint8_t hour, uint8_t minute) {
  if (hour >= 12) {
    hour -= 12;
  }
  float hourPosition = (360 / 12.0) * (hour + (minute / 60.0));

  renderFace(fixPosition(hourPosition), fixPosition((360 / 60.0) * minute));
}