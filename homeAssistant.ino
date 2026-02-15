/*
 * homeAssistant.ino - Home Assistant MQTT autodiscovery.
 * Two devices: (1) Utility meter (DSMR keys) - doHaAutoDiscovery when we have dsmrKeys; (2) Debug device - hadebugDevice.
 * MVP: no LoRa/DSMR data yet, so DSMR/MBUS loops are no-ops; only debug device is registered.
 */
#include "ArduinoJson.h"

/* When LoRa/DSMR is ported, set these from dsmrTelegram and use in doHaAutoDiscovery / haEraseDevice. */
static const int DSMR_KEYS_COUNT = 0;
static const int MBUS_KEYS_COUNT = 0;
static const int MBUS_METER_COUNT = 0;

void controlHA() {
  if (_mqtt_tls) {
    if (!mqttclientSecure.connected()) return;
  } else {
    if (!mqttclient.connected()) return;
  }
  /* MVP: no telegramAction; debug device is driven from checkConnection and connectMqtt. */
  if (!haDiscovered) doHaAutoDiscovery();
}

void doHaAutoDiscovery() {
  if (!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) return;
  if (_mqtt_tls) {
    if (!mqttclientSecure.connected()) return;
  } else {
    if (!mqttclient.connected()) return;
  }
  /* DSMR/MBUS autodiscovery when we have keys (LoRa port); MVP: skip. */
  for (int i = 0; i < DSMR_KEYS_COUNT; i++) {
    (void)i;
    /* haAutoDiscovery(dsmrKeys[i].keyName, keyUnit, dsmrKeys[i].deviceType, dsmrKeys[i].keyTopic); */
  }
  for (int i = 0; i < MBUS_METER_COUNT; i++) {
    (void)i;
    /* mbus meter discovery */
  }
  /* Debug device: register all debug sensors/switches/select. */
  hadebugDevice(false);
  haDiscovered = true;
}

void haAutoDiscovery(String friendlyName, String unit, String deviceType, String mqttTopic) {
  if (!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) return;
  String jsonOutput;
  String tempTopic = _mqtt_prefix;
  if (mqttTopic == "") {
    tempTopic += friendlyName;
    tempTopic.replace(" ", "_");
    tempTopic.toLowerCase();
  } else {
    tempTopic += mqttTopic;
    tempTopic.replace(" ", "_");
    tempTopic.toLowerCase();
  }
  DynamicJsonDocument doc(1024);
  doc["name"] = friendlyName;
  if (friendlyName == "Packet loss") unit = "%";
  if (deviceType != "") doc["device_class"] = deviceType;
  if (unit != "") doc["unit_of_measurement"] = unit;
  doc["state_topic"] = tempTopic;
  if (deviceType == "energy" || deviceType == "gas" || deviceType == "water") doc["state_class"] = "total_increasing";
  else doc["state_class"] = "measurement";
  if (friendlyName == "SNR" || friendlyName == "Spreading factor") doc["icon"] = "mdi:wifi-strength-3";
  if (friendlyName == "Packet loss") doc["icon"] = "mdi:antenna";
  friendlyName.replace(" ", "_");
  friendlyName.toLowerCase();
  String deviceName = _ha_device;
  deviceName.replace(" ", "_");
  deviceName.toLowerCase();
  doc["unique_id"] = deviceName + "_" + friendlyName;
  doc["object_id"] = deviceName + "_" + friendlyName;
  if (_payload_format > 0) doc["value_template"] = "{{ value_json.value }}";
  doc["availability_topic"] = _mqtt_prefix.length() > 0 ? _mqtt_prefix.substring(0, _mqtt_prefix.length() - 1) : "";
  JsonObject device = doc.createNestedObject("device");
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(deviceName);
  device["name"] = _ha_device;
  device["model"] = "P1 LoRa dongle for DSMR compatible utility meters";
  device["manufacturer"] = "plan-d.io";
  device["configuration_url"] = "http://" + WiFi.localIP().toString();
  device["sw_version"] = String(fw_ver / 100.0);
  String configTopic = "homeassistant/sensor/" + deviceName + "_" + friendlyName + "/config";
  serializeJson(doc, jsonOutput);
  bool pushSuccess = pubMqtt(configTopic, jsonOutput, true);
  if (mqttDebug && pushSuccess) {
    Serial.print(configTopic);
    Serial.print(" ");
    serializeJson(doc, Serial);
    Serial.println("");
  }
  if (mqttPushCount < 4) delay(100);
}

void haEraseDevice() {
  if (!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) return;
  if (_mqtt_tls) {
    if (!mqttclientSecure.connected()) return;
  } else {
    if (!mqttclient.connected()) return;
  }
  syslog("Erasing Home Assistant MQTT autodiscovery entries", 0);
  /* Erase DSMR keys (when DSMR_KEYS_COUNT > 0) - MVP: skip. */
  for (int i = 0; i < DSMR_KEYS_COUNT; i++) {
    (void)i;
    /* String tempTopic = "homeassistant/sensor/" + deviceName + " " + dsmrKeys[i].keyName; pubMqtt(tempTopic, "", false); */
  }
  for (int i = 0; i < MBUS_KEYS_COUNT; i++) {
    (void)i;
    /* mbus erase */
  }
  /* Erase debug device: same 12 config topics as in hadebugDevice. */
  const char* debugChanNames[] = {
    "reboots", "last_reset_reason_hw", "free_heap_size", "max_allocatable_block",
    "min_free_heap", "last_reset_reason_fw", "syslog", "ip", "firmware",
    "release_channel", "reboot", "loraset"
  };
  const char* debugTypes[] = {
    "sensor", "sensor", "sensor", "sensor", "sensor", "sensor",
    "sensor", "sensor", "sensor", "sensor", "switch", "select"
  };
  for (int i = 0; i < 12; i++) {
    String chanName = String(apSSID) + "_" + String(debugChanNames[i]);
    String configTopic = "homeassistant/" + String(debugTypes[i]) + "/" + chanName + "/config";
    pubMqtt(configTopic, "", true);
    if (mqttDebug) {
      Serial.print("Erasing ");
      Serial.println(configTopic);
    }
  }
}

