/*Add your custom functions to process meter readings here*/

void onTelegram(){
  /*This function is executed whenever a meter telegram containing a meterID and meterTimestamp is received.
   * Use this function if you want the most recent meter updates
   */
  printTelegramValues();
  controlHA();
  mqttPushTelegramValues();
}

void onTelegramCrc(){
  /*This function is executed whenever a meter telegram containing a meterID, a meterTimestamp and a valid CRC is received.
   * Sometimes a meter telegram containing valid measurements can be reveived, but the CRC can be off.
   * Use this function if you want absolute assurance about the telegram contents.
   */
}

void printTelegramValues(){
  /*Example function which loops over both the configured DSMR keys and the detected Mbus keys and, if found, prints their value.
   * This function shows you how you can access these keys and their values.
   */
  Serial.print("Meter ID: ");
  Serial.println(meterId);
  Serial.print("Meter time: ");
  Serial.print(meterTimestamp);
  Serial.print(" ");
  struct tm meterTime;
  localtime_r(&meterTimestamp , &meterTime);
  char timeStringBuff[30];
  strftime(timeStringBuff,sizeof(timeStringBuff),"%A, %B %d %Y %H:%M:%S", &meterTime);
  Serial.println(timeStringBuff);
  /*Loop over the configured DSMR keys and, if present in the received meter telegram, print them*/ 
  for(int i = 0; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){
    if(*dsmrKeys[i].keyFound == true){
      Serial.print(dsmrKeys[i].keyName);
      Serial.print(": ");
      if(dsmrKeys[i].keyType > 0 && dsmrKeys[i].keyType < 4){
        if(fmodf(*dsmrKeys[i].keyValueFloat, 1.0) == 0) Serial.print(int(*dsmrKeys[i].keyValueFloat));
        else Serial.print(round2(*dsmrKeys[i].keyValueFloat));
        Serial.print(" ");
      }
      if(dsmrKeys[i].keyType == 2 || dsmrKeys[i].keyType == 3){
        if(dsmrKeys[i].deviceType == "energy") Serial.print("kWh ");
        else if(dsmrKeys[i].deviceType == "power") Serial.print("kW ");
        else if(dsmrKeys[i].deviceType == "power") Serial.print("kW ");
        else if(dsmrKeys[i].deviceType == "voltage") Serial.print("V ");
        else if(dsmrKeys[i].deviceType == "current") Serial.print("A ");
        else if(dsmrKeys[i].deviceType == "signal_strength") Serial.print("dBm ");
        else Serial.print(" ");
      }
      if(dsmrKeys[i].keyType == 3){
        Serial.print("(");
        Serial.print(*dsmrKeys[i].keyValueLong);
        Serial.print(")");
      }
      if(dsmrKeys[i].keyType == 4){
        Serial.print(*dsmrKeys[i].keyValueString);
      }
      if(dsmrKeys[i].keyType == 5){
        Serial.print(*dsmrKeys[i].keyValueLong);
      }
      Serial.println("");
    }
  }
  /*Loop over the Mbus keys found in the meter telegram and print their value*/
  for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
    if(mbusMeter[i].keyFound == true){
      if(mbusMeter[i].type == 3) Serial.print("Natural gas consumption: ");
      else if (mbusMeter[i].type == 4) Serial.print("Heat consumption: ");
      else if (mbusMeter[i].type == 7) Serial.print("Water consumption: ");
      if(fmodf(mbusMeter[i].keyValueFloat, 1.0) == 0) Serial.print(int(mbusMeter[i].keyValueFloat));
      else Serial.print(round2(mbusMeter[i].keyValueFloat));
      if(mbusMeter[i].type == 3 || mbusMeter[i].type == 7) Serial.print(" m³ (");
      else if (mbusMeter[i].type == 7) Serial.print(" kWh (");
      Serial.print(mbusMeter[i].keyTimeStamp);
      Serial.println(")");
    }
  }
}

