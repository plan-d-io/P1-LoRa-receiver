void setupServer(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      Serial.println("Index, returning index_html");
      request->send_P(200, "text/html", index_html);
  });
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    if(httpDebug){
      Serial.print("GET to ");
      Serial.println(request->url());
    }
    int params = request->params();
    if(request->url() == "/config" || request->url() == "/config/"){
      /*Request to query or update the configuration. First check if the request has arguments corresponding to NVS key names*/
      if(params == 0){
        /*If not, return the full configuration as JSON*/
        //request->send(200, "application/json", returnConfig()); 
        request->send(200, "application/json", configBuffer); 
      }
      else{
        String response, foundInConfig;
        for(int i=0; i<params; i++){
          const AsyncWebParameter* p = request->getParam(i);
          int retVarType, retVarNum;
          if(findInConfig(p->name().c_str(), retVarType, retVarNum)){
            /*Check if the NVS key name passed as argument exists*/ 
            if(p->value() != ""){
             /*If a value is passed as well, update the associated variable in its respective data store*/
             storeConfigVar(p->value(), retVarType, retVarNum);
            }
            /*Build a JSON response containing the new value for every updated key, concatenate if there are multiple*/
            foundInConfig = returnConfigVar(p->name().c_str(), retVarType, retVarNum, 1);
            if(foundInConfig != ""){
              response += foundInConfig.substring(1, foundInConfig.length()-1);
              response += ",";
            }
          }
        }
        /*Tidy up the concatenation*/
        if(response != ""){
          response = response.substring(0, response.length()-1);
          response = "{" + response;
          response += "}";
          request->send(200, "application/json", response);
        }
        else request->send(404, "text/plain");
      }
    }
    
  });
  // Handle POST to /config
  // Shared body handler lambda
  auto configBodyHandler = [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (httpDebug) {
      Serial.print("POST/PUT/PATCH to ");
      Serial.println(request->url());
    }
  
    if (len > 1 && (request->url() == "/config" || request->url() == "/config/")) {
      String jsonResponse;
  
      // Try JSON config
      if (processConfigJson((const char*)data, jsonResponse, true)) {
        request->send(200, "application/json", jsonResponse);
      } else {
        // Fallback to config string (e.g. form data)
        String configResponse;
        String safeString = String((const char*)data).substring(0, total);
        processConfigString(safeString, configResponse, true);
  
        if (configResponse != "") {
          request->send(200, "application/json", configResponse);
        } else {
          request->send(404, "text/plain", "Invalid config string");
        }
      }
    } else {
      request->send(404, "text/plain", "Invalid request or empty payload");
    }
  };
  
  // Register for POST
  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request) {},
            nullptr, configBodyHandler);
  
  // Register for PUT
  server.on("/config", HTTP_PUT, [](AsyncWebServerRequest *request) {},
            nullptr, configBodyHandler);
  
  // Optionally, register for PATCH as well
  server.on("/config", HTTP_PATCH, [](AsyncWebServerRequest *request) {},
            nullptr, configBodyHandler);

  // Handle GET requests for different routes
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("GET /data request received");
    Serial.println(httpTelegramValues(""));
    if(request->params() == 0) {
      //request->send(200, "application/json", ssidList);
      request->send(200, "application/json", httpTelegramValues(""));
    } 
    else {
      const AsyncWebParameter* p = request->getParam(0, false);
      //Serial.println(p->name().c_str());
      request->send(200, "application/json", ssidList);
      //request->send(200, "application/json", httpTelegramValues(p->name().c_str()));
    }
  });
  
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /wifi request received");
      request->send(200, "application/json", ssidList);
  });
  
  server.on("/loraset", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /loraset request received");
      request->send(200, "application/json", loraSettings());
  });
  
  server.on("/releasechan", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /releasechan request received");
      request->send(200, "application/json", releaseChannels());
  });
  
  server.on("/payloadformat", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /payloadformat request received");
      request->send(200, "application/json", payloadFormat());
  });
  
  server.on("/svg", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /svg request received");
      request->send(200, "application/json", returnSvg());
  });
  
  server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /info request received");
      request->send(200, "text/plain", infoMsg);
  });
  
  server.on("/hostname", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /hostname request received");
      request->send(200, "text/plain", apSSID);
  });
  
  server.on("/email", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /email request received");
      request->send(200, "text/plain", _user_email);
  });
  
  // Handle SPIFFS-based web pages and assets
  server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /test request received");
      request->send_P(200, "text/html", index_html);
  });
  
  server.on("/test/", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /test/ request received");
      request->send_P(200, "text/html", index_html);
  });
  
  server.on("/reboot.html", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /reboot.html request received");
      request->send_P(200, "text/html", reboot_html);
  });
  
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /reboot request received");
      request->send_P(200, "text/html", reboot_html);
      saveResetReason("Reboot requested from webmin");
      setReboot();
  });
  
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /style.css request received");
      request->send_P(200, "text/css", css);
  });
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /script.js request received");
      request->send_P(200, "text/javascript", script_js);
  });
  // Handle SPIFFS file retrieval
  server.on("/syslog", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /syslog request received");
      request->send(SPIFFS, "/syslog.txt", "text/plain");
  });
  
  server.on("/syslog0", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /syslog0 request received");
      request->send(SPIFFS, "/syslog0.txt", "text/plain");
  });
  
  // Handle favicon.ico (empty response)
  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("GET /favicon.ico request received");
      request->send(SPIFFS, "", "text/plain");
  });
  
  // Handle all other requests (fallback)
  server.onNotFound([](AsyncWebServerRequest *request) {
    Serial.print("Unhandled request received: ");
    Serial.println(request->url());  // Print the requested URL
    request->send(404, "text/plain");
  });

  server.begin();
}
