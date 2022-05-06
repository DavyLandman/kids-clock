#include "config.hpp"

static ESP8266WebServer http(80);

static void renderConfigPage();
static void handleConfigChange();
static void writeAlarmConfig();
static void readAlarmConfig();
static void faviconSVG();
static void faviconPNG();

static uint16_t sleepTime = 19 * 60;
static uint16_t awakeTime = 7 * 60;
static uint16_t awakeTransition = 5;

Config::Config() {
  SPIFFS.begin();
  readAlarmConfig();
  http.on("/", HTTP_GET, renderConfigPage);
  http.on("/set", HTTP_POST, handleConfigChange);
  http.on("/favicon.svg", HTTP_GET, faviconSVG);
  http.on("/favicon.png", HTTP_GET, faviconPNG);
  http.onNotFound([] () { http.send(404, "text/plain", "404 Not Found");});
  http.begin();
}

uint16_t Config::getAwakeTime() {
    return awakeTime;
}
uint16_t Config::getSleepTime() {
    return sleepTime;
}

uint16_t Config::getAwakeTransition() {
    return awakeTransition;
}

void Config::handle() {
    http.handleClient();
}

static uint16_t read16(File & f) {
  uint8_t buf[2];
  f.read(buf, 2);
  return buf[0] | (buf[1] << 8);
}

static void write16(File & f, uint16_t v) {
  f.write((uint8_t)(v & 0xFF));
  f.write((uint8_t)((v >> 8) & 0xFF));
}

static void readAlarmConfig() {
  File f = SPIFFS.open("/config.bin", "r");
  if (!f) {
    Serial.println("Config file not available");
    return;
  }
  Serial.println("Reading config file");
  sleepTime = read16(f);
  awakeTime = read16(f);
  awakeTransition = read16(f);
  f.close();
}

static void writeAlarmConfig() {
  File f = SPIFFS.open("/config.bin", "w");
  if (!f) {
    Serial.println("Error opening config file");
    return;
  }
  write16(f, sleepTime);
  write16(f, awakeTime);
  write16(f, awakeTransition);
  f.close();
}

static void renderConfigPage() {
  char buffer[2048];
  int generated = sprintf(buffer, "<!DOCTYPE html>"
    "<html lang=\"nl\"><head><title>Kids Clock</title>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/normalize/8.0.1/normalize.css\">"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.14.0/css/all.css\">"
    "<link rel=\"icon\" type=\"image/svg+xml\" href=\"/favicon.svg\">"
    "<link rel=\"icon\" type=\"image/png\" href=\"/favicon.png\">"
    "<style>"
      "@import url('https://fonts.googleapis.com/css2?family=Noto+Sans&display=swap');"
      "body{ font-family: 'Noto Sans', sans-serif; margin: 1em; line-height: 1.2; }"
      ".entry{ display: block; margin: 0.5em; }"
    "</style></head>"
    "<body>"
    "<img src=\"/favicon.svg\" width=\"100%\" style=\"max-width: 200px; margin: 0 auto; display: block;\"/>"
    "<form action=\"/set\" method=\"POST\">"
    "<span class=\"entry\"><label for=\"sleep\">Sleep</label><input id=\"sleep\" name=\"sleep\" type=\"time\" value=\"%02d:%02d\"/></span>"
    "<span class=\"entry\"><label for=\"awakeTransition\">Awake transition</label><input id=\"awakeTransition\" name=\"awakeTransition\" type=\"number\" style=\"width:3em\" value=\"%d\"/> minutes</span>"
    "<span class=\"entry\"><label for=\"awake\">Awake</label><input id=\"awake\" name=\"awake\" type=\"time\" value=\"%02d:%02d\"/></span>"
    "<input type=\"submit\" value=\"Change\" style=\"display:block\">"
    "</form>"
    "</body></html>"
  , sleepTime / 60, sleepTime % 60, awakeTransition, awakeTime / 60, awakeTime % 60);
  http.send(200, "text/html", buffer, generated);
}

static void faviconSVG() {
  File f = SPIFFS.open("/favicon.svg", "r");
  http.streamFile(f, "image/svg+xml");
  f.close();
}
static void faviconPNG() {
  File f = SPIFFS.open("/favicon.png", "r");
  http.streamFile(f, "image/png");
  f.close();
}

static long parseTime(const String & arg) {
  return (arg.substring(0, 2).toInt() * 60) + arg.substring(3).toInt();
}

static void handleConfigChange() {
  sleepTime = parseTime(http.arg("sleep"));
  awakeTime = parseTime(http.arg("awake"));
  awakeTransition = http.arg("awakeTransition").toInt();
  writeAlarmConfig();
  http.sendHeader("Location", String("/"), true);
  http.send(302, "text/plain", "");
}