void mqttPushTelegramValues(){
  if(_mqtt_en){
    if(sinceLastUpload > _upload_throttle*1000){
      Serial.println(ESP.getFreeHeap()/1000.0);
      if(mqttDebug) Serial.println("Performing MQTT push");
      String availabilityTopic = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
      pubMqtt(availabilityTopic, "online", false);
      /*Loop over the configured DSMR keys and, if found in the meter telegram and enabled, push their value over MQTT*/
      for(int i = 0; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){
        if(*dsmrKeys[i].keyFound == true){
          /*Check if the key,value pair needs to be pushed
           * The unsigned long _key_pushlist contains 32 bits indicating if the value needs to be pushed (1) or not (0)
           * E.g. if _key_pushlist is 329, its binary representation is 00000000000000000000000101001001
           * The LSB (at the rightmost side) represents dsmrKeys[0], the bit to the left of it dsmrKeys[1] etc.
           * We boolean AND the _key_pushlist with a binary mask 1, which is shifted to the left according to which key in dsmrKeys[] we are comparing
           * If the result is 1, this means the key,value pair must be pushed
           * E.g. if _key_pushlist is 329, dsmrKeys[0] must be pushed, dsmrKeys[1] not, dsmrKeys[2] not, dsmrKeys[3] must, etc.
           * Sounds complicated, but this way we only need one variable in NVS to store up to 32 options.
           */
          unsigned long mask = 1;
          mask <<= i;
          unsigned long test = _key_pushlist & mask;
          if(test > 0){
            if(mqttDebug){
              Serial.print(dsmrKeys[i].keyName);
              Serial.print(" ");
              Serial.println(dsmrKeyPayload(i));
            }
            pushDSMRKey(dsmrKeys[i].keyName, dsmrKeyPayload(i), "");
          }
        }
      }
      /*Loop over the Mbus keys found in the meter telegram and push their value over MQTT*/
      for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
        String friendlyName;
        if(mbusMeter[i].keyFound == true && mbusMeter[i].enabled == true){
          String tempJson = mbusKeyPayload(i);
          for(int j = 0; j < sizeof(mbusKeys)/sizeof(mbusKeys[0]); j++){
            if(mbusKeys[j].keyType == mbusMeter[i].type){
              friendlyName = mbusKeys[j].keyName;
            }
          }
          if(mqttDebug){
            Serial.print(friendlyName);
            Serial.print(" ");
            Serial.println(tempJson);
          }
          pushDSMRKey(friendlyName, tempJson, "");
        }
      }
      sinceLastUpload = 0;
    }
  }
}

String httpTelegramValues(String option){
  String jsonOutput = "[";
  /*Get the most important measurements*/
  if(option == "basic"){
    int keys[] = {0, 1, 2, 13, 14};
    for(int i = 0; i < 5; i++){
      String tempJson = dsmrKeyPayload(keys[i]);
      tempJson += ",";
      jsonOutput += tempJson;
    }
    for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
      if(mbusMeter[i].keyFound == true){
        String tempJson = mbusKeyPayload(i);
        tempJson += ",";
        jsonOutput += tempJson;
      }
    }
  }
  /*Otherwise, return all enabled keys found in the telegram*/
  else{
    for(int i = 0; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){
      if(option == "all"){
        String tempJson = dsmrKeyPayload(i);
        tempJson += ",";
        jsonOutput += tempJson;
      }
      else{
        if(*dsmrKeys[i].keyFound == true){
          unsigned long mask = 1;
          mask <<= i;
          unsigned long test = _key_pushlist & mask;
          if(test > 0){
            String tempJson = dsmrKeyPayload(i);
            tempJson += ",";
            jsonOutput += tempJson;
          }
        }
      }
    }
    for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
      if(mbusMeter[i].keyFound == true){
        String tempJson = mbusKeyPayload(i);
        tempJson += ",";
        jsonOutput += tempJson;
      }
    }
  }
  jsonOutput = jsonOutput.substring(0, jsonOutput.length()-1);
  jsonOutput += "]";
  return jsonOutput;
}

bool pushDSMRKey(String friendlyName, String payload, String mqttTopic){
  String tempTopic = _mqtt_prefix;
  String jsonOutput;
  bool pushSuccess = false;
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
  pushSuccess = pubMqtt(tempTopic, payload, false);
  if(mqttDebug && pushSuccess){
    Serial.print(tempTopic);
    Serial.print(" ");
    Serial.println(payload);
    Serial.println("");
  }
  else{
    if(mqttDebug) Serial.println("Could not make MQTT push");
  }
  return pushSuccess;
}

