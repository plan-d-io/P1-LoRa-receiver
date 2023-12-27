#include "boards.h"
#include <LoRa.h>
#include "mbedtls/aes.h"
#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include <esp32/rom/crc.h>
#include <elapsedMillis.h>
#include <WiFi.h>
#include "time.h"
#include "ArduinoJson.h"
#include <PubSubClient.h>

/*Keep or change these three settings, but make sure the are identical on both transmitter and receiver!*/
uint8_t networkNum = 127;                 //Must be unique for every transmitter-receiver pair
char plaintextKey[] = "abcdefghijklmnop"; //The key used to encrypt/decrypt the meter telegram
char networkID[] = "myLoraNetwork";       //Used to generate the initialisation vector for the AES encryption algorithm

/*WiFi settings*/
bool _wifi_en = true;
String _wifi_ssid = "YOURWIFISSID";
String _wifi_password = "YOURWIFIPASSWORD";

/*MQTT settings*/
String _mqtt_host = "";                   //ip-address of your mqtt broker
unsigned int _mqtt_port = 1883;
String _mqtt_id = "LoRa-P1";
bool _mqtt_auth = false;
String _mqtt_user = "mqtt username";
String _mqtt_pass = "mqtt password";
String _mqtt_prefix = "data/devices/utility_meter/";
String haDeviceName = "Utility meter";
bool _mqtt_en = false;                    //set to true to enable mqtt client
bool _ha_en = false;                      //set to true to enable Home Assistant integration
bool debugInfo = true;

/*SF, BW and  wait times used to sync transmitter and receiver
 Needs to be indentical on both transmitter and receiver*/
static const byte loraConfig[][4] ={
  {12, 125, 8, 38},
  {12, 250, 3, 20},
  {11, 250, 2, 10},
  {10, 250, 2, 8},
  {9, 250, 2, 8},
  {8, 250, 2, 8},
  {7, 250, 1, 8}
};

/*ms between updates to keep in line with 1% LoRa duty cycle, for single and three phase meter telegrams
 Needs to be indentical on both transmitter and receiver*/
static const unsigned long loraUpdate[][7] ={
  {230474, 107207, 57803, 31099, 16700, 9000, 4500},
  {372346, 173169, 93369, 50238, 26977, 14538, 7000}
  };

/*Template of meter telegram*/
float meterData[] = {
  999999.999, //totConT1
  999999.999, //totConT2
  999999.999, //totInT1
  999999.999, //totInT2
  99.999,     //TotpowCon
  99.999,     //TotpowIn
  999999.999, //avgDem
  999999.999, //maxDemM
  999.99,     //volt1
  999.99,     //current1
  999999.999, //totGasCon
  999999.999, //totWatCon
  99.999,     //powCon1
  999999.999, //powCon2
  999999.999, //powCon3
  99.999,     //powIn1
  999999.999, //powIn2
  999999.999  //powIn3
  999.99,     //volt2
  999.99,     //volt3
  999.99,     //current2
  999.99,     //current3
  0,          //pad
  0           //pad
};

struct keyConfig {
  String dsmrKey;
  float maxVal;
  String deviceType;
  String  keyName;
  String  keyTopic;
  bool retain;
};

/*LoRa runtime vars*/
elapsedMillis runLoop, waitForSync, waitForSend, telegramTimeOut, delayCRC, mqttConnectLoop;
unsigned long waitForSyncVal, waitForSendVal;
int syncMode, syncTry, packetLoss;
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
/*WiFi & MQTT runtime vars*/
WiFiClient wificlient;
PubSubClient mqttclient(wificlient);
bool haDiscovered, wifiError, mqttWasConnected, mqttPushed;
unsigned int mqttPushCount, mqttPushFails, reconncount;

void setup() {
  initBoard();
  // When the power is turned on, a delay is required.
  delay(1500);
  setLCD(0, 0, 0);
  Serial.begin(115200);
  Serial.println("LoRa P1 Receiver");
  /*Start WiFi*/
  if(_wifi_en){
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifi_ssid.c_str(), _wifi_password.c_str());
    WiFi.setHostname("p1dongle");
    elapsedMillis startAttemptTime;
    Serial.println("Attempting connection to WiFi network " + _wifi_ssid);
    while (WiFi.status() != WL_CONNECTED && startAttemptTime < 20000) {
      delay(200);
      Serial.print(".");
    }
    Serial.println("");
    if(WiFi.status() == WL_CONNECTED){
      Serial.println("Connected to the WiFi network " + _wifi_ssid);
      if(_mqtt_en){
        setupMqtt();
        connectMqtt();
      }
    }
    else{
      Serial.println("Could not connect to the WiFi network");
      wifiError = true;
    }
  }
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

void loop() {
  if(syncMode >= 0) syncLoop();
  else {
    setLCD(14, 0, 0);
    if(sendCRC) {
      if(delayCRC > 200){
        sendCRCAck();
        sendCRC = false;
      }
    }
    /* When receiver, in telegram receive mode, has not received a meter telegram for
     * more than 5 minutes, revert back into sync mode */
    if(telegramTimeOut > 300000) syncMode = 0; 
  }
  onReceive(LoRa.parsePacket());
  mqttclient.loop();
  if(_mqtt_en){
    if(mqttConnectLoop > 300000){
      connectMqtt();
      mqttConnectLoop = 0;
    }   
  }
}

void onReceive(int packetSize) {
  if (packetSize == 0)
      return;
  // read packet header bytes:
  byte inNetworkNum = LoRa.read();
  byte inMessageType = LoRa.read();
  byte inMessageCounter = LoRa.read();
  byte inPayloadSize = LoRa.read();
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
    Serial.println("Wrong network ID");
    return;
  }
  byte incoming[inPayloadSize];
  int i = 0;
  while(i<inPayloadSize) {
    incoming[i]= LoRa.read();
    i++;
  }
  if(inMessageType == 0 || inMessageType == 1 || inMessageType == 3 || inMessageType == 31 || inMessageType == 32 || inMessageType == 33 || inMessageType == 128){
    if(inPayloadSize == 48 || inPayloadSize == 96) processTelegram(inMessageType, inMessageCounter, incoming);
  }
  else if(inMessageType == 170 || inMessageType == 178 || inMessageType == 85 || inMessageType == 93){
    processSync(inMessageType, inMessageCounter, incoming);
  }
}
