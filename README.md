# P1-LoRa-receiver MVP

Minimal firmware for **M5Stack Atom Lite**: web server, WiFi (STA/AP), NTP, and NVS config. No LoRa, MQTT, or HTTP upload. Use this build to verify the web UI and housekeeping before adding the rest.

## Target board

- **M5Stack Atom Lite** (ESP32-PICO-D4, built-in RGB LED on GPIO 27, button on GPIO 39).

## Requirements

- **Arduino IDE 2.x**
- **ESP32 board support** (espressif/arduino-esp32), preferably **3.x**
- **Board**: **M5Stack-ATOM** or **M5Stack Atom Lite** (from M5Stack board package or ESP32 Arduino)

### Libraries (Arduino Library Manager)

1. **AsyncTCP** by **ESP32Async**
2. **ESPAsyncWebServer** by **ESP32Async**
3. **ArduinoJson** by Benoit Blanchon (6.x or 7.x)
4. **elapsedMillis** by Peter Feerick
5. **UUID** by Rob Tillaart
6. **Adafruit NeoPixel** by Adafruit (for built-in RGB LED on Atom Lite)

### Install steps

1. **Arduino IDE** → Sketch → Include Library → Manage Libraries.
2. Search and install: **AsyncTCP** (ESP32Async), **ESPAsyncWebServer** (ESP32Async), **ArduinoJson**, **elapsedMillis**, **UUID**, **Adafruit NeoPixel**.
3. **Tools** → **Board** → **ESP32 Arduino** → **M5Stack-ATOM** (or **M5Stack Atom Lite** if listed).
4. **Tools** → **Partition Scheme** → e.g. **Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)** or **Minimal SPIFFS** if you use OTA later.
5. **Tools** → **Upload Speed** → **115200** (or 921600 if stable).

## Build and upload

1. Open `P1-LoRa-receiver.ino` in Arduino IDE.
2. Connect the M5Stack Atom Lite via USB.
3. **Tools** → **Port** → select the correct COM port.
4. **Sketch** → **Upload**.

## First run

- If no WiFi credentials are stored, the device starts in **AP mode** with SSID `p1receiver` (or `P1xxxxxx` from MAC).
- Connect your PC/phone to that WiFi, then open **http://192.168.4.1** (or the AP IP shown in Serial).
- In the web UI: choose your home WiFi, set password, submit. Device reboots and connects in STA mode.
- In STA mode, use the device’s LAN IP (see Serial Monitor) to open the web UI again.

## API (for the web UI)

- `GET /config` – full config JSON  
- `GET /config?KEY=value` – set one config key, response JSON  
- `PUT /config` or `POST /config` – body: JSON config (e.g. `{"WIFI_SSID":"MySSID","WIFI_PASSWD":"secret"}`)  
- `GET /wifi` – list of scanned SSIDs (JSON)  
- `GET /data` – placeholder meter data (JSON array)  
- `GET /loraset`, `GET /releasechan`, `GET /payloadformat` – options for dropdowns  
- `GET /svg` – status icons (wifi/meter/cloud/broker)  
- `GET /info`, `GET /hostname`, `GET /email` – text  
- `GET /reboot` – show reboot page and trigger reboot  
- `GET /style.css` – stylesheet  
- `GET /syslog`, `GET /syslog0` – in this build: plain text message (no filesystem; log is Serial only)  

## Serial

- **115200** baud.
- Logs and debug go to Serial; `httpDebug` in the main .ino can be set to `true` for request logging.

## Next steps (not in this MVP)

- LoRa receive and parsing  
- MQTT publish  
- HTTP(S) push  
- Home Assistant / EnergieID  

This MVP keeps the same file layout and config/API shape as the full project so you can add those features back in step by step.
