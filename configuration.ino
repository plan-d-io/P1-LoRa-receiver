/*
 * configuration.ino - NVS config load/save and HTTP API config helpers
 * MVP: no MQTT/HA/EID logic, mqttHostError/sinceConnCheck set for compatibility.
 */
#include "ArduinoJson.h"

int numKeys(void) {
  return 32;
}

boolean restoreConfig() {
  preferences.begin("cofy-config", true);
  for (int i = 0; i < (int)(sizeof(configBool) / sizeof(configBool[0])); i++) {
    if (preferences.isKey(configBool[i].configName.c_str()))
      *configBool[i].var = preferences.getBool(configBool[i].configName.c_str());
    else
      *configBool[i].var = configBool[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configInt) / sizeof(configInt[0])); i++) {
    if (preferences.isKey(configInt[i].configName.c_str()))
      *configInt[i].var = preferences.getInt(configInt[i].configName.c_str());
    else
      *configInt[i].var = configInt[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configUInt) / sizeof(configUInt[0])); i++) {
    if (preferences.isKey(configUInt[i].configName.c_str()))
      *configUInt[i].var = preferences.getUInt(configUInt[i].configName.c_str());
    else
      *configUInt[i].var = configUInt[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configULong) / sizeof(configULong[0])); i++) {
    if (preferences.isKey(configULong[i].configName.c_str()))
      *configULong[i].var = preferences.getULong(configULong[i].configName.c_str());
    else
      *configULong[i].var = configULong[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configString) / sizeof(configString[0])); i++) {
    if (preferences.isKey(configString[i].configName.c_str()))
      *configString[i].var = preferences.getString(configString[i].configName.c_str());
    else
      *configString[i].var = configString[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configPass) / sizeof(configPass[0])); i++) {
    if (preferences.isKey(configPass[i].configName.c_str()))
      *configPass[i].var = preferences.getString(configPass[i].configName.c_str());
    else
      *configPass[i].var = configPass[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configSecret) / sizeof(configSecret[0])); i++) {
    if (preferences.isKey(configSecret[i].configName.c_str()))
      *configSecret[i].var = preferences.getString(configSecret[i].configName.c_str());
    else
      *configSecret[i].var = configSecret[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configIP) / sizeof(configIP[0])); i++) {
    if (preferences.isKey(configIP[i].configName.c_str()))
      *configIP[i].var = preferences.getUInt(configIP[i].configName.c_str());
    else
      *configIP[i].var = configIP[i].defaultValue;
  }
  preferences.end();

  if (_dev_fleet) _rel_chan = "develop";
  else if (_alpha_fleet) _rel_chan = "alpha";
  else if (_v2_fleet) _rel_chan = "V2";
  else _rel_chan = "main";
  if (_mqtt_id == "") _mqtt_id = String(apSSID);
  if (_uuid == "") {
    syslog("No UUID found, generating new one", 1);
    byte mac[6];
    WiFi.macAddress(mac);
    uint32_t macPart = ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | (uint32_t)mac[5];
    uint32_t seed2 = random(999999999);
    uuid.seed(seed2, macPart);
    uuid.generate();
    char* uuidCharArray = uuid.toCharArray();
    char newArray[9];
    memcpy(newArray, uuidCharArray, 8);
    newArray[8] = '\0';
    _uuid = "P1" + String(newArray);
    syslog("Generated new UUID: " + _uuid, 1);
    _eidclaim = "";
  }
  if (_eidclaim == "") _eidclaim = _uuid.substring(2);

  return true;
}

