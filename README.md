# P1-LoRa bridge
Send P1 meter telegrams from DSMR digital meters over long range (LoRa) to a WiFi enabled receiver, using this firmware for ESP32 microcontrollers and Semtech SX1276/77/78/79 radios
> You see, wire telegraph is a kind of a very, very long cat. You pull his tail in New York and his head is meowing in Los Angeles. And radio operates exactly the same way. The only difference is that there is no cat.  
> \-  Albert Einstein (attributed)

## Introduction
This repo contains firmware for ESP32 microcontrollers equipped with Semtech SX1276/77/78/79 LoRa radios to transmit DSMR P1 telegrams from Dutch/Belgian energy meters over long ranges to a WiFi enabled receiver, which forwards the meter data to the local network (e.g. over MQTT). 

For this, you’ll need two ESP32s:
-	One ESP32 to connect to your utility meter and transmit P1 telegrams over LoRa
-	Another ESP32 at your home, to receive the LoRa P1 telegrams and forward them over your home Wi-Fi connection.

The code in this repo is tested and verified to work on LilyGO TTGO T3 LoRa32 868MHz V1.6.1 ESP32 boards, but should work for any ESP32 Pico D4 with a Semtech SX1276/77/78/79 radio. YMMV.

**NOTE**: this code is currently just a technology demonstration!

## Features
- Transmit P1 meter telegrams in (near) real-time from locations outside WiFi range
- Auto-negotiation of LoRa RF channel parameters between transmitter and receiver to achieve optimal data throughput
- Auto-tune of LoRa RF channel parameters during operation to ensure optimal throughput even if RF environment changes
- AES256 encryption of meter data over LoRa
- MQTT client on receiver side
- Home Assistant integration

## Use case
Most devices enabling real-time access to P1 meter data (“_dongles_”) use WiFi. Flats and condos, however, often have their utility meters outside Wi-Fi access, e.g. in a basement or another shared space. This firmware uses LoRa to connect P1 utility meters to the home WiFi network. LoRa is a radio protocol designed to carry small amounts of data over long distances and/or challenging environments like multiple floors. So it’s like the very long cat, except that there is no cat.

## Installation

## About LoRa
LoRa operates in the unlicensed ISM radio spectrum. Anyone is free to use this slice of spectrum as long as they adhere to a maximum use time, defined as the duty cycle limit. For EU 868MHz, the maximum duty cycle is 1%. So if there are 86400 seconds in a day, this means your device can transmit a total of 86400 x 1% = 864 seconds per day. This is called the _air time_.

Air time is dependent on the size of the payload (three-phase meters have a larger payload than single-phase meters), symbol encoding and RF channel settings (bandwidth (BW) and spreading factor (SF)). 

Each increment in both SF or BW roughly halves the amount of LoRa airtime and, as such, doubles update rates (see table above). Symbol encoding is kept default on this firmware.

A lower BW (125 vs 250) might help with penetration of challenging environments (e.g. multiple solid walls), but also increases the chances of clock mismatch between transmitter and receiver (see xxx). This can especially be a problem with cheaper LoRa modules, or transmitter/receiver modules from different manufacturers. By default, this firmware only uses BW 125 during the first RF handshake step, switching to BW 250 for all steps afterwards.

## RF auto negotiation
- The receiver starts up, connects to your Wi-Fi, and starts listening to LoRa broadcasts from the transmitter on SF12 BW125.
- Once connected to your digital meter, the transmitters starts up and sends discovery packets on SF12 BW125. This is the lowest bandwidth supported, but it does have the best range.
- Once the receiver receives the discovery packet, a handshake between the transmitter and receiver is initiated by exchanging (virtual) meter telegrams and CRC acknowledgements at increasingly higher bandwidth settings. Both transmitter and receiver eventually settle on the highest bandwidth setting still allowing reliable communication (packet loss < 50%).
- Once the transmitter and receiver are synced, real P1 meter telegrams are exchanged. The update rate is dependent on the RF channel settings to comply to the 1% duty cycle limit.

## RF channel monitoring
RF channel performance might change during the day, e.g. damp vs dry weather or a car parked in front of your digital meter in the basement of your apartment building. Both transmitter and receiver keep track of RF channel integrity by exchanging CRC acknowledgments of transmitted meter telegrams. If packet loss is more than 50%,  a new RF handshake is initiated to settle on more reliable settings. Likewise, if packet loss is below 15%, a new handshake is initiated to settle on settings providing higher throughput. This all happens automatically. 

## Receiver
The receiver connects to your WiFi and pushes 
Transmitter receives acknowledgement and starts sending virtual meter telegrams (containing the same amount of bytes of a real digital meter telegram). Transmitter also calculates (but does not send) CRC.
Receiver receives telegram, calculates CRC, transmits CRC as acknowledgement back to transmitter.
Transmitter receives CRC, compares with calculated CRC to check RF channel integrity, repeats this a few times.
If RF channel integrity has been confirmed, transmitter instructs receiver to switch to better RF channel (higher BW and lower SF) and switches as well.
This repeats until RF channel integrity cannot longer be confirmed, or until  SF7 BW250 (best settings possible).
If RF channel integrity could not be established, both transmitter and receiver switch back to RF settings which were known to work.
After this handshake has concluded, the transmitter starts sending meter telegrams containing real data from the P1 port. For every received telegram, the receiver sends back the CRC. The transmitter checks this CRC to determine packet loss. If packet loss is too high, a new

