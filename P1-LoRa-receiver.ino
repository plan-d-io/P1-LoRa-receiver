#include "boards.h"
#include "LoRaConfig.h"
#include <LoRa.h>
#include "mbedtls/aes.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include <esp32/rom/crc.h>
#include "rom/rtc.h"
#include <esp_int_wdt.h>
#include <esp_task_wdt.h>
#include <LittleFS.h>
#define SPIFFS LittleFS
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Preferences.h>
#include <PubSubClient.h>
#include <time.h>
#include <Update.h>
#include "ArduinoJson.h"
#include <elapsedMillis.h>
#include "UUID.h"
#include "configStore.h"
#include "ledControl.h"
#include "externalIntegrations.h"
#include "WebRequestHandler.h"
#include "webHelp.h"
#define TRIGGER 25 //Pin to trigger meter telegram request

/*Keep or change these three settings, but make sure the are identical on both transmitter and receiver!*/
uint8_t networkNum = 127;                 //Must be unique for every transmitter-receiver pair
char plaintextKey[] = "abcdefghijklmnop"; //The key used to encrypt/decrypt the meter telegram
char networkID[] = "myLoraNetwork";       //Used to generate the initialisation vector for the AES encryption algorithm


/*LoRa runtime vars*/
elapsedMillis runLoop, waitForSync, waitForSend, telegramTimeOut, delayCRC;
bool packetRSSIFound, packetSNRFound, packetSFFound, packetLossFound;
unsigned long waitForSyncVal, waitForSendVal;
int syncMode, syncTry, packetLoss;
float packetSNR, packetSF, packetLossf, packetRSSI;
int syncCount = 0;
byte setSF, setBW;
byte telegramCounter, telegramAckCounter;
bool sendCRC;
/*AES encryption runtime vars*/
unsigned int payloadLength;
unsigned int aesBufferSize = 96;
uint32_t romCRC;
unsigned char key[32];
unsigned char iv[16], iv2[16];

unsigned int fw_ver = 217;

//General global vars
Preferences preferences;
AsyncWebServer server(80);
DNSServer dnsServer;
uint8_t* certData = nullptr; 
WiFiClient wificlient;
PubSubClient mqttclient(wificlient);
WiFiClientSecure *client = new WiFiClientSecure;
PubSubClient mqttclientSecure(*client);
HTTPClient https;
UUID uuid;
bool clientSecureBusy, mqttPaused, mqttWasPaused, resetWifi, factoryReset, updateAvailable;
bool wifiError, mqttWasConnected, wifiSave, wifiScan, debugInfo, timeconfigured, timeSet, spiffsMounted,rebootInit;
bool bundleLoaded = true;
bool haDiscovered = false;
String configBuffer, resetReason, infoMsg, ssidList;
char apSSID[] = "P1000000";
unsigned int mqttPushCount, mqttPushFails, onlineVersion, fw_new;
unsigned int secureClientError = 0;
time_t meterTimestamp;
//Global timing vars
elapsedMillis sinceConnCheck, sinceUpdateCheck, sinceClockCheck, sinceLastUpload, sinceDebugUpload, sinceRebootCheck, sinceMeterCheck, sinceWifiCheck, sinceTelegramRequest;
//General housekeeping vars
unsigned int reconncount, remotehostcount, telegramCount, telegramAction;
int wifiRSSI;
float freeHeap, minFreeHeap, maxAllocHeap;
byte mac[6];
uint8_t prevButtonState = false;
/*Debug*/
bool serialDebug = true;
bool telegramDebug = false;
bool mqttDebug = false;
bool httpDebug = false;
bool extendedTelegramDebug = false;