void hadebugDevice(bool eraseMeter) {
  if (!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) return;
  if (_mqtt_tls) {
    if (!mqttclientSecure.connected()) return;
  } else {
    if (!mqttclient.connected()) return;
  }
  Serial.println("performing autodisc");
  for (int i = 0; i < 12; i++) {
    String chanName = "";
    DynamicJsonDocument doc(1024);
    if (i == 0) {
      chanName = String(apSSID) + "_reboots";
      doc["name"] = "Reboots";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/reboots";
    } else if (i == 1) {
      chanName = String(apSSID) + "_last_reset_reason_hw";
      doc["name"] = "Last reset reason (hardware)";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/last_reset_reason_hw";
    } else if (i == 2) {
      chanName = String(apSSID) + "_free_heap_size";
      doc["name"] = "Free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/free_heap_size";
    } else if (i == 3) {
      chanName = String(apSSID) + "_max_allocatable_block";
      doc["name"] = "Allocatable block size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/max_allocatable_block";
    } else if (i == 4) {
      chanName = String(apSSID) + "_min_free_heap";
      doc["name"] = "Lowest free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/min_free_heap";
    } else if (i == 5) {
      chanName = String(apSSID) + "_last_reset_reason_fw";
      doc["name"] = "Last reset reason (firmware)";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/last_reset_reason_fw";
    } else if (i == 6) {
      chanName = String(apSSID) + "_syslog";
      doc["name"] = "Syslog";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/syslog";
    } else if (i == 7) {
      chanName = String(apSSID) + "_ip";
      doc["name"] = "IP";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/ip";
    } else if (i == 8) {
      chanName = String(apSSID) + "_firmware";
      doc["name"] = "Firmware";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/firmware";
    } else if (i == 9) {
      chanName = String(apSSID) + "_release_channel";
      doc["name"] = "Release channel";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/release_channel";
    } else if (i == 10) {
      chanName = String(apSSID) + "_reboot";
      doc["name"] = "Reboot";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/reboot";
      doc["payload_on"] = "{\"value\": \"on\"}";
      doc["payload_off"] = "{\"value\": \"off\"}";
      doc["state_on"] = "on";
      doc["state_off"] = "off";
      doc["value_template"] = "{{ value_json.value }}";
      String tempTopic = _mqtt_prefix.length() > 0 ? _mqtt_prefix.substring(0, _mqtt_prefix.length() - 1) : "";
      tempTopic += "/set/reboot";
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      doc["command_topic"] = tempTopic;
      doc["icon"] = "mdi:restart";
    } else if (i == 11) {
      chanName = String(apSSID) + "_loraset";
      doc["name"] = "LoRa radio settings";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/loraset";
      JsonArray options = doc.createNestedArray("options");
      options.add("Automatic");
      options.add("SF12 BW125");
      options.add("SF12 BW250");
      options.add("SF11 BW250");
      options.add("SF10 BW250");
      options.add("SF9 BW250");
      options.add("SF8 BW250");
      options.add("SF7 BW250");
      doc["value_template"] = "{{ value_json.value }}";
      String tempTopic = _mqtt_prefix.length() > 0 ? _mqtt_prefix.substring(0, _mqtt_prefix.length() - 1) : "";
      tempTopic += "/set/loraset";
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      doc["command_topic"] = tempTopic;
      doc["icon"] = "mdi:antenna";
    }
    doc["unique_id"] = chanName;
    doc["object_id"] = chanName;
    doc["availability_topic"] = _mqtt_prefix.length() > 0 ? _mqtt_prefix.substring(0, _mqtt_prefix.length() - 1) : "";
    doc["value_template"] = "{{ value_json.value }}";
    JsonObject device = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(apSSID);
    device["name"] = apSSID;
    device["model"] = "P1 LoRa dongle debug monitoring";
    device["manufacturer"] = "plan-d.io";
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    device["sw_version"] = String(fw_ver / 100.0);
    String configTopic = "";
    if (i == 10) configTopic = "homeassistant/switch/" + chanName + "/config";
    else if (i == 11) configTopic = "homeassistant/select/" + chanName + "/config";
    else configTopic = "homeassistant/sensor/" + chanName + "/config";
    String jsonOutput = "";
    if (eraseMeter) {
      if (chanName.length() > 0) pubMqtt(configTopic, "", true);
      if (mqttDebug) {
        Serial.print("Erasing ");
        Serial.println(configTopic);
      }
    }
    serializeJson(doc, jsonOutput);
    if (!eraseMeter) {
      if (chanName.length() > 0) {
        bool pushSuccess = pubMqtt(configTopic, jsonOutput, true);
        if (mqttDebug && pushSuccess) {
          Serial.println("");
          Serial.print(configTopic);
          Serial.print(" ");
          serializeJson(doc, Serial);
        }
      }
    }
    if (mqttPushCount < 4) delay(100);
  }
  pubMqtt("sys/devices/" + String(apSSID) + "/reboot", "{\"value\": \"off\"}", false);
  pubMqtt("sys/devices/" + String(apSSID) + "/loraset", "{\"value\": \"" + _loraset + "\"}", false);
}
