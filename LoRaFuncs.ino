void processSync(byte inMsgType, byte inMsgCounter, byte msg[]){
  if(msg[1] > 6 && msg[1] < 13 && (msg[2] == 125 || msg[2] == 250)){ //check for validity of link parameters
    if(inMsgType == 170 ){ //start sync message
      if(sizeof(msg) > 3){
        /* If the unit is waiting for the start sync signal, initiate syncing procedure.
         * If the start sync signal is rebroadcast during the syncing, reinitiate procedure.
         */
        syncCount = msg[0];
        if(loraConfig[syncCount][0] == msg[1] && loraConfig[syncCount][1] == msg[2]){
          setLCD(11, 0, 0);
          setSF = msg[1];
          setBW = msg[2];
          waitForSyncVal = msg[3]*1000;
          Serial.print("Received sync signal to set SF to ");
          Serial.print(setSF);
          Serial.print(", BW to ");
          Serial.print(setBW);
          Serial.print(", and timer to ");
          Serial.print(waitForSyncVal/1000);
          Serial.println(" seconds");
          waitForSync = 0;
          telegramCounter++;
          syncMode = 4;
        }
        else Serial.println("Channel settings do not match");

      }
    }
    else if(inMsgType == 85 ){ //stop sync signal
      if(sizeof(msg) > 2){
        if(syncMode > -1){
          syncCount = msg[0];
          if(loraConfig[syncCount][0] == msg[1] && loraConfig[syncCount][1] == msg[2]){
            setSF = loraConfig[syncCount][0];
            setBW = loraConfig[syncCount][1];
            waitForSync = 0;
            Serial.print("Received stop sync signal, setting SF to ");
            Serial.print(setSF);
            Serial.print(", BW to ");
            Serial.print(setBW);
            Serial.println(", and stopping sync procedure");
            sendSyncAck(false);
            telegramCounter = 0;
            syncMode = 9;
          }
          else Serial.println("Channel settings do not match");
        }
        else{
          Serial.println("wrong sync mode");
        }
      }
      else{
      Serial.print("Wrong size: ");
      Serial.println(sizeof(msg));
      }
    }
  }  
}

void sendSyncAck(bool startSync){
  Serial.print("Sending sync ACK with SF");
  Serial.print(setSF);
  Serial.print(", BW");
  Serial.print(setBW);
  Serial.print(", timer ");
  Serial.println(waitForSyncVal/1000);
  unsigned long tempMillis = millis();
  LoRa.beginPacket();
  LoRa.write(networkNum);
  if(startSync)LoRa.write(178);
  else LoRa.write(93);
  LoRa.write(telegramCounter);
  LoRa.write(3);
  LoRa.write(setSF);
  LoRa.write(setBW);
  LoRa.write(byte(waitForSyncVal/1000));
  LoRa.endPacket();
  Serial.print("Transmission took ");
  Serial.print(millis() - tempMillis);
  Serial.println(" ms");
  Serial.print("Setting SF to ");
  Serial.print(setSF);
  Serial.print(", BW to ");
  Serial.println(setBW);
  LoRa.setSpreadingFactor(setSF);
  LoRa.setSignalBandwidth(setBW*1000);
  telegramCounter = 0;
}