boolean saveConfig() {
  preferences.begin("cofy-config", false);
  for (int i = 0; i < (int)(sizeof(configBool) / sizeof(configBool[0])); i++)
    preferences.putBool(configBool[i].configName.c_str(), *configBool[i].var);
  for (int i = 0; i < (int)(sizeof(configInt) / sizeof(configInt[0])); i++)
    preferences.putInt(configInt[i].configName.c_str(), *configInt[i].var);
  for (int i = 0; i < (int)(sizeof(configUInt) / sizeof(configUInt[0])); i++)
    preferences.putUInt(configUInt[i].configName.c_str(), *configUInt[i].var);
  for (int i = 0; i < (int)(sizeof(configULong) / sizeof(configULong[0])); i++)
    preferences.putULong(configULong[i].configName.c_str(), *configULong[i].var);
  for (int i = 0; i < (int)(sizeof(configString) / sizeof(configString[0])); i++)
    preferences.putString(configString[i].configName.c_str(), *configString[i].var);
  for (int i = 0; i < (int)(sizeof(configPass) / sizeof(configPass[0])); i++)
    preferences.putString(configPass[i].configName.c_str(), *configPass[i].var);
  for (int i = 0; i < (int)(sizeof(configSecret) / sizeof(configSecret[0])); i++)
    preferences.putString(configSecret[i].configName.c_str(), *configSecret[i].var);
  for (int i = 0; i < (int)(sizeof(configIP) / sizeof(configIP[0])); i++)
    preferences.putUInt(configIP[i].configName.c_str(), *configIP[i].var);
  preferences.end();
  syslog("Config saved to NVS", 1);
  return true;
}

boolean saveBoots() {
  preferences.begin("cofy-config", false);
  preferences.putUInt("reboots", _bootcount);
  preferences.end();
  return true;
}

boolean resetConfig() {
  preferences.begin("cofy-config", false);
  preferences.remove("WIFI_SSID");
  preferences.remove("WIFI_PASSWD");
  preferences.putBool("WIFI_STA", false);
  preferences.remove("FIP_EN");
  if (resetWifi) {
    preferences.putString("LAST_RESET", "Rebooting for WiFi reset");
    syslog("WiFi credentials reset by user", 2);
  } else if (factoryReset) {
    preferences.remove("MQTT_EN");
    preferences.remove("MQTT_TLS");
    preferences.remove("MQTT_AUTH");
    preferences.remove("PUSH_FULL");
    preferences.remove("BETA_FLT");
    preferences.remove("ALPHA_FLT");
    preferences.remove("V2_FLT");
    preferences.remove("HA_EN");
    preferences.remove("EID_EN");
    preferences.remove("PUSH_MBUS");
    preferences.remove("MQTT_PORT");
    preferences.remove("PUSH_DSMR");
    preferences.remove("UPL_THROTTLE");
    preferences.remove("UUID");
    preferences.remove("EIDCLAIM");
    preferences.remove("MQTT_HOST");
    preferences.remove("MQTT_ID");
    preferences.remove("MQTT_USER");
    preferences.remove("HA_DEVICE");
    preferences.remove("REL_CHAN");
    preferences.remove("EMAIL");
    preferences.remove("MQTT_PASS");
    preferences.remove("MQTT_PFIX");
    preferences.remove("LORA_SET");
    preferences.putString("LAST_RESET", "Rebooting for factory reset");
    syslog("Factory reset by user", 2);
  }
  preferences.end();
  delay(200);
  rebootInit = true;
  return rebootInit;
}

bool findInConfig(String param, int& varType, int& varNum) {
  for (int i = 0; i < (int)(sizeof(configBool) / sizeof(configBool[0])); i++) {
    if (configBool[i].configName == param) { varType = 0; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configInt) / sizeof(configInt[0])); i++) {
    if (configInt[i].configName == param) { varType = 1; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configUInt) / sizeof(configUInt[0])); i++) {
    if (configUInt[i].configName == param) { varType = 2; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configULong) / sizeof(configULong[0])); i++) {
    if (configULong[i].configName == param) { varType = 3; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configString) / sizeof(configString[0])); i++) {
    if (configString[i].configName == param) { varType = 4; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configPass) / sizeof(configPass[0])); i++) {
    if (configPass[i].configName == param) { varType = 5; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configSecret) / sizeof(configSecret[0])); i++) {
    if (configSecret[i].configName == param) { varType = 6; varNum = i; return true; }
  }
  for (int i = 0; i < (int)(sizeof(configIP) / sizeof(configIP[0])); i++) {
    if (configIP[i].configName == param) { varType = 7; varNum = i; return true; }
  }
  return false;
}

