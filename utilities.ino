/*
 * utilities.ino - WiFi, NTP, LittleFS, hostname, reboot. Secure client uses embedded cert bundle.
 */
#include "ArduinoJson.h"
#include <esp_system.h>
#include "esp_task_wdt.h"

String timestring;

/* Reset reason strings from esp_reset_reason_t (ESP-IDF / Arduino ESP32 core 3.x). */
void get_reset_reason(int reason) {
  switch (reason) {
    case 0:  resetReason = "UNKNOWN"; break;
    case 1:  resetReason = "POWERON_RESET"; break;
    case 2:  resetReason = "EXT_RESET"; break;
    case 3:  resetReason = "SW_RESET"; break;
    case 4:  resetReason = "PANIC_RESET"; break;
    case 5:  resetReason = "INT_WDT_RESET"; break;
    case 6:  resetReason = "TASK_WDT_RESET"; break;
    case 7:  resetReason = "WDT_RESET"; break;
    case 8:  resetReason = "DEEPSLEEP_RESET"; break;
    case 9:  resetReason = "BROWNOUT_RESET"; break;
    case 10: resetReason = "SDIO_RESET"; break;
    case 11: resetReason = "USB_RESET"; break;
    case 12: resetReason = "JTAG_RESET"; break;
    case 13: resetReason = "EFUSE_RESET"; break;
    case 14: resetReason = "PWR_GLITCH_RESET"; break;
    case 15: resetReason = "CPU_LOCKUP_RESET"; break;
    default: resetReason = "NO_MEAN";
  }
}

boolean scanWifi() {
  syslog("Performing wifi scan", 1);
  boolean foundSavedSSID = false;
  int16_t n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == _wifi_ssid) foundSavedSSID = true;
  }
  syslog("WiFi scan complete: " + String(n) + " network(s) found", 1);
  DynamicJsonDocument doc(1024);
  JsonArray data = doc.createNestedArray("SSIDlist");
  int offset = 0;
  if (foundSavedSSID) {
    data[0]["SSID"] = _wifi_ssid;
    offset = 1;
  }
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) != _wifi_ssid) {
      data[offset]["SSID"] = WiFi.SSID(i);
      offset++;
    }
  }
  serializeJson(doc, ssidList);
  wifiScan = false;
  WiFi.scanDelete();
  return foundSavedSSID;
}

String getHostname() {
  WiFi.macAddress(mac);
  char macbuf[7] = "000000";
  String macbufs = "";
  macbufs += String(mac[3], HEX);
  macbufs += String(mac[4], HEX);
  macbufs += String(mac[5], HEX);
  macbufs.toUpperCase();
  macbufs.toCharArray(macbuf, 7);
  apSSID[2] = macbuf[0];
  apSSID[3] = macbuf[1];
  apSSID[4] = macbuf[2];
  apSSID[5] = macbuf[3];
  apSSID[6] = macbuf[4];
  apSSID[7] = macbuf[5];
  return macbufs;
}

void initWifi() {
  scanWifi();
  if (_wifi_STA && _wifi_ssid.length() > 0) {
    syslog("WiFi mode: station", 1);
    WiFi.mode(WIFI_STA);
    if (_fip_en) {
      if (!WiFi.config(_fipaddr, _fdefgtw, _fsubn, _fdns1, _fdns2)) syslog("Failed to set static IP", 2);
    }
    WiFi.begin(_wifi_ssid.c_str(), _wifi_password.c_str());
    WiFi.setHostname("p1receiver");
    elapsedMillis startAttemptTime;
    syslog("Attempting connection to " + _wifi_ssid, 0);
    while (WiFi.status() != WL_CONNECTED && startAttemptTime < 20000) {
      delay(200);
      Serial.print(".");
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      syslog("Connected to " + _wifi_ssid, 1);
      syslog("Local IP: " + WiFi.localIP().toString(), 0);
      _fipaddr = (uint32_t)WiFi.localIP();
      _fdefgtw = (uint32_t)WiFi.gatewayIP();
      _fsubn = (uint32_t)WiFi.subnetMask();
      _fdns1 = (uint32_t)WiFi.dnsIP();
      _fdns2 = (uint32_t)WiFi.dnsIP(1);
      if (!MDNS.begin("p1receiver")) Serial.println("mDNS failed");
      else MDNS.addService("http", "tcp", 80);
      unitState = 4;
      WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
      setClock(true);
      printLocalTime(true);
      sinceConnCheck = 60000;
      /* Match old code: enable update autocheck when STA connects so version check runs after ~1 min. */
      _update_autoCheck = true;
      sinceUpdateCheck = 86400000 - 60000;
      setupSecureClient();
      if (_mqtt_en) setupMqtt();
      if (_update_start) {
        syslog("OTA update requested, starting update", 1);
        startUpdate();
      }
      if (_update_finish) {
        syslog("Finish update requested", 1);
        finishUpdate(false);
      }
    } else {
      syslog("Could not connect to WiFi", 2);
      wifiError = true;
      _wifi_STA = false;
      unitState = 1;
    }
  }
  if (!_wifi_STA) {
    syslog("WiFi mode: access point", 1);
    WiFi.mode(WIFI_AP);
    WiFi.softAP("p1receiver");
    dnsServer.start(53, "*", WiFi.softAPIP());
    MDNS.begin("p1receiver");
    syslog("AP set up", 1);
    unitState = 0;
  }
}

