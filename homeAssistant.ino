/*Home Assistant functions, e.g. MQTT autodiscovery*/

void controlHA(){
  /*Control function for the Home Assistant integration
   * Performs MQTT autodiscovery once enough telegrams have been received to populate all data fields, and repeats this
   * every once in a while.
   */
  //If MQTT is not connected, we can't do much
  if(_mqtt_tls){
    if(!mqttclientSecure.connected()) return;
  }
  else{
    if(!mqttclient.connected()) return;
  }
  if(telegramAction < 4){
    haEraseDevice();                                            //make sure any existing device and sensor in HA is erased to prevent double entries
    hadebugDevice(true);
  }
  if(telegramAction == 4){
    haDiscovered = false;                                       //then, perform the MQTT autodiscovery untill succes
    hadebugDevice(false);
  }
  if(!haDiscovered && telegramAction > 4) doHaAutoDiscovery();
  if(telegramAction > 600) telegramAction = 4;                  //reperform the autodiscovery every once in a while
  telegramAction++;
}

void doHaAutoDiscovery() {
  /*Create an MQTT device "Utility meter" in Home Assistant and register the enabled DSMR keys as entities
   * by sending a HA autodiscovery payload for each key.
   * This only needs to be done once, but it can't hurt repeating it from time to time (e.g. to overcome broker reboots).
   */
  if(!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) {
    return;
  }
  if(_mqtt_tls){
    if(!mqttclientSecure.connected()) return;
  }
  else{
    if(!mqttclient.connected()) return;
  }
  for(int i = 0; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){
    if(dsmrKeys[i].keyFound){
      syslog("Performing Home Assistant MQTT autodiscovery for key " + dsmrKeys[i].keyName, 0);
      String keyUnit;
      if(dsmrKeys[i].deviceType == "energy") keyUnit = "kWh";
      else if(dsmrKeys[i].deviceType == "power") keyUnit = "kW";
      else if(dsmrKeys[i].deviceType == "voltage") keyUnit = "V";
      else if(dsmrKeys[i].deviceType == "current") keyUnit = "A";
      else if(dsmrKeys[i].deviceType == "signal_strength") keyUnit = "dBm";
      else if(dsmrKeys[i].deviceType == "gas" || dsmrKeys[i].deviceType == "water") keyUnit = "m³";
      else keyUnit = "";
      haAutoDiscovery(dsmrKeys[i].keyName, keyUnit, dsmrKeys[i].deviceType, dsmrKeys[i].keyTopic);
    }
  }
  for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
    String friendlyName;
    if(mbusMeter[i].keyFound == true){
      for(int j = 0; j < sizeof(mbusKeys)/sizeof(mbusKeys[0]); j++){
        if(mbusKeys[j].keyType == mbusMeter[i].type){
          String keyUnit;
          syslog("Performing Home Assistant MQTT autodiscovery for key " + mbusKeys[j].keyName, 0);
          if(mbusMeter[i].type == 3 || mbusMeter[i].type == 7) keyUnit = "m³";
          else if (mbusMeter[i].type == 7) keyUnit = "kWh";
          haAutoDiscovery(mbusKeys[j].keyName, keyUnit, mbusKeys[j].deviceType, mbusKeys[j].keyTopic);
        }
      }
    }
  }
  haDiscovered = true;
}


