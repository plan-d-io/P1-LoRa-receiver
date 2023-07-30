# P1-LoRa
DSMR utility meter P1 port to LoRa to MQTT  
> You see, wire telegraph is a kind of a very, very long cat. You pull his tail in New York and his head is meowing in Los Angeles. And radio operates exactly the same way. The only difference is that there is no cat.  
> \-  Albert Einstein (attributed)

Stand-alone houses have their electrical utility meter inside the home, allowing them to plug a Wi-Fi-enabled dongle into the local (P1) data port to transmit real-time energy usage data. Flats and condos, however, often have their utility meters installed in a shared space outside Wi-Fi access.   
This repo contains firmware for ESP32 microcontrollers equipped with Semtech SX1276/77/78/79 LoRa radios to bridge this range gap. You’ll need two ESPs:
-	One ESP to connect to your utility meter and transmit P1 telegrams over LoRa
-	Aother ESP at your home,  to receive the LoRa P1 telegrams and forward them on your home Wi-Fi.

LoRa is a radio protocol designed to carry small amounts of data over long distances and/or challenging environments. So it’s like the very long cat, except that there is no cat.  

The code in this repo is designed for LilyGO TTGO T3 LoRa32 868MHz V1.6.1 ESP32 boards, but should work for any ESP32 Pico D4 with a Semtech SX1276/77/78/79 radio. YMMV.
