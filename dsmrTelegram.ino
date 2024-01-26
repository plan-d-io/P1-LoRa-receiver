/*Configure and process the meter telegram*/

struct mbusMeterType {
      String mbusKey;
      int type = 0; //3 = gas meter, 4 = heat/cold, 7 = water meter
      int measurementType = 0; //1 = base value, 3 = non-temperature compensated
      String id;
      float keyValueFloat;
      unsigned long keyTimeStamp;
      bool enabled;
      bool keyFound;
};
mbusMeterType mbusMeter[4];

struct keyConfig {
  String dsmrKey;
  float* keyValueFloat;
  unsigned long* keyValueLong;
  String* keyValueString;
  uint8_t keyType;
  String deviceType;
  String  keyName;
  String  keyTopic;
  bool retain;
  bool* keyFound;
};

struct mbusConfig {
  int keyType; //3 = gas meter, 4 = heat/cold, 7 = water meter
  String deviceType;
  String  keyName;
  String  keyTopic;
  bool retain;
};

float dummyFloat, totConT1, totConT2, totCon, totInT1, totInT2, totIn, powCon, powIn, netPowCon, totGasCon, totWatCon, totHeatCon, volt1, volt2, volt3, current1, current2, current3, avgDem, maxDemM, powCon1, powCon2, powCon3, powIn1, powIn2, powIn3;
float prevtotConT1, prevtotConT2, prevtotCon, prevtotIntT1, prevtotIntT2, prevtotIn;
unsigned long dummyInt, actTarrif, maxDemTime, totGasTime, totWatTime, totHeatTime;
String dummyString, p1Version, meterId;
bool dummyBool, totConFound, totInFound, netPowConFound, totConT1Found, totConT2Found, totInT1Found, totInT2Found, actTarrifFound, powConFound, powInFound, avgDemFound, maxDemMFound, volt1Found, current1Found, volt2Found, volt3Found, current2Found, current3Found, powCon1Found, powCon2Found, powCon3Found, powIn1Found, powIn2Found, powIn3Found;
int spurCount;