void haAutoDiscovery(String friendlyName, String unit, String deviceType, String mqttTopic) {
  if(!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) {
    return;
  }
  String jsonOutput;
  String tempTopic = _mqtt_prefix;
  if(mqttTopic == ""){           //Use key_name as mqtt topic if keyTopic left empty
    tempTopic += friendlyName;
    tempTopic.replace(" ", "_");
    tempTopic.toLowerCase();
  }
  else{
    tempTopic += mqttTopic;
    tempTopic.replace(" ", "_");
    tempTopic.toLowerCase();
  }
  DynamicJsonDocument doc(1024);
  doc["name"] = friendlyName;
  if(friendlyName == "Packet loss") unit = "%";
  if(deviceType != "") doc["device_class"] = deviceType;
  if(unit != "") doc["unit_of_measurement"] = unit;
  doc["state_topic"] = tempTopic;
  if(deviceType == "energy" || deviceType == "gas" || deviceType == "water") doc["state_class"] = "total_increasing";
  else doc["state_class"] = "measurement";
  if(friendlyName == "SNR" || friendlyName == "Spreading factor") doc["icon"] = "mdi:wifi-strength-3";
  if(friendlyName == "Packet loss") doc["icon"] = "mdi:antenna";
  friendlyName.replace(" ", "_");
  friendlyName.toLowerCase();  
  String deviceName = _ha_device;
  deviceName.replace(" ", "_");
  deviceName.toLowerCase();
  doc["unique_id"] = deviceName + "_" + friendlyName;
  doc["object_id"] = deviceName + "_" + friendlyName;
  if(_payload_format > 0) doc["value_template"] = "{{ value_json.value }}";
  doc["availability_topic"] = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
  JsonObject device  = doc.createNestedObject("device");
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(deviceName);
  device["name"] = _ha_device;
  device["model"] = "P1 LoRa dongle for DSMR compatible utility meters";
  device["manufacturer"] = "plan-d.io";
  device["configuration_url"] = "http://" + WiFi.localIP().toString();
  device["sw_version"] = String(fw_ver/100.0);
  String configTopic = "homeassistant/sensor/" + deviceName + "_" + friendlyName + "/config";
  serializeJson(doc, jsonOutput);
  bool pushSuccess = pubMqtt(configTopic, jsonOutput, true);
  if(mqttDebug && pushSuccess){
    Serial.print(configTopic);
    Serial.print(" ");
    serializeJson(doc, Serial);
    Serial.println("");
  }
  if(mqttPushCount < 4) delay(100);
}

void haEraseDevice(){
  /*This function erases all the existing HA MQTT entities generated by the dongle by sending an empty payload to their respective topics.
   * This prevents duplicate entities after a reboot, and ensures the HA entities always reflect the most recent state (e.g. after a firmware update
   * that changes certain parameters of the entities).
   */
  if(!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) {
    return;
  }
  if(_mqtt_tls){
    if(!mqttclientSecure.connected()) return;
  }
  else{
    if(!mqttclient.connected()) return;
  }
  syslog("Erasing Home Assistant MQTT autodiscovery entries", 0);
  for(int i = 0; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){ //erase all the DSMR keys
    String tempTopic = "homeassistant/sensor/";
    String deviceName = _ha_device;
    deviceName += " ";
    tempTopic += deviceName;
    //if(dsmrKeys[i].keyTopic == ""){
      tempTopic += dsmrKeys[i].keyName;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
    //}
    /*else{
      tempTopic += dsmrKeys[i].keyTopic;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
    }*/
    pubMqtt(tempTopic, "", false);
    if(mqttDebug){
      Serial.print("Erasing ");
      Serial.println(tempTopic);
    }
  }
  for(int i = 0; i < sizeof(mbusKeys)/sizeof(mbusKeys[0]); i++){  //erase all the mbus keys
    String tempTopic = "homeassistant/sensor/";
    String deviceName = _ha_device;
    deviceName += " ";
    tempTopic += deviceName;
    //if(mbusKeys[i].keyTopic == ""){ 
      tempTopic += mbusKeys[i].keyName;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
    //}
    /*else{
      tempTopic += mbusKeys[i].keyTopic;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
    }*/
    pubMqtt(tempTopic, "", false);
    if(mqttDebug){
      Serial.print("Erasing ");
      Serial.println(tempTopic);
    }
  }
}

