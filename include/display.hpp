#ifndef __DISPLAY_H
#define __DISPLAY_H
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>

enum State: int {
    Invalid,
    Awake,
    Sleeping,
    WakingUp
};

class Display {
private:
    TFT_eSPI lcd = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
    TFT_eSprite face = TFT_eSprite(&lcd);
    State currentState = Invalid;
    void plotPixel(int16_t x, int16_t y, float alpha, uint16_t color);
    void drawWideLineAA(float ax, float ay, float bx, float by, float r, uint16_t color);
    uint16_t lookupColor(uint16_t x, uint16_t y);
    void drawNeedle(float angle, uint16_t length);
    void renderFace(float hourAngle, float minuteAngle);
    void renderEdges();
    void updateStatus(State newState);
    void updateStatus(const uint16_t* img, const String &txt, uint16_t color);
    void progressLine(uint32_t line_y, float hide_line);
    void updateProgress(float progress);
    void showTime(uint8_t hour, uint8_t minute);
public:
    Display();
    void render(uint8_t hour, uint8_t minute, State state, float progress);
    void setBrightness(uint8_t brightness); // 0 = background off, 255= full
};

#endif