static const keyConfig dsmrKeys[] PROGMEM = {
  /*The list of all possible DSMR keys and how to parse them.
   * { "DSMR key code", key type, "HA device type", "user-readable name", "mqtt topic", retain, key found in telegram }
   * Key types:
   *  0: other
   *  1: numeric (floating point) without unit, e.g. 0-0:96.14.0(0002)
   *  2: numeric (floating point) with unit, e.g. 1-0:1.4.0(02.351*kW)
   *  3: timestamped (Mbus) message, e.g. 0-1:24.2.3(200512134558S)(00112.384*m3)
   *  4: string, e.g. 0-0:96.13.0(3031323334353)
   *  5: integer
   *  The key type determines how the associated value will be parsed.
   * Home Assistant device types: see https://developers.home-assistant.io/docs/core/entity/sensor/#available-device-classes
   *  Examples: "" (none), power, energy, voltage, current, natural gas, water, heat.
   *  The device type determines how the value will be registered and represented in Home Assistant. You can leave this to "" (none) if you don't use HA.
   * Mqtt topic:
   *  The MQTT topic to post the measurement to. Will be prefixed by mqtt_prefix (standard: data/devices/utility_meter/).
   *  If left empty, the firmware will use the user readable name cast to lowercase with spaces replaced by _
   * Retain:
   *  If the MQTT payload should be sent with the retain flag.
   * IMPORTANT
   *  DSMR keys are adressed in the firmware by their order in this array. DO NOT rearrange keys.
   *  If you want to add new keys, add them to the bottom of the array.
   */
    /*The first three keys are custom keys, required for dongle operation*/
    { "A-0:0.0.1", &totCon, &dummyInt, &dummyString, 2, "energy", "Total energy consumed",  "",  false, &totConFound},
    { "A-0:0.0.2", &totIn, &dummyInt, &dummyString, 2, "energy", "Total energy injected",  "",  false, &totInFound},
    { "A-0:0.0.3", &netPowCon, &dummyInt, &dummyString, 2, "power", "Total active power",  "",  false, &netPowConFound},
    { "R-0:0.0.1", &packetRSSI, &dummyInt, &dummyString, 2, "signal_strength", "RSSI",  "",  false, &packetRSSIFound},
    { "R-0:0.0.2", &packetSNR, &dummyInt, &dummyString, 1, "", "SNR",  "",  false, &packetSNRFound},
    { "R-0:0.0.3", &packetSF, &dummyInt, &dummyString, 1, "", "Spreading factor",  "",  false, &packetSFFound},
    { "R-0:0.0.4", &packetLossf, &dummyInt, &dummyString, 1, "", "Packet loss",  "",  false, &packetLossFound},
    /*Standard keys (required)*/
    { "1-0:1.8.1", &totConT1, &dummyInt, &dummyString, 2, "energy", "Total consumption T1",  "",  false, &totConT1Found},
    { "1-0:1.8.2", &totConT2, &dummyInt, &dummyString, 2, "energy", "Total consumption T2",  "",  false, &totConT2Found},
    { "1-0:2.8.1", &totInT1, &dummyInt, &dummyString, 2, "energy", "Total injection T1",  "",  false, &totInT1Found},
    { "1-0:2.8.2", &totInT2, &dummyInt, &dummyString, 2, "energy", "Total injection T2",  "",  false, &totInT2Found},
    { "1-0:1.7.0", &powCon, &dummyInt, &dummyString, 2, "power", "Active power consumption",  "",  false, &powConFound},
    { "1-0:2.7.0", &powIn, &dummyInt, &dummyString, 2, "power", "Active power injection",  "",  false, &powInFound},
    { "1-0:1.4.0", &avgDem, &dummyInt, &dummyString, 2,"power", "Current average demand",  "",  false, &avgDemFound},
    { "1-0:1.6.0", &maxDemM, &maxDemTime, &dummyString, 3, "power", "Maximum demand current month",  "",  false, &maxDemMFound},
    { "1-0:32.7.0", &volt1, &dummyInt, &dummyString, 2, "voltage", "Voltage phase 1", "",  false, &volt1Found},
    { "1-0:31.7.0", &current1, &dummyInt, &dummyString, 2, "current", "Current phase 1", "",  false, &current1Found},
    /*Custom keys, add your own here (optional)*/
    { "1-0:21.7.0", &powCon1, &dummyInt, &dummyString, 2, "power", "L1 active power consumption", "",  false, &powCon1Found},
    { "1-0:41.7.0", &powCon2, &dummyInt, &dummyString, 2, "power", "L2 active power consumption", "",  false, &powCon2Found},
    { "1-0:61.7.0", &powCon3, &dummyInt, &dummyString, 2, "power", "L3 active power consumption", "",  false, &powCon3Found},
    { "1-0:22.7.0", &powIn1, &dummyInt, &dummyString, 2, "power", "L1 active power injection", "",  false, &powIn1Found},
    { "1-0:42.7.0", &powIn2, &dummyInt, &dummyString, 2, "power", "L2 active power injection", "",  false, &powIn2Found},
    { "1-0:62.7.0", &powIn3, &dummyInt, &dummyString, 2, "power", "L3 active power injection", "",  false, &powIn3Found},
    { "1-0:52.7.0", &volt2, &dummyInt, &dummyString, 2, "voltage", "Voltage phase 2", "",  false, &volt2Found},
    { "1-0:72.7.0", &volt3, &dummyInt, &dummyString, 2, "voltage", "Voltage phase 3", "",  false, &volt3Found},
    { "1-0:51.7.0", &current2, &dummyInt, &dummyString, 2, "current", "Current phase 2", "",  false, &current2Found},
    { "1-0:71.7.0", &current3, &dummyInt, &dummyString, 2, "current", "Current phase 3", "",  false, &current3Found}
    //{ "0-0:96.13.0", &dummyFloat, &dummyInt, &dummyString, 4, "", "Text message", "",  false, &dummyBool}
};

