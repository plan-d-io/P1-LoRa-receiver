void setupMqtt() {
  String mqttinfo = "MQTT enabled! Will connect as " + _mqtt_id;
  if (_mqtt_auth) {
    mqttinfo = mqttinfo + " using authentication, with username " + _mqtt_user;
  }
  Serial.println(mqttinfo);
  mqttclient.setClient(wificlient);
  mqttclient.setKeepAlive(300).setSocketTimeout(300);
  /*Set broker location*/
  IPAddress addr;
  if (_mqtt_host.length() > 0) {
    if (addr.fromString(_mqtt_host)) {
      Serial.println("MQTT host has IP address " + _mqtt_host);
      mqttclient.setServer(addr, _mqtt_port);
      mqttclient.setCallback(callback);
    }
    else {
      return;
    }
  }
}

void connectMqtt() {
  // Loop until we're (re)connected
  int mqttretry = 0;
  bool disconnected = false;
  if(!mqttclient.connected()) {
    disconnected = true;
    if(mqttWasConnected){
      Serial.println("Lost connection to MQTT broker");
    }
    Serial.println("Trying to connect to MQTT broker");
    while(!mqttclient.connected() && mqttretry < 2){
      Serial.print("...");
      String mqtt_topic = "data/devices/utility_meter";
      if (_mqtt_auth) mqttclient.connect(_mqtt_id.c_str(), _mqtt_user.c_str(), _mqtt_pass.c_str(), mqtt_topic.c_str(), 1, true, "offline");
      else mqttclient.connect(_mqtt_id.c_str(), "data/devices/utility_meter", 1, true, "offline");
      mqttretry++;
      reconncount++;
      delay(250);
    }
    Serial.println("");
  }
  if(disconnected){
    if(mqttretry < 2){
      Serial.println("Connected to MQTT broker");
      mqttclient.publish("data/devices/utility_meter", "online", true);
      mqttclient.subscribe("set/devices/utility_meter/reboot");
      if(debugInfo && !mqttWasConnected){
        //hadebugDevice(true);
        delay(500);
        //hadebugDevice(false);
        delay(500);
        //getHeapDebug();
      }
      mqttWasConnected = true;
      reconncount = 0;
    }
    else{
      Serial.println("Failed to connect to MQTT broker");
    }
  }
}

bool pubMqtt(String topic, String payload, boolean retain){
  bool pushed = false;
  if(_wifi_en && _mqtt_en && !wifiError){
    if(mqttclient.connected()){
      if(mqttclient.publish(topic.c_str(), payload.c_str(), retain)){
        mqttPushFails = 0;
        pushed = true;
      }
      else mqttPushFails++;
    }
  }
  return pushed;
}

void callback(char* topic, byte* payload, unsigned int length) {
  time_t now;
  unsigned long dtimestamp = time(&now);
  Serial.print("got mqtt message on ");
  Serial.print(String(topic));
  String messageTemp;
  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }
  Serial.print(", ");
  Serial.println(messageTemp);
  if (String(topic) == "set/devices/utility_meter/reboot") {
    StaticJsonDocument<200> doc;
    deserializeJson(doc, messageTemp);
    if(doc["value"] == "true"){
      //if(saveConfig()){
        Serial.println("Reboot requested from MQTT");
        pubMqtt("set/devices/utility_meter/reboot", "{\"value\": \"false\"}", false);
        delay(500);
        //setReboot();
      //}
    }
  }
}

static const keyConfig dsmrKeys[] PROGMEM = {
    { "1-0:1.8.1", 999999, "energy", "Total consumption T1",  "",  false},
    { "1-0:1.8.2", 999999, "energy", "Total consumption T2",  "",  false},
    { "1-0:2.8.1", 999999, "energy", "Total injection T1",  "",  false},
    { "1-0:2.8.2", 999999, "energy", "Total injection T2",  "",  false},
    { "1-0:1.7.0", 99, "power", "Active power consumption",  "",  false},
    { "1-0:2.7.0", 99, "power", "Active power injection",  "",  false},
    { "1-0:1.4.0", 99, "power", "Current average demand",  "",  false},
    { "1-0:1.6.0", 99, "power", "Maximum demand current month",  "",  false},
    { "1-0:32.7.0", 500, "voltage", "Voltage phase 1", "",  false},
    { "1-0:31.7.0", 50, "current", "Current phase 1", "",  false},
    { "0-1:24.2.3", 999999, "gas", "Natural gas consumption", "",  false},
    { "0-1:24.2.1", 999999, "water", "Water consumption", "",  false},
    { "1-0:21.7.0", 99, "power", "L1 Active power consumption",  "",  false},
    { "1-0:41.7.0", 99, "power", "L2 Active power consumption",  "",  false},
    { "1-0:61.7.0", 99, "power", "L3 Active power consumption",  "",  false},
    { "1-0:22.7.0", 99, "power", "L1 Active power injection",  "",  false},
    { "1-0:42.7.0", 99, "power", "L2 Active power injection",  "",  false},
    { "1-0:62.7.0", 99, "power", "L3 Active power injection",  "",  false},
    { "1-0:52.7.0", 500, "voltage", "Voltage phase 2", "",  false},
    { "1-0:72.7.0", 500, "voltage", "Voltage phase 3", "",  false},
    { "1-0:51.7.0", 50, "current", "Current phase 2", "",  false},
    { "1-0:71.7.0", 50, "current", "Current phase 3", "",  false},
};

