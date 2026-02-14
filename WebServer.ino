/*
 * WebServer.ino - Async web server routes for config, data, wifi scan, static pages.
 * LittleFS for syslog.
 * Large PROGMEM (index_html, css, reboot_html) sent chunked to avoid allocation panic.
 */
#include <pgmspace.h>
#include <freertos/semphr.h>
extern SemaphoreHandle_t syslogMutex;

/* Stream PROGMEM content in chunks to avoid large send_P buffer (causes panic on connect). */
static void sendChunkedProgmem(AsyncWebServerRequest* request, const char* contentType, const char* content) {
  AsyncWebServerResponse* response = request->beginChunkedResponse(contentType, [content](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
    size_t totalLen = strlen_P(content);
    if (index >= totalLen) return 0;
    size_t toSend = (totalLen - index) > maxLen ? maxLen : (totalLen - index);
    memcpy_P(buffer, content + index, toSend);
    return toSend;
  });
  request->send(response);
}

void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", index_html);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->params() == 0) {
      request->send(200, "application/json", configBuffer);
      return;
    }
    String response, foundInConfig;
    for (size_t i = 0; i < request->params(); i++) {
      const AsyncWebParameter* p = request->getParam(i);
      int retVarType, retVarNum;
      if (findInConfig(p->name().c_str(), retVarType, retVarNum)) {
        if (p->value() != "") storeConfigVar(p->value(), retVarType, retVarNum);
        foundInConfig = returnConfigVar(p->name().c_str(), retVarType, retVarNum, 1);
        if (foundInConfig != "") {
          response += foundInConfig.substring(1, foundInConfig.length() - 1);
          response += ",";
        }
      }
    }
    if (response != "") {
      response = "{" + response.substring(0, response.length() - 1) + "}";
      request->send(200, "application/json", response);
    } else request->send(404, "text/plain");
  });

  auto configBodyHandler = [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    (void)index;
    if (len < 1 || total < 1) {
      request->send(404, "text/plain", "Empty body");
      return;
    }
    if (request->url() != "/config" && request->url() != "/config/") {
      request->send(404, "text/plain");
      return;
    }
    String jsonResponse;
    String body = String((const char*)data).substring(0, total);
    if (processConfigJson(body, jsonResponse, true)) {
      request->send(200, "application/json", jsonResponse);
    } else {
      String configResponse;
      processConfigString(body, configResponse, true);
      if (configResponse != "") request->send(200, "application/json", configResponse);
      else request->send(404, "text/plain", "Invalid config");
    }
  };

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, nullptr, configBodyHandler);
  server.on("/config", HTTP_PUT, [](AsyncWebServerRequest* request) {}, nullptr, configBodyHandler);
  server.on("/config/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", configBuffer);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (request->params() == 0) {
      request->send(200, "application/json", httpTelegramValues(""));
      return;
    }
    const AsyncWebParameter* p = request->getParam(0);
    request->send(200, "application/json", httpTelegramValues(p->name().c_str()));
  });

  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", ssidList);
  });
  server.on("/wifi/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", ssidList);
  });

  server.on("/loraset", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", loraSettings());
  });
  server.on("/loraset/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", loraSettings());
  });

  server.on("/releasechan", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", releaseChannels());
  });
  server.on("/releasechan/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", releaseChannels());
  });

  server.on("/payloadformat", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", payloadFormat());
  });
  server.on("/payloadformat/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", payloadFormat());
  });

  server.on("/svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", returnSvg());
  });

  server.on("/info", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", infoMsg);
  });

  server.on("/hostname", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", apSSID);
  });

  server.on("/email", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", _user_email);
  });

  server.on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", index_html);
  });
  server.on("/test/", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", index_html);
  });

  server.on("/reboot.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", reboot_html);
  });

  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", reboot_html);
    saveResetReason("Reboot requested from webmin");
    setReboot();
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/css", css);
  });

  server.on("/syslog", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!spiffsMounted) {
      request->send(200, "text/plain", "LittleFS not mounted. Log output is Serial only.");
      return;
    }
    if (!LittleFS.exists("/syslog.txt")) {
      request->send(200, "text/plain", "No log entries yet.\n");
      return;
    }
    /* Read file under mutex and send copy to avoid concurrent read/write crash with syslog(). */
    if (syslogMutex != NULL) xSemaphoreTake(syslogMutex, portMAX_DELAY);
    String body = readFileToString(LittleFS, "/syslog.txt", 6000);
    if (syslogMutex != NULL) xSemaphoreGive(syslogMutex);
    request->send(200, "text/plain", body);
  });
  server.on("/syslog0", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!spiffsMounted) {
      request->send(200, "text/plain", "LittleFS not mounted.");
      return;
    }
    if (!LittleFS.exists("/syslog0.txt")) {
      request->send(200, "text/plain", "No previous log file.\n");
      return;
    }
    if (syslogMutex != NULL) xSemaphoreTake(syslogMutex, portMAX_DELAY);
    String body = readFileToString(LittleFS, "/syslog0.txt", 6000);
    if (syslogMutex != NULL) xSemaphoreGive(syslogMutex);
    request->send(200, "text/plain", body);
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(404, "text/plain");
  });

  server.onNotFound([](AsyncWebServerRequest* request) {
    sendChunkedProgmem(request, "text/html", index_html);
  });

  /* Do not call server.begin() here - see startServer() called from loop() after delay.
   * Avoids lwIP "Required to lock TCPIP core" assert on ESP32 Arduino 3.x when TCP stack
   * is not yet fully ready. */
}

void startServer() {
  static bool started = false;
  if (started) return;
  server.begin();
  started = true;
  syslog("HTTP server started", 1);
}