String printLocalTime(boolean verbosePrint) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    timestring = "";
    if (verbosePrint) syslog("Failed to obtain time from RTC", 2);
    timeSet = false;
    return "";
  }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y/%m/%d %H:%M:%S", &timeinfo);
  timestring = String(timeStringBuff);
  if (verbosePrint) syslog("Time set: " + timestring, 1);
  timeSet = true;
  return timestring;
}

void logHostname() {
  syslog("Hostname: " + String(apSSID), 1);
}

unsigned long printUnixTime() {
  return (unsigned long)time(nullptr);
}

void setClock(boolean firstSync) {
  if (firstSync) {
    syslog("Configuring NTP time sync", 1);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    timeconfigured = true;
  }
}

void WiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  (void)event;
  (void)info;
  sinceConnCheck = 60000;
}

void checkConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    syslog("Lost WiFi, reconnecting...", 2);
    WiFi.disconnect();
    wifiError = true;
    mqttClientError = true;
    elapsedMillis t;
    while (WiFi.status() != WL_CONNECTED && t < 20000) {
      WiFi.begin(_wifi_ssid.c_str(), _wifi_password.c_str());
      delay(500);
    }
    if (t >= 20000) {
      reconncount++;
      syslog("WiFi reconnect timeout", 2);
    }
  }
  if (wifiError && WiFi.status() == WL_CONNECTED) {
    wifiError = false;
    syslog("WiFi reconnected", 1);
    reconncount = 0;
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiRSSI = WiFi.RSSI();
    if (_mqtt_en && !mqttPaused) {
      if (mqttPushFails > 5) {
        mqttClientError = true;
        syslog("MQTT client connection failed", 4);
        mqttPushFails = 0;
        reconncount++;
      }
      if (mqttHostError) setupMqtt();
      else connectMqtt();
      if (mqttWasPaused) {
        connectMqtt();
        mqttWasPaused = false;
      }
    }
  }
}

void setReboot() {
  sinceConnCheck = 0;
  saveConfig();
  rebootInit = true;
  sinceRebootCheck = 0;
  syslog("Rebooting", 2);
}

void forcedReset() {
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&wdt_config);
  esp_task_wdt_add(NULL);
  while (true) { delay(10); }
}

double round2(double value) {
  return (int)(value * 100 + 0.05) / 100.0;
}

IPAddress uint32ToIPAddress(uint32_t ipInt) {
  return IPAddress((uint8_t)(ipInt), (uint8_t)(ipInt >> 8), (uint8_t)(ipInt >> 16), (uint8_t)(ipInt >> 24));
}

uint32_t ipStringToUint32(String ipStr) {
  IPAddress ip;
  if (!ip.fromString(ipStr)) return 0;
  return (uint32_t)ip[3] << 24 | (uint32_t)ip[2] << 16 | (uint32_t)ip[1] << 8 | ip[0];
}

/* LittleFS init and file helpers for syslog. */
void initLittleFS() {
  syslog("Mounting LittleFS... ", 1);
  if (!LittleFS.begin(false)) {
    /* Empty or invalid partition (e.g. after erase): format once then mount. */
    syslog("LittleFS mount failed, formatting... ", 1);
    if (!LittleFS.format() || !LittleFS.begin(false)) {
      syslog("Could not mount LittleFS (use Serial only for logs)", 3);
      spiffsMounted = false;
      return;
    }
  }
  syslog("LittleFS used/total: " + String(LittleFS.usedBytes()) + "/" + String(LittleFS.totalBytes()), 1);
  if (!writeFile(LittleFS, "/test.txt", "Hello ") || !appendFile(LittleFS, "/test.txt", "World!\r\n") || !deleteFile(LittleFS, "/test.txt")) {
    syslog("LittleFS file I/O test failed", 3);
    LittleFS.end();
    spiffsMounted = false;
    return;
  }
  spiffsMounted = true;
  syslog("LittleFS OK", 1);
}

bool writeFile(fs::FS& fs, const char* path, const char* message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) return false;
  bool ok = file.print(message);
  file.close();
  return ok;
}

bool appendFile(fs::FS& fs, const char* path, const char* message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) return false;
  bool ok = file.print(message);
  file.close();
  return ok;
}

bool deleteFile(fs::FS& fs, const char* path) {
  return fs.remove(path);
}

bool renameFile(fs::FS& fs, const char* path1, const char* path2) {
  return fs.rename(path1, path2);
}

int sizeFile(fs::FS& fs, const char* path) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    return 0;
  }
  int n = (int)file.size();
  file.close();
  return n;
}

/* Read file into String (max len bytes). Caller must hold syslogMutex if used for syslog. */
String readFileToString(fs::FS& fs, const char* path, size_t maxLen) {
  String out;
  File f = fs.open(path, "r");
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return out;
  }
  while (f.available() && out.length() < maxLen)
    out += (char)f.read();
  f.close();
  return out;
}
