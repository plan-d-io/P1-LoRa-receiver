# P1-LoRa bridge
Send P1 meter telegrams from DSMR digital meters over long range (LoRa) to a WiFi enabled receiver with this firmware for ESP32 microcontrollers and Semtech SX1276/77/78/79 radios.

> You see, wire telegraph is a kind of a very, very long cat. You pull his tail in New York and his head is meowing in Los Angeles. And radio operates exactly the same way. The only difference is that there is no cat.  
> \-  Albert Einstein (attributed)

## Introduction
This repo contains firmware for ESP32 microcontrollers equipped with Semtech SX1276/77/78/79 LoRa radios to transmit DSMR P1 telegrams from Dutch/Belgian energy meters over long ranges to a WiFi enabled receiver, which forwards the meter data to the local network (e.g. over MQTT). 

For this to work, you’ll need two ESP32s:
-	One ESP32 to connect to your utility meter and transmit P1 telegrams over LoRa: the _transmitter_.
-	Another ESP32 at your home, to receive the LoRa P1 telegrams and forward them over your home Wi-Fi connection: the _receiver_.

The code in this repo is tested and verified to work on LilyGO TTGO T3 LoRa32 868MHz V1.6.1 ESP32 boards, but should work for any ESP32 Pico D4 with a Semtech SX1276/77/78/79 radio. YMMV.

**NOTE**: this code is currently just a technology demonstration!

## Features
- Transmit P1 meter telegrams in (near) real-time from locations outside WiFi range
- Auto-negotiation of LoRa RF channel parameters ("LoRa handshake") between transmitter and receiver to achieve optimal data throughput
- Auto-tune of LoRa RF channel parameters during operation to ensure optimal throughput even if the RF (Radio Frequency) environment changes
- AES256 encryption of meter data over LoRa
- MQTT client on receiver side
- Home Assistant integration

## Use case
Most devices enabling real-time access to P1 meter data (“_dongles_”) use WiFi. Flats and condos, however, often have their utility meters beyond Wi-Fi access, e.g. in a basement or another shared space. This repo contains code to use LoRa to connect P1 utility meters to a base station connected to your home WiFi network. LoRa is a radio protocol designed to carry small amounts of data over long distances and/or challenging environments like multiple floors. _So it’s like the very long cat, except that there is no cat._

Why roll your own base station, in stead of relying on a LoRaWAN, NB-IoT or other public networks?
- Cost: no monthly charges whatsoever.
- Update rate: most public networks limit airtime to 0.1%, meaning you can get, at most, one update every 15 min (if you are lucky and have an expensive data plan). This firmware gives you updates every ~30s to 6m.
- Privacy: your meter data does not travel any third party network, nor is it processed by anyone else but yourself or the provider of your own choosing.

## Installation
### Preparation
This firmware is meant for compiling in the Arduino 1.x IDE use the ESP32 Arduino core 2.0.7 or later.

You will need some additional libraries.
* In Arduino IDE Go to _Sketch_>_Include Library_>_Manage Libraries_. Search for and install the following libraries
  * `LoRa` by Sandeep Mistry version **0.8.0**
  * `PubSubClient` by Nick O'Leary version **2.8.0**
  * `ArduinoJSon` by Benoit Blanchon version **6.19.4**
  * `elapsedMillis` by Peter Feerick version **1.0.6**

By default, the code in this repo is written for LilyGO TTGO T3 LoRa32 OLED boards. However, it should  work with any ESP32 Pico D4 connected to a Semtech SX1276/77/78/79 radio. You might need to change some code.

### Hardware
To connect the transmitting ESP32 to your digital meters' P1 port, you will need a level shifter to transform the 5V P1 signal to an 3.3 inverted signal which can be fed to the ESP32. You can find a [suitable schematic here](https://github.com/plan-d-io/P1-dongle/wiki/Build:-DIY-instructions#building-from-scratch). Note that this firmware uses ESP32 pins 12 and 13 as RX and TX.