float gasValue, heatValue, waterValue;
unsigned long gasTime, heatTime, waterTime;
static const mbusConfig mbusKeys[] PROGMEM = {
  /*The availability and id of M-bus meters (water, gas, heat, ...) is dependent on your type of installation, so you can't set the Mbus OBIS keys to be extracted
   * from the telegram beforehand.
   * The dongle will automatically detect the type and id of connected M-bus meter and process the data according to the fields in this list (e.g. MQTT topic).
   * If the M-bus meter reports both temperature (base) and non-temperature corrected values, the temperature corrected value will be used.
   * If you want to extract a specific M-bus key, add it to the dsmrKeys list above.
   * Format: { Mbus meter type, HA device type, "user-readable name", "mqtt topic", retain }
   * Mqtt topic: see note in dsmrKeys above
   * Mbus meter types:
   *  3: natural gas
   *  4: heat/cold
   *  7: water
   * Note: multiple M-bus meters from the same type are currently not supported
   */
    { 3, "gas", "Natural gas consumption", "",  false},
    { 4, "", "heat", "",  false},
    { 7, "water", "Water consumption", "",  false}
};

void processMeterTelegram(){
  for(int i = 7; i < sizeof(dsmrKeys)/sizeof(dsmrKeys[0]); i++){
    *dsmrKeys[i].keyFound = false;
  }
  boolean meterIdFound = true;
  int i = 0;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[0])){
    totConT1 = meterData[0];
    totConT1Found = true;
  }
  else meterIdFound = false;
  i++;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[1])){
    totConT2 = meterData[1];
    totConT2Found = true;
  }
  else meterIdFound = false;
  i++;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[2])){
    totInT1 = meterData[2];
    totInT1Found = true;
  }
  else meterIdFound = false;
  i++;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[3])){
    totInT2 = meterData[3];
    totInT2Found = true;
  }
  else meterIdFound = false;
  i++;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[4])){
    powCon = meterData[4];
    powConFound = true;
  }
  else meterIdFound = false;
  i++;
  if(checkFloat(dsmrKeys[i+7].dsmrKey, dsmrKeys[i+7].deviceType, meterData[5])){
    powIn = meterData[5];
    powInFound = true;
  }
  else meterIdFound = false;
  i++;
  if(meterData[6] < 99.9 && meterData[6] > -1.0){
    avgDem = meterData[6];
    avgDemFound = true;
  }
  i++;
  if(meterData[7] < 99.9 && meterData[7] > -1.0){
    maxDemM = meterData[7];
    maxDemMFound = true;
  }
  i++;
  if(meterData[8] < 500.0 && meterData[8] > -1.0){
    volt1 = meterData[8];
    volt1Found = true;
  }
  i++;
  if(meterData[9] < 99.0 && meterData[9] > -1.0){
    current1 = meterData[9];
    current1Found = true;
  }
  i++;
  if(meterIdFound){
    spurCount = 0;
    /*Process minimum required readings*/
    float tempFloat = 0.0;
    if(totConT1Found && totConT2Found){
      tempFloat = totConT1 + totConT2;
      if(checkFloat("A-0:0.0.1", "energy", tempFloat)){
        totCon = tempFloat;
        totConFound = true;
      }
    }
    tempFloat = 0.0;
    if(totInT1Found && totInT2Found){
      tempFloat = totInT1 + totInT2;
      if(checkFloat("A-0:0.0.2", "energy", tempFloat)){
        totIn = tempFloat;
        totInFound = true;
      }
    }
    tempFloat = 0.0;
    if(powConFound && powInFound){
      tempFloat = powCon - powIn;
      if(checkFloat("A-0:0.0.3", "power", tempFloat)){
        netPowCon = tempFloat;
        netPowConFound = true; 
      }
    }
    meterTimestamp = getTime();
    sinceMeterCheck = 0;
    if(meterData[10] < 999999.9 && meterData[10] > -1.0){
      registerMbusMeter("0-1:24.2.3","3");
      parseMbus("0-1:24.2.3", meterData[10]);
    }
    if(meterData[11] < 999999.9 && meterData[11] > -1.0){
      registerMbusMeter("0-2:24.2.1", "7");
      parseMbus("0-2:24.2.1", meterData[11]);
    }
    if(payloadLength >= 24){
      if(meterData[12] < 99.0 && meterData[12] > -1.0){
        powCon1 = meterData[12];
        powCon1Found = true;
      }
      if(meterData[13] < 99.0 && meterData[13] > -1.0){
        powCon2 = meterData[13];
        powCon2Found = true;
      }
      if(meterData[14] < 99.0 && meterData[14] > -1.0){
        powCon3 = meterData[14];
        powCon3Found = true;
      }
      if(meterData[15] < 99.0 && meterData[15] > -1.0){
        powIn1 = meterData[15];
        powIn1Found = true;
      }
      if(meterData[16] < 99.0 && meterData[16] > -1.0){
        powIn2 = meterData[16];
        powIn2Found = true;
      }
      if(meterData[17] < 99.0 && meterData[17] > -1.0){
        powIn3 = meterData[17];
        powIn3Found = true;
      }
      if(meterData[18] < 500.0 && meterData[18] > -1.0){
        volt2 = meterData[18];
        volt2Found = true;
      }
      if(meterData[19] < 500.0 && meterData[19] > -1.0){
        volt3 = meterData[19];
        volt3Found = true;
      }
      if(meterData[20] < 99.9 && meterData[20] > -1.0){
        current2 = meterData[20];
        current2Found = true;
      }
      if(meterData[21] < 99.9 && meterData[21] > -1.0){
        current3 = meterData[21];
        current3Found = true;
      }
    }
    onTelegram();
    telegramCount++;
  }
  else spurCount++;
}