void pushMqtt(){
  float totCon = meterData[0] + meterData[1];
  float totIn = meterData[2] + meterData[3];
  float netPowCon = meterData[4] - meterData[5];
  int packetRSSI = LoRa.packetRssi();
  float packetSNR = LoRa.packetSnr();
  int packetSF = int(setSF);
  for(int i = 0; i <6; i++){
    String jsonOutput = "";
    String totalsTopic = "";
    DynamicJsonDocument totals(128);
    if(i == 0){
      if(totCon < 999999.9 && totCon > -1.0){
        if(fmodf(totCon, 1.0) == 0) totals["value"] = int(totCon);
        else totals["value"] = round2(totCon);
        totalsTopic = _mqtt_prefix;
        totalsTopic += "total_energy_consumed";
        serializeJson(totals, jsonOutput);
        pubMqtt(totalsTopic, jsonOutput, false);
      }
    }
    else if(i == 1){
      if(totIn < 999999.9 && totIn > -1.0){
        if(fmodf(totIn, 1.0) == 0) totals["value"] = int(totIn);
        else totals["value"] = round2(totIn);
        totalsTopic = _mqtt_prefix;
        totalsTopic += "total_energy_injected";
        serializeJson(totals, jsonOutput);
        pubMqtt(totalsTopic, jsonOutput, false);
      }
    }
    else if(i == 2){
      if(netPowCon < 99.0 && netPowCon > -99.0){
        if(fmodf(netPowCon, 1.0) == 0) totals["value"] = int(netPowCon);
        else totals["value"] = round2(netPowCon);
        totalsTopic = _mqtt_prefix;
        totalsTopic += "total_active_power";
        serializeJson(totals, jsonOutput);
        pubMqtt(totalsTopic, jsonOutput, false);
      }
    }
    else if(i == 3){
      totals["value"] = packetRSSI;
      totalsTopic = _mqtt_prefix;
      totalsTopic += "rssi";
      serializeJson(totals, jsonOutput);
      pubMqtt(totalsTopic, jsonOutput, false);
    }
    else if(i == 4){
      if(fmodf(packetSNR, 1.0) == 0) totals["value"] = int(packetSNR);
      else totals["value"] = round2(packetSNR);
      totalsTopic = _mqtt_prefix;
      totalsTopic += "snr";
      serializeJson(totals, jsonOutput);
      pubMqtt(totalsTopic, jsonOutput, false);
    }
    else if(i == 5){
      totals["value"] = packetSF;
      totalsTopic = _mqtt_prefix;
      totalsTopic += "spreading_factor";
      serializeJson(totals, jsonOutput);
      pubMqtt(totalsTopic, jsonOutput, false);
    }
    else if(i == 5){
      totals["value"] = packetLoss;
      totalsTopic = _mqtt_prefix;
      totalsTopic += "packet_lossç";
      serializeJson(totals, jsonOutput);
      pubMqtt(totalsTopic, jsonOutput, false);
    }
    Serial.print(totalsTopic);
    Serial.print(" ");
    Serial.println(jsonOutput);
  }
  String jsonOutput = "";
  /*Perform HA autodiscovery a few times*/
  if(mqttPushCount > 1 && mqttPushCount <4){
    jsonOutput ="";
    for(int i = 0; i <7; i++){
      DynamicJsonDocument doci(1024);
      String friendlyName = "Utility meter ";
      String tempTopic = _mqtt_prefix;
      if(i == 0){
        friendlyName +=  "Total energy consumed";
        tempTopic += "total_energy_consumed";
        doci["device_class"] = "energy";
        doci["unit_of_measurement"] = "kWh";
        doci["state_class"] = "total_increasing";
      }
      else if(i == 1){
        friendlyName +=  "Total energy injected";
        tempTopic += "total_energy_injected";
        doci["device_class"] = "energy";
        doci["unit_of_measurement"] = "kWh";
        doci["state_class"] = "total_increasing";
      }
      else if(i == 2){
        friendlyName +=  "Total active power";
        tempTopic += "total_active_power";
        doci["device_class"] = "power";
        doci["unit_of_measurement"] = "kW";
        doci["state_class"] = "measurement";
      }
      else if(i == 3){
        friendlyName +=  "RSSI";
        tempTopic += "rssi";
        doci["device_class"] = "signal_strength";
        doci["unit_of_measurement"] = "dBm";
        doci["state_class"] = "measurement";
      }
      else if(i == 4){
        friendlyName +=  "SNR";
        tempTopic += "snr";
        doci["icon"] = "mdi:wifi-strength-3";
        doci["state_class"] = "measurement";
      }
      else if(i == 5){
        friendlyName +=  "Spreading factor";
        tempTopic += "spreading_factor";
        doci["icon"] = "mdi:wifi-strength-3";
        doci["state_class"] = "measurement";
      }
      else if(i == 6){
        friendlyName +=  "Packet loss";
        tempTopic += "spreading_factor";
        doci["icon"] = "mdi:antenna";
        doci["unit_of_measurement"] = "%";
        doci["state_class"] = "measurement";
      }
      doci["name"] = friendlyName;
      doci["state_topic"] =  tempTopic;
      friendlyName.replace(" ", "_");
      friendlyName.toLowerCase();
      doci["unique_id"] = friendlyName;
      doci["object_id"] = friendlyName;
      doci["value_template"] = "{{ value_json.value }}";
      doci["availability_topic"] = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
      JsonObject device  = doci.createNestedObject("device");
      JsonArray identifiers = device.createNestedArray("identifiers");
      identifiers.add("P1_utility_meter");
      device["name"] = haDeviceName;
      device["model"] = "P1 LoRa dongle for DSMR compatible utility meters";
      device["manufacturer"] = "plan-d.io";
      String configTopic = "homeassistant/sensor/" + friendlyName + "/config";
      serializeJson(doci, jsonOutput);
      pubMqtt(configTopic, jsonOutput, true);
      Serial.print(configTopic);
      Serial.print(" ");
      Serial.println(jsonOutput);
      configTopic = "";
      jsonOutput = "";
    }
  }
  int loopSize;
  if(payloadLength >= 24) loopSize = 22;
  else loopSize = payloadLength;
  for(int i = 0; i< loopSize; i++){
    jsonOutput = "";
    DynamicJsonDocument doc(128);
    String tempTopic = _mqtt_prefix;
    if(meterData[i] < dsmrKeys[i].maxVal && meterData[i] > -1){
      if(fmodf(meterData[i], 1.0) == 0) doc["value"] = int(meterData[i]);
      else doc["value"] = round2(meterData[i]);
      tempTopic += dsmrKeys[i].keyName;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      serializeJson(doc, jsonOutput);
      pubMqtt(tempTopic, jsonOutput, false);
      Serial.print(tempTopic);
      Serial.print(" ");
      Serial.println(jsonOutput);
    }
    /*Erase HA autodiscovered entity*/
    if(mqttPushCount < 1){
      tempTopic = dsmrKeys[i].keyName;
      tempTopic.replace(" ", "_");
      tempTopic.toLowerCase();
      tempTopic = "homeassistant/sensor/" + tempTopic + "/config";
      String payload = "";
      Serial.print("Erasing ");
      Serial.println(tempTopic);
      pubMqtt(tempTopic, payload, false); 
    }
    else if(mqttPushCount > 1 && mqttPushCount <4){
      jsonOutput = "";
      DynamicJsonDocument doc(1024);
      String friendlyName = "Utility meter " + dsmrKeys[i].keyName;
      doc["name"] = friendlyName;
      doc["state_topic"] = tempTopic;
      doc["device_class"] = dsmrKeys[i].deviceType;
      if(dsmrKeys[i].deviceType == "power") doc["unit_of_measurement"] = "kW";
      if(dsmrKeys[i].deviceType == "energy") doc["unit_of_measurement"] = "kWh";
      if(dsmrKeys[i].deviceType == "voltage") doc["unit_of_measurement"] = "V";
      if(dsmrKeys[i].deviceType == "current") doc["unit_of_measurement"] = "A";
      if(dsmrKeys[i].deviceType == "gas" || dsmrKeys[i].deviceType == "water") doc["unit_of_measurement"] = "m³";
      if(dsmrKeys[i].deviceType == "energy" || dsmrKeys[i].deviceType == "gas" || dsmrKeys[i].deviceType == "water") doc["state_class"] = "total_increasing";
      else doc["state_class"] = "measurement";
      friendlyName.replace(" ", "_");
      friendlyName.toLowerCase();
      doc["unique_id"] = friendlyName;
      doc["object_id"] = friendlyName;
      doc["value_template"] = "{{ value_json.value }}";
      doc["availability_topic"] = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
      JsonObject device  = doc.createNestedObject("device");
      JsonArray identifiers = device.createNestedArray("identifiers");
      identifiers.add("P1_utility_meter");
      device["name"] = haDeviceName;
      device["model"] = "P1 LoRa dongle for DSMR compatible utility meters";
      device["manufacturer"] = "plan-d.io";
      //device["configuration_url"] = "http://" + WiFi.localIP().toString();
      //device["sw_version"] = String(fw_ver/100.0);
      String configTopic = "homeassistant/sensor/" + friendlyName + "/config";
      serializeJson(doc, jsonOutput);
      pubMqtt(configTopic, jsonOutput, true);
      Serial.print("Registering ");
      Serial.print(configTopic);
      Serial.print(" ");
      Serial.println(jsonOutput);     
    }
  }
  mqttPushCount++;
  if(mqttPushCount > 21) mqttPushCount = 2;
}

double round2(double value) {
   return (int)(value * 100 + 0.05) / 100.0;
}