### Compiling
The code in **this** repository is meant for the _receiver_, meaning the ESP32 which will act as a base station, receiving P1 telegrams over LoRa and forwarding them over your home WiFi. 

Compile and flash the code in this repository to the _transmitter_, the ESP32 connected to your digital meter.

Ensure the `networkNum`, `plaintextKey` and `networkID` variables are set to identical values on both transmitter or receiver, or else communication will fail.

### Installation
- Flash the receiver first. Place it on a suitable location (see FAQ below) and power it up. The receiver will start to listen to transmitter broadcast packets on SF12 BW125.
- Flash the transmitter. Connect it to your digital meters' P1 port through the level shifter. It will start transmitting broadcast packets on SF12 BW125.
- If both transmitter and receiver can hear each other, they will start a handshake to determine the optimal RF channel settings.
- Once the handshake is concluded, the transmitter will start forwarding the P1 meter telegrams.
- If you have provided valid MQTT settings in the receiver firmware, it will start forwarding P1 meter data over MQTT.
- If you have provided valid Home Assistant settings in the receiver firmware, it will create a MQTT device in HA and update it with every received P1 telegram (note: you need to have an MQTT broker running).

## About LoRa
LoRa operates in the unlicensed ISM radio spectrum. Anyone is free to use this slice of spectrum as long as they adhere to a maximum use time, defined as the duty cycle limit. For EU 868MHz, the maximum duty cycle is 1%. So if there are 86400 seconds in a day, this means your device can transmit a total of 86400 x 1% = 864 seconds per day. This is called the _air time_.

Air time is dependent on the size of the payload (three-phase meters have a larger payload than single-phase meters), symbol encoding and RF channel settings (bandwidth _BW_ and spreading factor _SF_). 

<table>
  <tr>
   <td>SF</td>
   <td>BW</td>
   <td colspan="3">Single-phase telegram</td>
   <td colspan="3">Three-phase telegram</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td>Airtime (ms)</td>
    <td>Telegrams/h</td>
    <td>Time between telegrams (s)</td>
    <td>Airtime (ms)</td>
    <td>Telegrams/h</td>
    <td>Time between telegrams (s)</td>
  </tr>
  <tr>
    <td>12</td>
    <td>125</td>
    <td>2305</td>
    <td>15,6</td>
    <td>230,5</td>
    <td>3723</td>
    <td>9,6</td>
    <td>372,3</td>
  </tr>
  <tr>
    <td>12</td>
    <td>250</td>
    <td>1072</td>
    <td>33,6</td>
    <td>107,2</td>
    <td>1731</td>
    <td>20,8</td>
    <td>173,2</td>
  </tr>
  <tr>
    <td>11</td>
    <td>250</td>
    <td>578</td>
    <td>62,3</td>
    <td>57,8</td>
    <td>934</td>
    <td>38,6</td>
    <td>93,4</td>
  </tr>
  <tr>
    <td>10</td>
    <td>250</td>
    <td>311</td>
    <td>115,8</td>
    <td>31,1</td>
    <td>502</td>
    <td>71,7</td>
    <td>50,2</td>
  </tr>
  <tr>
    <td>9</td>
    <td>250</td>
    <td>167</td>
    <td>215,6</td>
    <td>16,7</td>
    <td>270</td>
    <td>133,4</td>
    <td>27</td>
  </tr>
  <tr>
    <td>8</td>
    <td>250</td>
    <td>90</td>
    <td>400</td>
    <td>9</td>
    <td>145</td>
    <td>247,6</td>
    <td>14,5</td>
  </tr>
  <tr>
    <td>8</td>
    <td>250</td>
    <td>45</td>
    <td>800</td>
    <td>4,5</td>
    <td>70</td>
    <td>500</td>
    <td>7</td>
  </tr>
</table>

_Indicative air time and update rates for different bandwidth settings_

Each increment in SF or decrement in BW roughly halves the amount of LoRa airtime and, as such, doubles update rates (see table above). Symbol encoding is kept default on this firmware.