void hadebugDevice(bool eraseMeter){
  if(!_ha_en || !_mqtt_en || mqttClientError || mqttHostError) {
    return;
  }
  if(_mqtt_tls){
    if(!mqttclientSecure.connected()) return;
  }
  else{
    if(!mqttclient.connected()) return;
  }
  Serial.println("performing autodisc");
  //if(eraseMeter) syslog("Erasing Home Assistant MQTT debug entries", 0);
  //else syslog("Performing Home Assistant MQTT debug autodiscovery", 0);
  for(int i = 0; i < 12; i++){
    String chanName = "";
    DynamicJsonDocument doc(1024);
    if(i == 0){
      chanName = String(apSSID) + "_reboots";
      doc["name"] = "Reboots";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/reboots";
    }
    else if(i == 1){
      chanName = String(apSSID) + "_last_reset_reason_hw";
      doc["name"] = "Last reset reason (hardware)";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/last_reset_reason_hw";
    }
    else if(i == 2){
      chanName = String(apSSID) + "_free_heap_size";
      doc["name"] = "Free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/free_heap_size";
    }
    else if(i == 3){
      chanName = String(apSSID) + "_max_allocatable_block";
      doc["name"] = "Allocatable block size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/max_allocatable_block";
    }
    else if(i == 4){
      chanName = String(apSSID) + "_min_free_heap";
      doc["name"] = "Lowest free heap size";
      doc["unit_of_measurement"] = "kB";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/min_free_heap";
    }
    else if(i == 5){
      chanName = String(apSSID) + "_last_reset_reason_fw";
      doc["name"] = "Last reset reason (firmware)";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/last_reset_reason_fw";
    }
    else if(i == 6){
      chanName = String(apSSID) + "_syslog";
      doc["name"] = "Syslog";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/syslog";
    }
    else if(i == 7){
      chanName = String(apSSID) + "_ip";
      doc["name"] = "IP";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/ip";
    }
    else if(i == 8){
      chanName = String(apSSID) + "_firmware";
      doc["name"] = "Firmware";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/firmware";
    }
    else if(i == 9){
      chanName = String(apSSID) + "_release_channel";
      doc["name"] = "Release channel";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/release_channel";
    }
    else if(i == 10){
      chanName = String(apSSID) + "_reboot";
      doc["name"] = "Reboot";
      doc["state_topic"] = "sys/devices/" + String(apSSID) + "/reboot";
      doc["payload_on"] = "{\"value\": \"on\"}";
      doc["payload_off"] = "{\"value\": \"off\"}";
      doc["state_on"] = "on";
      doc["state_off"] = "off";
      doc["value_template"] = "{{ value_json.value }}";
      String tempTopic = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
      tempTopic += "/set/reboot";
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      doc["command_topic"] = tempTopic;
      doc["icon"] = "mdi:restart";
    }
    else if(i == 11){
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
      String tempTopic = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
      tempTopic += "/set/loraset";
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      doc["command_topic"] = tempTopic;
      doc["icon"] = "mdi:antenna";
    }
    doc["unique_id"] = chanName;
    doc["object_id"] = chanName;
    doc["availability_topic"] = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
    doc["value_template"] = "{{ value_json.value }}";
    JsonObject device  = doc.createNestedObject("device");
    JsonArray identifiers = device.createNestedArray("identifiers");
    identifiers.add(apSSID);
    device["name"] = apSSID;
    device["model"] = "P1 LoRa dongle debug monitoring";
    device["manufacturer"] = "plan-d.io";
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    device["sw_version"] = String(fw_ver/100.0);
    String configTopic = "";
    if(i == 10) configTopic = "homeassistant/switch/" + chanName + "/config";
    else if(i == 11) configTopic = "homeassistant/select/" + chanName + "/config";
    else configTopic = "homeassistant/sensor/" + chanName + "/config";
    String jsonOutput ="";
    //Ensure devices are erased before created again
    if(eraseMeter){
      if(chanName.length() > 0) pubMqtt(configTopic, "", true);
      if(mqttDebug){
        Serial.print("Erasing ");
        Serial.println(configTopic);
      }
    }
    serializeJson(doc, jsonOutput);
    if(!eraseMeter){
      bool pushSuccess;
      if(chanName.length() > 0) {
        pushSuccess = pubMqtt(configTopic, jsonOutput, true);
        if(mqttDebug && pushSuccess){
          Serial.println("");
          Serial.print(configTopic);
          Serial.print(" ");
          serializeJson(doc, Serial);
        }
      }
    }
    if(mqttPushCount < 4) delay(100);
  }
  pubMqtt("sys/devices/" + String(apSSID) + "/reboot", "{\"value\": \"off\"}", false);
  
  pubMqtt("sys/devices/" + String(apSSID) + "/loraset", "{\"value\": \"" + _loraset + "\"}", false);
}
