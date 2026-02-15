/*
 * syslog.ino - Log to Serial and to LittleFS /syslog.txt when mounted.
 * Rotate when file exceeds 5120 bytes (move to /syslog0.txt).
 * syslogMutex serializes file access with the /syslog HTTP handler to avoid crash on concurrent read/write.
 */
#include "ArduinoJson.h"
#include <freertos/semphr.h>
extern SemaphoreHandle_t syslogMutex;

void syslog(String msg, int level) {
  String logmsg;
  if (timeSet) {
    logmsg = printLocalTime(false) + " ";
  }
  if (level == 0 || level == 1) logmsg += "INFO: ";
  else if (level == 2) logmsg += "WARNING: ";
  else if (level == 3 || level == 4) logmsg += "ERROR: ";
  else logmsg += "MISC: ";
  logmsg += msg;
  Serial.println(logmsg);

  /* Publish to MQTT in real time for level 1–3 (INFO, WARNING, ERROR), like original firmware */
  if (level > 0 && level < 4 && _mqtt_en && !mqttClientError && !mqttHostError) {
    DynamicJsonDocument doc(1024);
    doc["friendly_name"] = "System log";
    doc["value"] = logmsg;
    doc["entity"] = apSSID;
    doc["sensorId"] = "syslog";
    doc["timestamp"] = (unsigned long)time(nullptr);
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    pubMqtt("sys/devices/" + String(apSSID) + "/syslog", jsonOutput, false);
  }

  /* Append to file for level > 0 when LittleFS is mounted */
  if (level > 0 && spiffsMounted && syslogMutex != NULL) {
    xSemaphoreTake(syslogMutex, portMAX_DELAY);
    if (sizeFile(LittleFS, "/syslog.txt") > 5120) {
      Serial.println("Swapping logfiles");
      deleteFile(LittleFS, "/syslog0.txt");
      renameFile(LittleFS, "/syslog.txt", "/syslog0.txt");
    }
    logmsg += "\r\n";
    appendFile(LittleFS, "/syslog.txt", logmsg.c_str());
    xSemaphoreGive(syslogMutex);
  }
}

void saveResetReason(String rReason) {
  if (timeSet) _last_reset = printLocalTime(false) + " ";
  else _last_reset = "";
  _last_reset += rReason;
}

/* Push last numLines of syslog to MQTT (e.g. after reconnect). Reads from LittleFS under mutex. */
void pushSyslog(int numLines) {
  Serial.println(" ----Printing missing syslog lines");
  if (!_mqtt_en || mqttClientError || mqttHostError || !spiffsMounted || syslogMutex == NULL) {
    if (!spiffsMounted) Serial.println("SPIFFS is not mounted.");
    return;
  }
  xSemaphoreTake(syslogMutex, portMAX_DELAY);
  File file = LittleFS.open("/syslog.txt", "r");
  if (!file) {
    Serial.println("Failed to open syslog.txt for reading.");
    xSemaphoreGive(syslogMutex);
    return;
  }
  size_t len = file.size();
  size_t start = (len > 3072) ? (len - 3072) : 0;
  file.seek(start, SeekSet);
  String chunk;
  while (file.available() && chunk.length() < 3072) chunk += (char)file.read();
  file.close();
  xSemaphoreGive(syslogMutex);
  /* Collect last numLines (split by \r\n or \n) and send each to MQTT */
  int n = 0;
  int pos = chunk.length();
  for (int i = chunk.length() - 1; i >= 0 && n < numLines; i--) {
    if (chunk.charAt(i) == '\n') {
      n++;
      pos = i;
    }
  }
  String block = (pos < (int)chunk.length()) ? chunk.substring(pos) : chunk;
  block.trim();
  int idx = 0;
  while (block.length() > 0 && idx < numLines) {
    int sep = block.indexOf("\r\n");
    if (sep < 0) sep = block.indexOf('\n');
    if (sep < 0) sep = block.length();
    String line = block.substring(0, sep);
    line.trim();
    if (line.length() > 0) {
      Serial.println(line);
      DynamicJsonDocument doc(1024);
      doc["friendly_name"] = "System log";
      doc["value"] = line;
      doc["entity"] = apSSID;
      doc["sensorId"] = "syslog";
      doc["timestamp"] = (unsigned long)time(nullptr);
      String jsonOutput;
      serializeJson(doc, jsonOutput);
      pubMqtt("sys/devices/" + String(apSSID) + "/syslog", jsonOutput, false);
    }
    block = (sep < (int)block.length()) ? block.substring(sep + 1) : "";
    if (block.length() > 0 && (block.charAt(0) == '\n' || (block.length() > 1 && block.charAt(0) == '\r'))) block = block.substring(1);
    idx++;
  }
}