void parseMbus(String key, float splitValue){
  for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
    if(key == mbusMeter[i].mbusKey){  //check if the key matches a key stored in the mbusMeter array
      for(int j = 0; j < sizeof(mbusKeys)/sizeof(mbusKeys[0]); j++){  //if so, process the key according the parameters set for that key
        //Serial.println(value);
        if(mbusMeter[i].type == mbusKeys[j].keyType){
          //Serial.println(value);                
          mbusMeter[i].keyValueFloat = splitValue;
          mbusMeter[i].keyTimeStamp = getTime();
          if(mbusMeter[i].type == 3){
            totGasCon = splitValue;
            totGasTime = mbusMeter[i].keyTimeStamp;
          }
          else if(mbusMeter[i].type == 4){
            totHeatCon = splitValue;
            totHeatTime = mbusMeter[i].keyTimeStamp;
          }
          else if(mbusMeter[i].type == 7){
            totWatCon = splitValue;
            totWatTime = mbusMeter[i].keyTimeStamp;
          }
        }
      }
    }
  }
}

  
/*The following helper functions split the value part of each key,value pair according to the type of value*/
void splitToString(String &value, String &splitString){
  //Nothing to see here (yet)
}

void splitWithUnit(String &value, float &splitValue, String &splitUnit){
  /*Split a value containing a unit*/
  int unitStart = value.indexOf('*');
  splitValue = value.substring(0, unitStart).toFloat();
  splitUnit = value.substring(unitStart+1);
}

void splitNoUnit(String &value, float &splitValue){
  /*Split a value containing no unit*/
  splitValue = value.toFloat();
}