A lower BW (125 vs 250) might help with penetration of challenging RF environments (e.g. multiple solid walls), but also increases the chances of clock mismatch between transmitter and receiver. This can especially be a problem with cheaper LoRa modules, or transmitter/receiver modules from different manufacturers. By default, this firmware only uses BW 125 during the first RF handshake step, switching to BW 250 for all steps afterwards.

## LoRa RF channel handshake
One of the special features of this firmware is the LoRa handshake, by which transmitter and receiver negotiate the best possible RF settings to enable the fastest but still reliable update rate. This works as follow:
- The receiver starts up, connects to your Wi-Fi, and starts listening to LoRa broadcasts from the transmitter on SF12 BW125.
- Once connected to your digital meter, the transmitters starts up and sends discovery packets on SF12 BW125. This is the lowest bandwidth supported, but it does have the best range.
- Once the receiver receives the discovery packet, a handshake between the transmitter and receiver is initiated by exchanging (virtual) meter telegrams (by the transmitter) and CRC acknowledgements (by the receiver) at increasingly higher bandwidth settings. These virtual meter telegrams contain the same amount of data of single-phase meter telegram, and are used to assess the RF channel performance.
- Both transmitter and receiver eventually settle on the highest bandwidth setting still allowing reliable communication (packet loss < 50%).
- Once the transmitter and receiver are synced, real P1 meter telegrams are exchanged. The update rate is dependent on the RF channel settings to comply to the 1% duty cycle limit.

## RF channel monitoring
Another special feature of this firmware is continous RF channel performance monitoring. 

LoRa RF channel performance might change during the day, e.g. damp vs dry weather or a car parked in front of your digital meter in the basement of your apartment building. Both transmitter and receiver keep track of RF channel performance by exchanging CRC acknowledgments of transmitted meter telegrams. As such, the transmitter can determine how many transmitted meter telegrams have reached the receiver. If packet loss is more than 50%, a new RF handshake is initiated to settle on more reliable RF channel parameters. 

Likewise, if the receiver has not received any P1 meter telegram for more than 15 minutes it reverts back to RF handshake mode. By doing so, both transmitter and receiver eventually revert back to handshake mode if communication is lost for longer periods, allowing them to re-establish succesful communication.

Additionally, if packet loss is below 15%, a new handshake is initiated to settle on settings providing higher throughput. This all happens automatically. 

## FAQ & troubleshooting
> I once attended a wedding of two antennas. The ceremony wasn't great, but the reception was excellent.  
> \-  The author of this repository (attributed)

### Tips to improve reception quality
#### Antenna orientation
You are most probably using a [rubber ducky antenna](https://en.wikipedia.org/wiki/Rubber_ducky_antenna) on both transmitter and receiver. These monopole antennas radiate vertically polarized radio waves. In short, this means you need to align the antenna orientation of both transmitter and receiver in the same plane for optimal reception quality. Additionally, monopoles have a radiation pattern that more or less resembles  a donut. This means that there "dead spots" exactly above and below the antenna.

So, if the transmitter and receiver are located a long _horizontal_ distance apart, point both antennas _vertically_. If the transmitter and receiver are located a long _vertical_ distance apart (e.g. a basement level directly below), you might have better luck pointing the antennas _horizontally_. For anything in between, try to align them on an imaginary common plain.

Note that LoRa uses [chirp spread spectrum](https://en.wikipedia.org/wiki/Chirp_spread_spectrum), enabling it to benefit from multipath propagation. This means that the transmitter could, _potentially_, more easily reach the receiver by bouncing its signals off on other objects, like nearby facades, than it would by trying the shortest path through challenging RF objects (e.g. reinforced concrete floors). Try different positions for the antennas if perfomarnce isn't gooed. RF isn't a precise science, YMMV.

#### Indoor receiver location
To ensure reliable communication, the location of the receiver is even more important than the location of the transmitter. LoRa has a property called _light indoor reception_, meaning reception quality quickly fades (due to [destructive interference](https://en.wikipedia.org/wiki/Wave_interference)) as the receiver is placed more indoors. Therefore it is recommended to place the receiver as close as possible to a window facing the shortest (RF) path to the receiver.