void processTelegram(byte inMsgType, byte inMsgCounter, byte msg[]){
    if(inMsgType == 0 || inMsgType == 1 || inMsgType == 3 || inMsgType == 31 || inMsgType == 32 || inMsgType == 33){
      if(inMsgType == 0) Serial.println("Message is calibration telegram");
      else if(inMsgType == 1) Serial.println("Message is single phase meter telegram");
      else Serial.println("Message is three phase meter telegram");
      if(inMsgType != 0 && syncMode > 0){
        Serial.println("Transmitter is already synced, stopping syncing");
        syncMode = -1;
      }
      if(inMsgType == 0 || inMsgType == 1) payloadLength = 12; //number of 32bit values encoded into the payload
      else if(inMsgType >= 3) payloadLength = 24; 
      unsigned int payloadByteSize = payloadLength*4;
      /*Serial.print("Encrypted byte array: ");
      for(int k = 0; k < payloadByteSize; k++){
        Serial.print(msg[k], DEC);
        if(k<payloadByteSize-1) Serial.print(", ");
      }
      Serial.println("");*/
      /*Create a plaintext output buffer for the AES decryptor and set all elements to 0*/
      unsigned char plainText[aesBufferSize];
      memset(plainText,0,sizeof(plainText));
      memset(iv,0,sizeof(iv));
      memcpy(iv, iv2, sizeof(iv));
      /*The AES decryption part*/
      mbedtls_aes_context aes;
      mbedtls_aes_init( &aes );
      mbedtls_aes_setkey_dec( &aes, key, 256 );
      mbedtls_aes_crypt_cbc( &aes, MBEDTLS_AES_DECRYPT, aesBufferSize, iv, msg, plainText );
      mbedtls_aes_free( &aes );
      /*Serial.print("Decrypted byte array: ");
      int f = payloadByteSize - 1;
      for(int l = 0; l< payloadByteSize; l++){
        Serial.print(plainText[l], DEC);
        if(l<f) Serial.print(", ");
      }
      Serial.println("");*/
      /*Assemble the unsigned longs again and convert them back into floats*/
      for(int k = 0; k < payloadLength; k++){
        unsigned long recVar = 0;
        int j = k*4;
        recVar += plainText[j+3] << 24;
        recVar += plainText[j+2] << 16;
        recVar += plainText[j+1] << 8;
        recVar += plainText[j];
        float tempFloat = recVar/1000.0;
        meterData[k] = tempFloat;
        /*Serial.print("Received int ");
        Serial.print(recVar);
        Serial.print(" becomes float ");
        Serial.println(tempFloat, 3);*/
      }
      romCRC = (~crc32_le((uint32_t)~(0xffffffff), (const uint8_t*)plainText, 8))^0xffffffFF;
      telegramCounter++;
      telegramTimeOut = 0;
      if(inMsgType > 0){
        Serial.print("Message counter: ");
        Serial.println(inMsgCounter);
        Serial.print("Received counter: ");
        Serial.println(telegramCounter);
        if(inMsgCounter < telegramCounter){
          telegramCounter = inMsgCounter;
          Serial.println("Reached end of telegram count cycle, resetting");
        }
        int plTemp = (((inMsgCounter-telegramCounter)*100)/inMsgCounter);
        if(plTemp >= -0 && plTemp < 101) {
          packetLoss = plTemp;
          packetLossf = plTemp*1.0;
        }
        else packetLossf = 0.0;
        packetLossFound = true;
      }
      /* When replying too soon after a receive, the transmitter sometimes misses the reply (especially at low SF and close proximity)
       * Solved this for telegram receiver mode, but should probably add an additional state in the sync mode loop (to avoid using delay())
       */
      if(syncMode >= 0) {
        delay(20);
        sendCRCAck();
      }
      else{
        if(inMsgType > 0){
          sendCRC = true;
          delayCRC = 0;
          //pushMqtt();
          processMeterTelegram();
          meterError = false;
        }
      }
    }
}

void sendCRCAck(){
  Serial.print("Transmitting CRC with message counter ");
  Serial.println(telegramCounter);
  /*Transmit everything*/
  LoRa.beginPacket();
  LoRa.write(networkNum);
  LoRa.write(8);
  LoRa.write(telegramCounter);
  LoRa.write(4);
  unsigned char payload[4];
  payload[0] = romCRC & 0xFF;   
  payload[1] = (romCRC >> 8) & 0xFF;
  payload[2] = (romCRC >> 16) & 0xFF;
  payload[3] = (romCRC >> 24) & 0xFF;
  LoRa.write(payload[0]);
  LoRa.write(payload[1]);
  LoRa.write(payload[2]);
  LoRa.write(payload[3]);
  LoRa.endPacket();  // finish packet and send it.  ????????
}

void syncLoop(){
  if(syncMode == 0){
    //initial wait loop for sync packet to arrive
    setLCD(10, 0, 0);
    telegramCounter = 0;
    return;
  }
  else if(syncMode == 1){
    //waiting for sync and test telegrams to arrive
    setLCD(12, 0, 0);
    if(waitForSync > waitForSyncVal){
      waitForSend = 0;
      syncMode = 3;
    }
  }
  else if(syncMode == 3){
    if(syncCount > 0) syncMode = 1;
    else syncMode = 0; //Eventually the transmitter will revert back to SF12 125
    setLCD(10, 0, 0);
    Serial.print("We have received ");
    Serial.print(telegramCounter);
    Serial.println(" telegrams");
    if(telegramCounter == 0){
      setLCD(13, 0, 0);
      if(syncCount > 0) syncCount--; //revert to previous settings or stay on the first setting
      setSF = loraConfig[syncCount][0];
      setBW = loraConfig[syncCount][1];
      waitForSendVal = loraConfig[syncCount][2]*1000;
      waitForSyncVal = loraUpdate[0][syncCount]*2; //the transmitter has probably already stopped syncing, so wait if we can see meter telegrams being sent
      Serial.print("Reverting back to SF");
      Serial.print(setSF);
      Serial.print(" BW");
      Serial.println(setBW);
      LoRa.setSpreadingFactor(setSF);
      LoRa.setSignalBandwidth(setBW*1000);
    }
    telegramCounter = 0;
    waitForSync = 0;

  }
  else if(syncMode == 4){ //send ack of rf channel switch and make the switch
    setLCD(13, 0 ,0);
    if(waitForSync > 200){//wait a small period to submit ACK because at lower SF the transmitter sometimes doesn't switch fast enough from send to receive mode
      sendSyncAck(true);
      waitForSync = 0;
      syncMode = 1;
    }
  }
  else if(syncMode == 9){
    setLCD(16, 0, 0);
    Serial.println("Setting SF and BW");
    LoRa.setSpreadingFactor(setSF);
    LoRa.setSignalBandwidth(setBW*1000);
    telegramCounter = 0;
    syncMode = -1;
  }
}