void splitMeterTime(String &value, struct tm &splitTime, time_t &splitTimestamp){
  /*Split a timestamp, returning a tm struct and a time_t object*/
  splitTime.tm_year = (2000 + value.substring(0, 2).toInt()) - 1900; //Time.h years since 1900, so deduct 1900
  splitTime.tm_mon = (value.substring(2, 4).toInt()) - 1; //Time.h months start from 0, so deduct 1
  splitTime.tm_mday = value.substring(4, 6).toInt();
  splitTime.tm_hour = value.substring(6, 8).toInt();
  splitTime.tm_min = value.substring(8, 10).toInt();
  splitTime.tm_sec = value.substring(10, 12).toInt();
  splitTimestamp =  mktime(&splitTime);
  /*Metertime is in local time. DST shows the difference in hours to UTC*/
  if(value.substring(12) == "S"){
    splitTime.tm_isdst = 1;
    splitTimestamp = splitTimestamp - (2*3600);
  }
  else if(value.substring(12) == "W"){
    splitTime.tm_isdst = 0;
    splitTimestamp = splitTimestamp - (1*3600);
  }
  else splitTime.tm_isdst = -1;
}

void splitWithTimeAndUnit(String &value, float &splitValue, String &splitUnit, struct tm &splitTime, time_t &splitTimestamp){
  /*Split a timestamped value containing a unit (e.g. Mbus value)*/
  String buf = value;
  int timeEnd = value.indexOf(')');
  int valueStart = value.indexOf('(')+1;
  value = buf.substring(0, timeEnd);
  splitMeterTime(value, splitTime, splitTimestamp);
  value = buf.substring(valueStart);
  splitWithUnit(value, splitValue, splitUnit);
}

String getUnit(String deviceType){
  String unit;
  if(deviceType == "energy") unit = "kWh";
  else if(deviceType == "power") unit = "kW";
  else if(deviceType == "voltage") unit = "V";
  else if(deviceType == "current") unit = "A";
  return unit;
}

void registerMbusMeter(String key, String value){
  /*The number and type of connected Mbus meters, as well as their Mbus ID and value type they report (standard or non-temperature compensated)
   * can only be determined during runtime by parsing the meter telegram. This function does just that. It scans the telegram for Mbus
   * OBIS codes. The detected meters and their parameters are then stored in the mbusMeter array.
   * During telegram processing, the mbusMeter array is cross referenced with the mbusConfig array to determine how the value should be pushed
   * to the data broker.
   */
  for(int i = 0; i < sizeof(mbusMeter)/sizeof(mbusMeter[0]); i++){
    String tempMbusKey = "0-" + String(i+1) + ":24.1.0";
    if(key == tempMbusKey){
      mbusMeter[i].type = value.toInt();
      mbusMeter[i].keyFound = true;
      /*Check if the Mbus key,value pair needs to be pushed.
       * This uses the same method as the regular DSMR keys, but with a 16-bit integer instead of a 32-bit long
       */
      if(mbusMeter[i].type >=0 && mbusMeter[i].type <= 16){
        unsigned int mask = 1;
        mask <<= mbusMeter[i].type;
        unsigned long test = _mbus_pushlist & mask;
        if(test > 0){
          mbusMeter[i].enabled = true;
        }
      }
      if(telegramCount < 3 && telegramDebug){
        Serial.print(" Found ");
        Serial.print(tempMbusKey);
        Serial.print(", meter of type ");
        Serial.print(mbusMeter[i].type);
        if(mbusMeter[i].type == 3) Serial.print(" (gas)");
        else if(mbusMeter[i].type == 7) Serial.print(" (water)");
        else Serial.print(" (other)");
        Serial.print(", registering");
        if (mbusMeter[i].enabled == true) Serial.print(" and enabling");
      }
    }
    /*Meter ID can be reported in regular (Dutch) or DIN43863-5 (Fluvius) format*/
    tempMbusKey = "0-" + String(i+1) + ":96.1.0";
    if(key == tempMbusKey){ //regular
      mbusMeter[i].id = value;
    }
    tempMbusKey = "0-" + String(i+1) + ":96.1.1";
    if(key == tempMbusKey){ //DIN43863-5
      mbusMeter[i].id = value;
    }
    /*Value can be reported in base or non-temperature compensated format. Priority to base*/
    tempMbusKey = "0-" + String(i+1) + ":24.2.3";
    if(key == tempMbusKey){ //non-temperature compensated
      if(mbusMeter[i].measurementType != 1){
        mbusMeter[i].measurementType = 3;
        mbusMeter[i].mbusKey = tempMbusKey;
      }
    }
    tempMbusKey = "0-" + String(i+1) + ":24.2.1";
    if(key == tempMbusKey){ //base
      mbusMeter[i].measurementType = 1;
      mbusMeter[i].mbusKey = tempMbusKey;
    }
  }
}

