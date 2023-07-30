# P1-LoRa
DSMR utility meter P1 port (Fluvius etc) to LoRa to MQTT  
> You see, wire telegraph is a kind of a very, very long cat. You pull his tail in New York and his head is meowing in Los Angeles. And radio operates exactly the same way. The only difference is that there is no cat.  
> \-  Albert Einstein (attributed)

Stand-alone houses have their electrical utility meter inside the home, allowing them to plug a Wi-Fi-enabled dongle into the local (P1) data port to transmit real-time energy usage data. Flats and condos, however, often have their utility meters installed in a shared space outside Wi-Fi access.   

This repo contains firmware for ESP32 microcontrollers equipped with Semtech SX1276/77/78/79 LoRa radios to bridge this range gap. You’ll need two ESPs:
-	One ESP to connect to your utility meter and transmit P1 telegrams over LoRa
-	Aother ESP at your home,  to receive the LoRa P1 telegrams and forward them on your home Wi-Fi.

LoRa is a radio protocol designed to carry small amounts of data over long distances and/or challenging environments. So it’s like the very long cat, except that there is no cat.  

The code in this repo is designed for LilyGO TTGO T3 LoRa32 868MHz V1.6.1 ESP32 boards, but should work for any ESP32 Pico D4 with a Semtech SX1276/77/78/79 radio. YMMV.

## Features
- Auto-negotiation of RF channel parameters between transmitter and receiver
- Auto-tune of RF channel parameters during operation to ensure optimal throughput
- AES256 encryption of meter data
- Home Assistant integration

### RF channel auto-negotiation
- The receiver starts up, connects to your Wi-Fi, and starts listening to LoRa broadcasts from the transmitter on SF12 BW125.
- Once connected to your digital meter, the transmitters starts up and sends discovery packets on SF12 BW125.
- Receiver receives the discovery packet and sends an acknowledgement.
- Transmitter receives acknowledgement and starts sending virtual meter telegrams (containing the same amount of bytes of a real digital meter telegram). Transmitter also calculates (but does not send) CRC.
- Receiver receives telegram, calculates CRC, transmits CRC as acknowledgement back to transmitter.
- Transmitter receives CRC, compares with calculated CRC to check RF channel integrity, repeats this a few times.
- If RF channel integrity has been confirmed, transmitter instructs receiver to switch to better RF channel (higher BW and lower SF) and switches as well.
- This repeats until RF channel integrity cannot longer be confirmed, or until  SF7 BW250 (best settings possible).
- If RF channel integrity could not be established, both transmitter and receiver switch back to RF settings which were known to work.

After this handshake has concluded, the transmitter starts sending meter telegrams containing real data from the P1 port. For every received telegram, the receiver sends back the CRC. The transmitter checks this CRC to determine packet loss. If packet loss is too high, a new sync request is sent to the receiver.
