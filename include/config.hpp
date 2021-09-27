#ifndef __CONFIG_H
#define __CONFIG_H
#include <Arduino.h>
#include <ESP8266WebServer.h>
class Config{
public:
    Config();
    void handle();
    uint16_t getSleepTime();
    uint16_t getAwakeTime();
    uint16_t getAwakeTransitionSeconds();
};
#endif