bool checkFloat(String floatKey, String floatType, float floatValue){
  /*Hardcoded check to filter spurious meter readings*/
  bool floatValid = true;
  if(floatType == "energy"){
    if(floatValue > 999999.9 || floatValue < -1.0) floatValid = false;
    if(floatKey == "A-0:0.0.1"){
      if(prevtotCon == 0 || spurCount > 10) prevtotCon = floatValue;
      if(floatValue < prevtotCon || (floatValue > prevtotCon + 23.9)) floatValid = false; //limit to 23.9kWh being consumed between two meter readings
      else if(floatValid = true) prevtotCon = floatValue;
    }
    if(floatKey == "A-0:0.0.2"){
      if(prevtotIn == 0 || spurCount > 10) prevtotIn = floatValue;
      if(floatValue < prevtotIn || (floatValue > prevtotIn + 23.9)) floatValid = false;
      else if(floatValid = true) prevtotIn = floatValue;
    }
    if(floatKey == "1-0:1.8.1"){
      if(prevtotConT1 == 0 || spurCount > 10) prevtotConT1 = floatValue;
      if(floatValue < prevtotConT1 || (floatValue > prevtotConT1 + 23.9)) floatValid = false;
      else if(floatValid = true) prevtotConT1 = floatValue;
    }
    if(floatKey == "1-0:1.8.2"){
      if(prevtotConT2 == 0 || spurCount > 10) prevtotConT2 = floatValue;
      if(floatValue < prevtotConT2 || (floatValue > prevtotConT2 + 23.9)) floatValid = false;
      else if(floatValid = true) prevtotConT2 = floatValue;
    }
    if(floatKey == "1-0:2.8.1"){
      if(prevtotIntT1 == 0 || spurCount > 10) prevtotIntT1 = floatValue;
      if(floatValue < prevtotIntT1 || (floatValue > prevtotIntT1 + 23.9)) floatValid = false;
      else if(floatValid = true) prevtotIntT1 = floatValue;
    }
    if(floatKey == "1-0:2.8.2"){
      if(prevtotIntT2 == 0 || spurCount > 10) prevtotIntT2 = floatValue;
      if(floatValue < prevtotIntT2 || (floatValue > prevtotIntT2 + 23.9)) floatValid = false;
      else if(floatValid = true) prevtotIntT2 = floatValue;
    }
  }
  else if(floatType == "power"){
    if(floatValue > 99.9 || floatValue < -1.0) floatValid = false;
  }
  else if(floatType == "voltage"){
    if(floatValue > 430.0 || floatValue < -1.0) floatValid = false;
  }
  if(!floatValid) syslog("Spurious value for DSMR key " + floatKey + ": " + String(floatValue), 2);
  if(spurCount > 10) spurCount = 0;
  return floatValid;
}

int numKeys(void){
  int shifti = sizeof(dsmrKeys)/sizeof(dsmrKeys[0]);
  return shifti;
}
