/*
 * dataPlaceholder.ino - Placeholder for /data API (meter values). Real data when LoRa/DSMR is added.
 */
#include "ArduinoJson.h"

String httpTelegramValues(String option) {
  (void)option;
  DynamicJsonDocument doc(512);
  JsonArray arr = doc.to<JsonArray>();
  JsonObject o = arr.add<JsonObject>();
  o["friendly_name"] = "Electricity delivered";
  o["value"] = 0.0;
  o["unit"] = "kWh";
  o["timestamp"] = (unsigned long)time(nullptr);
  JsonObject o2 = arr.add<JsonObject>();
  o2["friendly_name"] = "Electricity received";
  o2["value"] = 0.0;
  o2["unit"] = "kWh";
  o2["timestamp"] = (unsigned long)time(nullptr);
  JsonObject o3 = arr.add<JsonObject>();
  o3["friendly_name"] = "Power";
  o3["value"] = 0.0;
  o3["unit"] = "kW";
  o3["timestamp"] = (unsigned long)time(nullptr);
  String out;
  serializeJson(doc, out);
  return out;
}