String returnConfigVar(String varName, int varType, int varNum, int level) {
  String jsonOutput;
  DynamicJsonDocument doc(1024);
  if (level == 0) {
    if (varType == 0) doc[varName] = *configBool[varNum].var;
    if (varType == 1) doc[varName] = *configInt[varNum].var;
    if (varType == 2) doc[varName] = *configUInt[varNum].var;
    if (varType == 3) doc[varName] = *configULong[varNum].var;
    if (varType == 4) doc[varName] = *configString[varNum].var;
    if (varType == 5) doc[varName] = *configPass[varNum].var;
    serializeJson(doc, jsonOutput);
  } else {
    JsonObject configVar = doc.createNestedObject(varName);
    if (varType == 0) {
      configVar["varName"] = configBool[varNum].varName;
      configVar["type"] = "bool";
      configVar["value"] = *configBool[varNum].var;
      configVar["defaultValue"] = configBool[varNum].defaultValue;
    } else if (varType == 1) {
      configVar["varName"] = configInt[varNum].varName;
      configVar["type"] = "int32";
      configVar["value"] = *configInt[varNum].var;
      configVar["defaultValue"] = configInt[varNum].defaultValue;
    } else if (varType == 2) {
      configVar["varName"] = configUInt[varNum].varName;
      configVar["type"] = "uint32";
      configVar["value"] = *configUInt[varNum].var;
      configVar["defaultValue"] = configUInt[varNum].defaultValue;
    } else if (varType == 3) {
      configVar["varName"] = configULong[varNum].varName;
      configVar["type"] = "uint64";
      configVar["value"] = *configULong[varNum].var;
      configVar["defaultValue"] = configULong[varNum].defaultValue;
    } else if (varType == 4) {
      configVar["varName"] = configString[varNum].varName;
      configVar["type"] = "string";
      configVar["value"] = *configString[varNum].var;
      configVar["defaultValue"] = configString[varNum].defaultValue;
    } else if (varType == 5) {
      configVar["varName"] = configPass[varNum].varName;
      configVar["type"] = "password";
      configVar["value"] = *configPass[varNum].var;
    } else if (varType == 6) {
      configVar["varName"] = configSecret[varNum].varName;
      configVar["type"] = "secret";
      configVar["value"] = *configSecret[varNum].var;
    } else if (varType == 7) {
      configVar["varName"] = configIP[varNum].varName;
      configVar["type"] = "ipaddress";
      configVar["value"] = uint32ToIPAddress(*configIP[varNum].var);
    }
    serializeJson(doc, jsonOutput);
    if (level == 2) {
      for (int i = 0; i < (int)(sizeof(addJson) / sizeof(addJson[0])); i++) {
        if (addJson[i][0] == varName) {
          DynamicJsonDocument addJsonFields(256);
          if (deserializeJson(addJsonFields, addJson[i][1].c_str())) {
            jsonOutput = jsonOutput.substring(0, jsonOutput.length() - 2);
            jsonOutput += ",";
            jsonOutput += addJson[i][1].substring(1);
            jsonOutput += "}";
          }
        }
      }
    }
  }
  return jsonOutput;
}

boolean storeConfigVar(String keyValue, int varType, int varNum) {
  long retLong;
  unsigned long retULong;
  float retFloat;
  if (varType == 0) {
    if (keyValue == "true" || keyValue == "True" || keyValue == "1") *configBool[varNum].var = true;
    else if (keyValue == "false" || keyValue == "False" || keyValue == "0") *configBool[varNum].var = false;
  } else if (varType == 1) {
    if (isNumeric(keyValue, retLong, retULong, retFloat) && retLong > -2146569506) *configInt[varNum].var = (int)retLong;
  } else if (varType == 2) {
    if (isNumeric(keyValue, retLong, retULong, retFloat) && retULong < 1073549248) *configUInt[varNum].var = (unsigned int)retULong;
  } else if (varType == 3) {
    if (isNumeric(keyValue, retLong, retULong, retFloat) && retULong < 1073549264) *configULong[varNum].var = retULong;
  } else if (varType == 4) {
    *configString[varNum].var = keyValue;
  } else if (varType == 5) {
    *configPass[varNum].var = keyValue;
  } else if (varType == 6) {
    *configSecret[varNum].var = keyValue;
  } else if (varType == 7) {
    *configIP[varNum].var = ipStringToUint32(keyValue);
  }
  saveConfig();
  return true;
}