String formatPayload(String key, float measurementValue, String measurementUnit, time_t measurementTimestamp, String deviceType, String friendlyName, String mqttTopic, String rawKey){
  String jsonOutput;
  if(_payload_format > 0){ //minimal JSON payload format
    DynamicJsonDocument doc(1024);
    if(measurementValue == 0 && rawKey != ""){
      doc["value"] = rawKey;
    }
    else{
      if(fmodf(measurementValue, 1.0) == 0) doc["value"] = int(measurementValue);
      else doc["value"] = round2(measurementValue);
    }
    if(measurementUnit != "") doc["unit"] = measurementUnit;
    doc["timestamp"] = measurementTimestamp;
    if(_payload_format > 1){ //standard JSON payload format
      //friendlyName.toLowerCase();
      String sensorId = _ha_device + "." + friendlyName;
      friendlyName = friendlyName;
      doc["friendly_name"] = friendlyName;
      sensorId.toLowerCase();
      sensorId.replace(" ", "_");
      doc["sensorId"] = sensorId;
      if(_payload_format > 2){ //COFY payload format
        doc["entity"] = _ha_device;
        if(friendlyName == "Total energy consumed"){
          doc["metric"] = "GridElectricityImport";
          doc["metricKind"] = "cumulative";
        }
        else if(friendlyName == "Total energy injected"){
          doc["metric"] = "GridElectricityExport";
          doc["metricKind"] = "cumulative";
        }
        else if(friendlyName == "Total active power"){
          doc["metric"] = "GridElectricityPower";
          doc["metricKind"] = "gauge";
        }
        else{
          for(int k = 0; k < sizeof(cofyKeys)/sizeof(cofyKeys[0]); k++){
            if(key == cofyKeys[k][0]){
              if(cofyKeys[k][1] != "") doc["metric"] = cofyKeys[k][1];
              doc["metricKind"] = cofyKeys[k][2];
            }
          }
        }
      }
    }
    serializeJson(doc, jsonOutput);
  }
  else{
    if(fmodf(measurementValue, 1.0) == 0) jsonOutput = String(int(measurementValue));
    else jsonOutput = String(round2(measurementValue));
  }
  return jsonOutput;
}

String dsmrKeyPayload(int i){
  if(dsmrKeys[i].keyType == 0 || dsmrKeys[i].keyType == 4){
    /*Other or string value (currently handled the same)*/
    return formatPayload(dsmrKeys[i].dsmrKey, 0, "", meterTimestamp, dsmrKeys[i].deviceType, dsmrKeys[i].keyName, dsmrKeys[i].keyTopic, *dsmrKeys[i].keyValueString);
  }
  else if(dsmrKeys[i].keyType == 1 || dsmrKeys[i].keyType == 5){
    /*Numeric value with no unit*/
    if(dsmrKeys[i].keyType == 1) return formatPayload(dsmrKeys[i].dsmrKey, *dsmrKeys[i].keyValueFloat, "", meterTimestamp, dsmrKeys[i].deviceType, dsmrKeys[i].keyName, dsmrKeys[i].keyTopic, "");
    if(dsmrKeys[i].keyType == 5) return formatPayload(dsmrKeys[i].dsmrKey, *dsmrKeys[i].keyValueLong, "", meterTimestamp, dsmrKeys[i].deviceType, dsmrKeys[i].keyName, dsmrKeys[i].keyTopic, "");
  }
  else if(dsmrKeys[i].keyType == 2){
    /*Numeric value with unit*/
    String unit;
    if(dsmrKeys[i].deviceType == "energy") unit = "kWh";
    else if(dsmrKeys[i].deviceType == "power") unit = "kW";
    else if(dsmrKeys[i].deviceType == "voltage") unit = "V";
    else if(dsmrKeys[i].deviceType == "current") unit = "A";
    return formatPayload(dsmrKeys[i].dsmrKey, *dsmrKeys[i].keyValueFloat, unit, meterTimestamp, dsmrKeys[i].deviceType, dsmrKeys[i].keyName, dsmrKeys[i].keyTopic, "");
  }
  else if(dsmrKeys[i].keyType == 3){
    /*timestamped (Mbus) message*/
    String unit;
    if(dsmrKeys[i].deviceType == "energy") unit = "kWh";
    else if(dsmrKeys[i].deviceType == "power") unit = "kW";
    else if(dsmrKeys[i].deviceType == "voltage") unit = "V";
    else if(dsmrKeys[i].deviceType == "current") unit = "A";
    return formatPayload(dsmrKeys[i].dsmrKey, *dsmrKeys[i].keyValueFloat, unit, *dsmrKeys[i].keyValueLong, dsmrKeys[i].deviceType, dsmrKeys[i].keyName, dsmrKeys[i].keyTopic, "");
  }
  else{
    if(telegramDebug) Serial.print("undef");
    return "";
  }
}

String mbusKeyPayload(int i){
  String measurementUnit;
  String deviceType;
  String friendlyName;
  String mqttTopic;
  for(int z = 0; z < sizeof(mbusKeys)/sizeof(mbusKeys[0]); z++){
    if(mbusKeys[z].keyType == mbusMeter[i].type){
      friendlyName = mbusKeys[z].keyName;
      deviceType = mbusKeys[z].deviceType;
      mqttTopic = mbusKeys[z].keyTopic;
    }
  }
  if(mbusMeter[i].type == 3 || mbusMeter[i].type == 7) measurementUnit = "m³";
  else if (mbusMeter[i].type == 7) measurementUnit = "kWh";
  return formatPayload(mbusMeter[i].mbusKey, mbusMeter[i].keyValueFloat, measurementUnit, mbusMeter[i].keyTimeStamp, deviceType, friendlyName, mqttTopic, "");
} 