void setup(){
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);
  setLCD(0, 0, 0);
  Serial.begin(115200);
  Serial.println("LoRa P1 Receiver");
  pinMode(TRIGGER, OUTPUT);
  unitState = -1;
  Serial.begin(115200);
  delay(500);
  getHostname();
  Serial.println();
  syslog("Digital meter dongle booting", 0);
  restoreConfig();
  _wifi_STA = true;
  _wifi_ssid = "Aether";
  _wifi_password = "RaidillondelEauRouge0x03";
  _update_autoCheck = false;
  _update_auto = false;
  initSPIFFS();
  externalIntegrationsBootstrap();
  if(_trigger_type == 0) digitalWrite(TRIGGER, HIGH);
  else digitalWrite(TRIGGER, LOW);
  syslog("Digital meter dongle " + String(apSSID) +" V" + String(fw_ver/100.0) + " by plan-d.io", 1);
  if(_dev_fleet) syslog("Using experimental (development) firmware", 2); //change this to one variable, but keep legacy compatibility intact
  if(_alpha_fleet) syslog("Using pre-release (alpha) firmware", 0);
  if(_v2_fleet) syslog("Using V2.0 firmware", 0);
  syslog("Checking if internal clock is set", 0);
  printLocalTime(true);
  _bootcount = _bootcount + 1;
  syslog("Boot #" + String(_bootcount), 1);
  saveBoots();
  get_reset_reason(rtc_get_reset_reason(0));
  syslog("Last reset reason (hardware): " + resetReason, 1);
  syslog("Last reset reason (firmware): " + _last_reset, 1);
  debugInfo = true;
  initWifi();
  server.addHandler(new WebRequestHandler());
  server.begin();
  configBuffer = returnConfig();
  String availabilityTopic = _mqtt_prefix.substring(0, _mqtt_prefix.length()-1);
  initLoRa();
  Serial.println("Done");
}

void loop(){
  blinkLed();
  if(wifiScan) scanWifi();
  if(sinceRebootCheck > 2000){
    if(rebootInit){
      forcedReset();
    }
    sinceRebootCheck = 0;
  }
  if(_trigger_type == 0){
    /*Continous triggering of meter telegrams*/
    if(sinceMeterCheck > 180000){
      if(!meterError) syslog("Meter disconnected", 2);
      meterError = true;
      if(_wifi_STA && unitState < 7) unitState = 6;
      else if(!_wifi_STA && unitState < 3) unitState = 2;
      sinceMeterCheck = 0;
    }
  }
  else if(_trigger_type == 1){
    /*On demand triggering of meter telegram*/
    if(sinceTelegramRequest >= _trigger_interval *1000){
      digitalWrite(TRIGGER, HIGH);
      sinceTelegramRequest = 0;
    }
    if(sinceMeterCheck > (_trigger_interval *1000) + 30000){
      syslog("Meter disconnected", 2);
      meterError = true;
      if(_wifi_STA && unitState < 7) unitState = 6;
      else if(!_wifi_STA && unitState < 3) unitState = 2;
      sinceMeterCheck = 0;
    }
  }
  if(!_wifi_STA){
    /*If dongle is in access point mode*/
    dnsServer.processNextRequest();
    if(sinceWifiCheck >= 600000){
      /*If dongle is in AP mode, check every once in a while if the configured wifi SSID can't be detected
       * If so, reboot so the dongle starts up again in connected STA mode. */
      if(scanWifi()){
        saveResetReason("Found saved WiFi SSID, rebooting to reconnect");
        if(saveConfig()){
          syslog("Found saved WiFi SSID, rebooting to reconnect", 1);
          setReboot();
        }
      }
      sinceWifiCheck = 0;
    }
    if(sinceClockCheck >= 600000){
      timeSet = false;
      sinceClockCheck = 0;
    }
  }
  else{
    /*If dongle is connected to wifi*/
    if(!bundleLoaded) restoreSPIFFS();
    if(_mqtt_en){
      if(_mqtt_tls){
        mqttclientSecure.loop();
      }
      else{
        mqttclient.loop();
      }
    }
    if(lastEIDcheck >= EIDcheckInterval){
      eidHello();
    }
    if(lastEIDupload > EIDuploadInterval){
      eidUpload();
    }
    if(_update_autoCheck && sinceUpdateCheck >= 86400000){
      updateAvailable = checkUpdate();
      if(updateAvailable) startUpdate();
      sinceUpdateCheck = 0;
    }
    if(sinceClockCheck >= 3600){
      if(!timeconfigured) timeSet = false; //if timeConfigured = true, the NTP serivce takes care of reqular clock syncing
      sinceClockCheck = 0;
    }
    if(sinceConnCheck >= 60000){
      if(_ha_en && debugInfo) hadebugDevice(false);
      checkConnection();
      sinceConnCheck = 0;
    }
    if(sinceDebugUpload >= 300000){
      getHeapDebug();
      sinceDebugUpload = 0;
    }
    /*If no remote hosts can be reached anymore, try a reboot for up to four times*/
    if(reconncount > 15 || remotehostcount > 60 || secureClientError > 4){
      _rebootSecure++;
      if(_rebootSecure < 4){
        saveResetReason("Rebooting to try fix connections");
        if(saveConfig()){
          syslog("Rebooting to try fix connections", 2);
          setReboot();
        }
        reconncount = 0;
      }
      else{
        /*After four reboots, increase time between reboots drastically*/
        if(reconncount > 150 || remotehostcount > 600 || secureClientError > 40){
          _rebootSecure++;
          saveResetReason("Rebooting to try fix connections");
          if(saveConfig()){
            syslog("Rebooting to try fix connections", 2);
            setReboot();
          }
          reconncount = 0;
        }
      }
    }
  }

  if(syncMode >= 0) syncLoop();
  else {
    setLCD(14, 0, 0);
    if(sendCRC) {
      if(delayCRC > 200){
        sendCRCAck();
        sendCRC = false;
      }
    }
  }
  /* When receiver, in telegram receive mode, has not received a meter telegram for
   * more than 5 minutes, revert back into sync mode */
  if(telegramTimeOut > 300000){
    syncMode = 0;
    setSF = 12;
    setBW = 125;
    syslog("Communication timeout, restarting sync without request", 3);
    LoRa.setSpreadingFactor(setSF);
    LoRa.setSignalBandwidth(setBW*1000);
    telegramTimeOut = 0;
  }
  onReceive(LoRa.parsePacket());
}