boolean processConfigJson(String jsonString, String& configResponse, bool updateConfig) {
  boolean isJson = false;
  DynamicJsonDocument jsonDoc(1024);
  if (deserializeJson(jsonDoc, jsonString) == DeserializationError::Ok) {
    isJson = true;
    JsonObject documentRoot = jsonDoc.as<JsonObject>();
    for (JsonPair keyValue : documentRoot) {
      int retVarType, retVarNum;
      if (findInConfig(keyValue.key().c_str(), retVarType, retVarNum)) {
        if (updateConfig) {
          if (retVarType == 0 && keyValue.value().is<bool>()) *configBool[retVarNum].var = keyValue.value().as<bool>();
          else if (retVarType == 1) *configInt[retVarNum].var = keyValue.value().as<int>();
          else if (retVarType == 2) *configUInt[retVarNum].var = keyValue.value().as<unsigned int>();
          else if (retVarType == 3) *configULong[retVarNum].var = keyValue.value().as<unsigned long>();
          else if (retVarType == 4 && keyValue.value().is<const char*>()) *configString[retVarNum].var = keyValue.value().as<const char*>();
          else if (retVarType == 5 && keyValue.value().is<const char*>()) *configPass[retVarNum].var = keyValue.value().as<const char*>();
          else if (retVarType == 6 && keyValue.value().is<const char*>()) *configSecret[retVarNum].var = keyValue.value().as<const char*>();
          else if (retVarType == 7 && keyValue.value().is<const char*>()) *configIP[retVarNum].var = ipStringToUint32(keyValue.value().as<const char*>());
        }
        String foundInConfig = returnConfigVar(keyValue.key().c_str(), retVarType, retVarNum, 1);
        if (foundInConfig != "") {
          configResponse += foundInConfig.substring(1, foundInConfig.length() - 1);
          configResponse += ",";
        }
      }
    }
    if (updateConfig) saveConfig();
    infoMsg = "Please reboot the dongle to have changes take effect";
    mqttHostError = true;
    sinceConnCheck = 60000;
    if (configResponse != "") {
      configResponse = configResponse.substring(0, configResponse.length() - 1);
      configResponse = "{" + configResponse;
      configResponse += "}";
    }
  }
  configBuffer = returnConfig();
  return isJson;
}

boolean processConfigString(String confString, String& response, bool updateConfig) {
  String foundInConfig;
  int separator;
  while (confString.length() > 0) {
    separator = confString.indexOf('\n');
    String subString = (separator >= 0) ? confString.substring(0, separator) : confString;
    if (subString != "") {
      int kvsep = subString.indexOf('=');
      if (kvsep > 0) {
        String key = subString.substring(0, kvsep);
        if (key == "WIFI_NW") key = "WIFI_SSID";
        int sepr = subString.indexOf('\r');
        String keyValue = (sepr > 0) ? subString.substring(kvsep + 1, sepr) : subString.substring(kvsep + 1);
        int retVarType, retVarNum;
        if (findInConfig(key.c_str(), retVarType, retVarNum)) {
          if (updateConfig) storeConfigVar(keyValue.c_str(), retVarType, retVarNum);
          foundInConfig = returnConfigVar(key, retVarType, retVarNum, 1);
          if (foundInConfig != "") {
            response += foundInConfig.substring(1, foundInConfig.length() - 1);
            response += ",";
          }
        }
      }
    }
    if (separator >= 0) confString = confString.substring(separator + 1);
    else break;
  }
  if (response != "") {
    response = response.substring(0, response.length() - 1);
    response = "{" + response;
    response += "}";
  }
  return true;
}

boolean isNumeric(String& varValue, long& longValue, unsigned long& ulongValue, float& floatValue) {
  unsigned int len = varValue.length() + 1;
  if (len >= 16) return false;
  char buf[16];
  varValue.toCharArray(buf, len);
  bool isInt = true, isFloat = false, foundDecimal = false, isSigned = false;
  for (int i = 0; i < len - 1; i++) {
    if (!isDigit(buf[i])) {
      if (buf[i] == '-' && i == 0) isSigned = true;
      else if (buf[i] == '.' && !foundDecimal) { foundDecimal = true; isFloat = true; isInt = false; }
      else { isFloat = false; isInt = false; }
    }
  }
  if (isInt) {
    if (isSigned) longValue = varValue.toInt();
    else { ulongValue = strtoul(buf, NULL, 10); longValue = (long)ulongValue; }
    return true;
  }
  if (isFloat) { floatValue = varValue.toFloat(); return true; }
  return false;
}

