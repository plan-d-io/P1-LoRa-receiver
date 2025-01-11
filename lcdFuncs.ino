void setLCD(int lcdState, unsigned long displayULong, int displayInt){
  String rfSettings = "SF";
  rfSettings = rfSettings + String(setSF);
  rfSettings = rfSettings + " ";
  rfSettings = rfSettings + String(setBW);
  if (u8g2) {
    u8g2->clearBuffer();
    if(lcdState >= 0 && screenSaver < screenTimeOut){
      u8g2->drawStr(0, 12, "LoRa P1 receiver");
      u8g2->drawStr(0, 26, rfSettings.c_str());
      if(lcdState == 0){
        u8g2->drawStr(0, 40, "Starting ...");
        u8g2->sendBuffer();
      }
      else if(lcdState == 1){
        u8g2->sendBuffer();
      }
      else if(lcdState == 10){
        u8g2->drawStr(0, 40, "Wait for sync req");
        u8g2->sendBuffer();
      }
      if(lcdState == 11){
        u8g2->drawStr(0, 40, "Received sync req");
        u8g2->sendBuffer();
      }
      else if(lcdState == 12){
        String tempBuf = "Listening for ";
        tempBuf += String((waitForSyncVal-waitForSync)/1000);
        tempBuf += " s";
        u8g2->drawStr(0, 40, tempBuf.c_str());
        tempBuf = "Received ";
        tempBuf += String(telegramCounter);
        u8g2->drawStr(0, 56, tempBuf.c_str());
        u8g2->sendBuffer();
      }
      else if(lcdState == 13){
        u8g2->drawStr(0, 40, "Sending sync ACK");
        u8g2->sendBuffer();
      }
      else if(lcdState == 13){
        u8g2->drawStr(0, 40, "Reverting rf settings");
        u8g2->sendBuffer();
      }
      else if(lcdState == 14){
        String tempBuf = "Received ";
        tempBuf += String(telegramCounter);
        u8g2->drawStr(0, 40, tempBuf.c_str());
        u8g2->sendBuffer();
      }
      else if(lcdState == 15){
        u8g2->drawStr(0, 40, "Syncing ...");
        u8g2->sendBuffer();
      }
      else if(lcdState == 16){
        u8g2->drawStr(0, 40, "Syncing end");
        u8g2->sendBuffer();
      }
    }
    else u8g2->sendBuffer();
  }
}