void onReceive(int packetSize) {
  if (packetSize == 0)
      return;
  // read packet header bytes:
  byte inNetworkNum = LoRa.read();
  byte inMessageType = LoRa.read();
  byte inMessageCounter = LoRa.read();
  byte inPayloadSize = LoRa.read();
  packetRSSIFound = true;
  packetSNRFound = true;
  packetSFFound = true;
  packetSF = setSF*1.0;
  Serial.print("SF: ");
  Serial.println(setSF);
  int rssiInt = LoRa.packetRssi();
  packetRSSI = rssiInt*1.0;
  Serial.print("RSSI: ");
  Serial.println(rssiInt);
  packetSNR = LoRa.packetSnr();
  Serial.print("Receiving LoRa message, meant for network ID ");
  Serial.print(inNetworkNum);
  Serial.print(", message type ");
  Serial.print(inMessageType);
  Serial.print(" witch message count ");
  Serial.print(inMessageCounter);
  Serial.print(", length of ");
  Serial.print(inPayloadSize);
  Serial.println(" bytes");
  if(inNetworkNum != networkNum) {
    syslog("Received message with wrong network ID " + String(inNetworkNum), 2);
    byte incoming;
    while(LoRa.available()) {
      incoming= LoRa.read();
    }
    return;
  }
  byte incoming[inPayloadSize];
  int i = 0;
  while(i<inPayloadSize) {
    incoming[i]= LoRa.read();
    i++;
  }
  byte rest;
  while(LoRa.available()) {
    rest= LoRa.read();
  }
  if(inMessageType == 0 || inMessageType == 1 || inMessageType == 3 || inMessageType == 31 || inMessageType == 32 || inMessageType == 33 || inMessageType == 128){
    if(inPayloadSize == 48 || inPayloadSize == 96) processTelegram(inMessageType, inMessageCounter, incoming);
  }
  else if(inMessageType == 170 || inMessageType == 178 || inMessageType == 85 || inMessageType == 93){
    processSync(inMessageType, inMessageCounter, incoming);
  }
  else if(inMessageType == 24){
    syncMode = 0;
    setSF = 12;
    setBW = 125;
    syslog("Received ACK for sync restart request, restarting sync", 2);
    LoRa.setSpreadingFactor(setSF);
    LoRa.setSignalBandwidth(setBW*1000);
  }
  else{
    byte incoming;
    while(LoRa.available()) {
      incoming= LoRa.read();
    }
  }
}

void initLoRa(){
  /*Start LoRa radio*/
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DIO0_PIN);
  if (!LoRa.begin(LoRa_frequency)) {
      Serial.println("Starting LoRa failed!");
      while (1);
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  Serial.println("Listening to SF12 125");
  /*Create a 32 byte key for use by the AES encryptor by SHA256 hashing the user provided key*/
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  const size_t keyLength = strlen(plaintextKey);         
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) plaintextKey, keyLength);
  mbedtls_md_finish(&ctx, key);
  mbedtls_md_free(&ctx);
  /*Create a 16 byte initialisation vector for the AES encryptor by SHA1 hashing the user provided network name*/
  md_type = MBEDTLS_MD_SHA1;
  const size_t networkIDLength = strlen(networkID);         
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) networkID, networkIDLength);
  mbedtls_md_finish(&ctx, iv);
  mbedtls_md_free(&ctx);
  /*Init some runtime variables*/
  waitForSendVal = 6000;
  waitForSyncVal = 38000;
  setSF = 12;
  setBW = 125;
  syncMode = 0;
  waitForSync = 300000;
}
