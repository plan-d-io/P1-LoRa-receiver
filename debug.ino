/*
 * debug.ino - Debug values and MQTT push (heap, reset reason, reboots, etc.).
 * get_reset_reason() remains in utilities.ino; this file provides getHeapDebug() and pushDebugValues().
 */
#include "ArduinoJson.h"

void getHeapDebug() {
  freeHeap = ESP.getFreeHeap() / 1000.0;
  minFreeHeap = ESP.getMinFreeHeap() / 1000.0;
  maxAllocHeap = ESP.getMaxAllocHeap() / 1000.0;
  Serial.println(freeHeap);
  if (_mqtt_en && debugInfo) pushDebugValues();
}

void pushDebugValues() {
  unsigned long dtimestamp = (unsigned long)time(nullptr);
  for (int i = 0; i < 11; i++) {
    String chanName = "";
    String dtopic = "";
    DynamicJsonDocument doc(1024);
    if (i == 0) {
      chanName = "reboots";
      doc["friendly_name"] = "Reboots";
      doc["value"] = _bootcount;
    } else if (i == 1) {
      chanName = "last_reset_reason_hw";
      doc["friendly_name"] = "Last reset reason (hardware)";
      doc["value"] = resetReason;
    } else if (i == 2) {
      chanName = "free_heap_size";
      doc["friendly_name"] = "Free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["value"] = freeHeap;
    } else if (i == 3) {
      chanName = "max_allocatable_block";
      doc["friendly_name"] = "Allocatable block size";
      doc["unit_of_measurement"] = "kB";
      doc["value"] = maxAllocHeap;
    } else if (i == 4) {
      chanName = "min_free_heap";
      doc["friendly_name"] = "Lowest free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["value"] = minFreeHeap;
    } else if (i == 5) {
      chanName = "last_reset_reason_fw";
      doc["friendly_name"] = "Last reset reason (firmware)";
      doc["value"] = _last_reset;
    } else if (i == 6) {
      chanName = "ip";
      doc["friendly_name"] = "IP";
      doc["value"] = WiFi.localIP().toString();
    } else if (i == 7) {
      chanName = "firmware";
      doc["friendly_name"] = "Firmware";
      doc["value"] = fw_ver / 100.0;
    } else if (i == 8) {
      chanName = "release_channel";
      doc["friendly_name"] = "Release channel";
      if (_alpha_fleet) doc["value"] = "alpha";
      else if (_dev_fleet) doc["value"] = "development";
      else doc["value"] = "main";
    } else if (i == 9) {
      chanName = "email";
      doc["friendly_name"] = "Email";
      doc["value"] = _user_email;
    } else if (i == 10) {
      chanName = "rssi";
      doc["friendly_name"] = "RSSI";
      doc["value"] = wifiRSSI;
    }
    doc["entity"] = apSSID;
    doc["sensorId"] = chanName;
    doc["timestamp"] = dtimestamp;
    if (_realto_en) dtopic = _mqtt_prefix + "sys/" + chanName;
    else dtopic = "sys/devices/" + String(apSSID) + "/" + chanName;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    if (_mqtt_en) {
      if (sinceLastUpload >= (unsigned long)_upload_throttle * 1000) {
        pubMqtt(dtopic, jsonOutput, true);
      }
    }
  }
}