String returnConfig() {
  String jsonOutput;
  DynamicJsonDocument doc(5120);
  JsonObject hostVar = doc.createNestedObject("HOSTNAME");
  hostVar["varName"] = "Dongle hostname";
  hostVar["type"] = "string";
  hostVar["value"] = String(apSSID);
  JsonObject fwVar = doc.createNestedObject("FW_VER");
  fwVar["varName"] = "Firmware version";
  fwVar["type"] = "numeric";
  fwVar["value"] = round2(fw_ver / 100.0);
  JsonObject eidintVar = doc.createNestedObject("EID_INTV");
  eidintVar["varName"] = "Allowed upload interval";
  eidintVar["type"] = "string";
  eidintVar["value"] = eidUploadInterval;
  for (int i = 0; i < (int)(sizeof(configBool) / sizeof(configBool[0])); i++) {
    if (!configBool[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configBool[i].configName);
    c["varName"] = configBool[i].varName;
    c["type"] = "bool";
    c["value"] = *configBool[i].var;
    c["defaultValue"] = configBool[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configInt) / sizeof(configInt[0])); i++) {
    if (!configInt[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configInt[i].configName);
    c["varName"] = configInt[i].varName;
    c["type"] = "int32";
    c["value"] = *configInt[i].var;
    c["defaultValue"] = configInt[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configUInt) / sizeof(configUInt[0])); i++) {
    if (!configUInt[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configUInt[i].configName);
    c["varName"] = configUInt[i].varName;
    c["type"] = "uint32";
    c["value"] = *configUInt[i].var;
    c["defaultValue"] = configUInt[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configULong) / sizeof(configULong[0])); i++) {
    if (!configULong[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configULong[i].configName);
    c["varName"] = configULong[i].varName;
    c["type"] = "uint64";
    c["value"] = *configULong[i].var;
    c["defaultValue"] = configULong[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configString) / sizeof(configString[0])); i++) {
    if (!configString[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configString[i].configName);
    c["varName"] = configString[i].varName;
    c["type"] = "string";
    c["value"] = *configString[i].var;
    c["defaultValue"] = configString[i].defaultValue;
  }
  for (int i = 0; i < (int)(sizeof(configPass) / sizeof(configPass[0])); i++) {
    if (!configPass[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configPass[i].configName);
    c["varName"] = configPass[i].varName;
    c["type"] = "password";
    if (*configPass[i].var != "") c["filled"] = true;
  }
  for (int i = 0; i < (int)(sizeof(configSecret) / sizeof(configSecret[0])); i++) {
    if (!configSecret[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configSecret[i].configName);
    c["varName"] = configSecret[i].varName;
    c["type"] = "secret";
    if (*configSecret[i].var != "") c["filled"] = true;
  }
  for (int i = 0; i < (int)(sizeof(configIP) / sizeof(configIP[0])); i++) {
    if (!configIP[i].includeInConfig) continue;
    JsonObject c = doc.createNestedObject(configIP[i].configName);
    c["varName"] = configIP[i].varName;
    c["type"] = "ipaddress";
    c["value"] = uint32ToIPAddress(*configIP[i].var);
    c["defaultValue"] = String(uint32ToIPAddress(configIP[i].defaultValue));
  }
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

String returnBasicConfig() {
  String basicParameters[] = {"REL_CHAN", "reboots", "UPD_AUTO", "UPD_AUTOCHK", "LORA_SET", "EMAIL", "WIFI_SSID", "MQTT_HOST", "MQTT_PORT", "MQTT_ID", "MQTT_USER", "MQTT_PFIX", "UUID"};
  String response = "{\"HOSTNAME\":\"" + String(apSSID) + "\",";
  response += "\"FW_VER\":\"" + String(round2(fw_ver / 100.0)) + "\",";
  for (size_t i = 0; i < sizeof(basicParameters) / sizeof(basicParameters[0]); i++) {
    int retVarType, retVarNum;
    if (findInConfig(basicParameters[i], retVarType, retVarNum)) {
      String foundInConfig = returnConfigVar(basicParameters[i], retVarType, retVarNum, 0);
      if (foundInConfig != "") {
        response += foundInConfig.substring(1, foundInConfig.length() - 1);
        response += ",";
      }
    }
  }
  if (response.endsWith(",")) response = response.substring(0, response.length() - 1);
  response += "}";
  return response;
}
