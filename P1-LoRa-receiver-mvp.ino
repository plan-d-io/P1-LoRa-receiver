/*
 * P1-LoRa-receiver MVP - Webserver + WiFi + NTP + config.
 * Target: M5Stack Atom Lite. No LoRa, MQTT, or HTTP upload in this build.
 */
#include "boards.h"
#include <esp_system.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <time.h>
#include "ArduinoJson.h"
#include <elapsedMillis.h>
#include "UUID.h"
#include <LittleFS.h>
#include "configStore.h"
#include "ledControl.h"
#include "webHelp.h"
#include <freertos/semphr.h>

Preferences preferences;
SemaphoreHandle_t svgMutex = NULL;
SemaphoreHandle_t syslogMutex = NULL;
AsyncWebServer server(80);
DNSServer dnsServer;
UUID uuid;

bool resetWifi = false;
bool factoryReset = false;
bool wifiError = false;
bool wifiScan = false;
bool debugInfo = true;
bool timeSet = false;
bool timeconfigured = false;
bool spiffsMounted = false;
bool rebootInit = false;
bool EIDuploadEn = false;

String configBuffer;
String resetReason;
String infoMsg = "";
String ssidList = "{}";

char apSSID[] = "P1000000";
unsigned int fw_ver = 300;  // 3.00 for MVP

elapsedMillis sinceConnCheck;
elapsedMillis sinceRebootCheck;
elapsedMillis sinceWifiCheck;
elapsedMillis sinceClockCheck;
elapsedMillis sinceBoot;

unsigned int reconncount = 0;
int wifiRSSI = 0;
uint8_t mac[6];

bool httpDebug = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("P1-LoRa-receiver MVP (webserver + WiFi + NTP)");

  /* Initialize WiFi early so WiFi.macAddress() is safe in getHostname() */
  WiFi.mode(WIFI_OFF);
  delay(10);

  pixel.begin();
  pixel.setBrightness(30);
  pixel.show();

  pinMode(BOARD_BUTTON_PIN, INPUT);

  setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");

  unitState = -1;
  getHostname();
  logHostname();

  syslog("P1 receiver MVP booting", 1);
  restoreConfig();
  syslog("Config restored from NVS", 1);
  /* Original behaviour (P1-dongle externalIntegrationsBootstrap): if SSID is set, use STA mode. OTA-safe. */
  if (_wifi_ssid.length() > 0) _wifi_STA = true;

  initLittleFS();
  configBuffer = returnConfig();
  syslog("Config buffer built", 1);

  syslog("P1 receiver " + String(apSSID) + " V" + String(fw_ver / 100.0) + " MVP", 1);
  syslog("Checking RTC/time", 1);
  printLocalTime(true);

  _bootcount = _bootcount + 1;
  syslog("Boot #" + String(_bootcount), 1);
  saveBoots();

  get_reset_reason((int)esp_reset_reason());
  syslog("Last reset (HW): " + resetReason, 1);
  syslog("Last reset (FW): " + _last_reset, 1);

  initWifi();
  if (svgMutex == NULL) svgMutex = xSemaphoreCreateMutex();
  if (syslogMutex == NULL) syslogMutex = xSemaphoreCreateMutex();
  setupServer();
  syslog("Web server routes registered", 1);
  sinceBoot = 0;

  syslog("Setup done", 1);
  unitState = (_wifi_STA && WiFi.status() == WL_CONNECTED) ? 4 : 0;
}

void loop() {
  /* Start HTTP server after TCPIP stack is ready (avoids lwIP assert on ESP32 3.x) */
  if (sinceBoot > 2000) {
    startServer();
  }

  blinkLed();

  if (wifiScan) scanWifi();

  if (sinceRebootCheck > 2000) {
    if (rebootInit) forcedReset();
    sinceRebootCheck = 0;
  }

  if (!_wifi_STA) {
    dnsServer.processNextRequest();
    if (sinceWifiCheck >= 600000) {
      if (scanWifi()) {
        saveResetReason("Found saved WiFi, rebooting to reconnect");
        if (saveConfig()) {
          syslog("Found saved WiFi, rebooting", 1);
          setReboot();
        }
      }
      sinceWifiCheck = 0;
    }
    if (sinceClockCheck >= 600000) {
      timeSet = false;
      sinceClockCheck = 0;
    }
  } else {
    if (sinceClockCheck >= 3600) {
      if (!timeconfigured) timeSet = false;
      sinceClockCheck = 0;
    }
    if (sinceConnCheck >= 60000) {
      checkConnection();
      sinceConnCheck = 0;
    }
  }
}

void setTimezone(String timezone) {
  setenv("TZ", timezone.c_str(), 1);
  tzset();
}
