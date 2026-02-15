/*
 * upgrade.ino - OTA firmware update and finish-update flow using GitHub + hardcoded CA.
 */
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <LittleFS.h>

/* Base URL for OTA binary on raw.githubusercontent.com (branch from config). */
static String getFirmwareBaseUrl() {
  String base = "https://raw.githubusercontent.com/plan-d-io/P1-LoRa-receiver/";
  if (_dev_fleet) base += "develop/";
  else if (_alpha_fleet) base += "alpha/";
  else if (_v2_fleet) base += "V2-0/";
  else base += "main/";
  return base;
}

bool checkUpdate() {
  if (!_update_autoCheck || !secureClient || !bundleLoaded) return false;
  clientSecureBusy = true;
  bool needUpdate = false;
  String checkUrl = getVersionUrl();
  syslog("OTA check: GET " + checkUrl, 0);
  bool gotVersion = false;
  if (https.begin(*secureClient, checkUrl)) {
    https.setConnectTimeout(20000);
    https.setTimeout(15000);
    int httpCode = https.GET();
    if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)) {
      String payload = https.getString();
      payload.trim();
      onlineVersion = (unsigned int)atoi(payload.c_str());
      syslog("OTA check: online version " + String(onlineVersion / 100.0), 1);
      secureClientError = 0;
      gotVersion = true;
      if (_rebootSecure > 0) {
        _rebootSecure = 0;
        saveConfig();
      }
    } else {
      if (httpCode > 0) syslog("OTA check: HTTP " + String(httpCode), 2);
      else {
        syslog("OTA check failed: error " + String(httpCode) + " " + String(https.errorToString(httpCode)), 2);
        secureClientError++;
      }
    }
    https.end();
  } else {
    syslog("OTA check: begin() failed", 2);
  }
  if (!gotVersion) {
    syslog("OTA check: retrying with GitHub CA (bundle may not verify this host)", 1);
    useGitHubCAOnly();
    if (https.begin(*secureClient, checkUrl)) {
      https.setConnectTimeout(20000);
      https.setTimeout(15000);
      int httpCode = https.GET();
      if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)) {
        String payload = https.getString();
        payload.trim();
        onlineVersion = (unsigned int)atoi(payload.c_str());
        syslog("OTA check OK with GitHub CA fallback, version " + String(onlineVersion / 100.0), 1);
        secureClientError = 0;
        gotVersion = true;
      }
      https.end();
    }
    useCertBundle();
  }
  clientSecureBusy = false;
  syslog("Firmware current " + String(fw_ver / 100.0) + " online " + String(onlineVersion / 100.0), 0);
  if (onlineVersion > fw_ver) {
    needUpdate = true;
    syslog("OTA: update available", 1);
  } else {
    syslog("OTA: no update available", 1);
  }
  return needUpdate;
}

bool startUpdate() {
  if (!(_update_auto && (fw_ver < onlineVersion || _update_start)) && !_update_start) return false;
  if (!secureClient || !bundleLoaded) {
    syslog("OTA start: secure client not ready", 2);
    _update_start = false;
    return false;
  }
  syslog("OTA start: preparing firmware upgrade", 1);
  clientSecureBusy = true;
  String fileUrl = getFirmwareBaseUrl() + "P1-LoRa-receiver-mvp.ino.m5stack_atom.bin";
  syslog("OTA: GET " + fileUrl, 0);
  bool begun = https.begin(*secureClient, fileUrl);
  if (!begun) {
    syslog("OTA start: begin() failed", 2);
    clientSecureBusy = false;
    _update_start = false;
    return false;
  }
  https.setConnectTimeout(20000);
  https.setTimeout(120000);  /* firmware download can take 2+ minutes */
  int httpCode = https.GET();
  if (httpCode <= 0 || (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY)) {
    https.end();
    if (httpCode <= 0) {
      syslog("OTA start failed (bundle?): " + String(https.errorToString(httpCode)) + ", retrying with GitHub CA", 1);
      useGitHubCAOnly();
      if (https.begin(*secureClient, fileUrl)) {
        https.setConnectTimeout(20000);
        https.setTimeout(120000);
        httpCode = https.GET();
      } else {
        syslog("OTA start: begin() failed with GitHub CA", 2);
        useCertBundle();
        clientSecureBusy = false;
        _update_start = false;
        secureClientError++;
        return false;
      }
    }
  }
  if (httpCode <= 0) {
    syslog("OTA start failed: " + String(https.errorToString(httpCode)), 2);
    https.end();
    useCertBundle();
    clientSecureBusy = false;
    _update_start = false;
    secureClientError++;
    return false;
  }
  if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY) {
    syslog("OTA start: HTTP " + String(httpCode), 2);
    https.end();
    useCertBundle();
    clientSecureBusy = false;
    _update_start = false;
    return false;
  }
  long contentLength = https.getSize();
  syslog("OTA: firmware size " + String(contentLength), 0);
  if (!Update.begin(contentLength)) {
    syslog("OTA: Update.begin failed (not enough space?)", 3);
    https.end();
    clientSecureBusy = false;
    _update_start = false;
    return false;
  }
  unitState = -1;
  syslog("OTA: writing firmware, wait 2-5 min...", 2);
  size_t written = Update.writeStream(*secureClient);
  https.end();
  if (written != (size_t)contentLength) {
    syslog("OTA: wrote " + String(written) + "/" + String(contentLength), 3);
    Update.abort();
    clientSecureBusy = false;
    _update_start = false;
    return false;
  }
  syslog("OTA: written " + String(written), 1);
  if (!Update.end(true)) {
    syslog("OTA: Update.end failed " + String(Update.getError()), 3);
    clientSecureBusy = false;
    _update_start = false;
    return false;
  }
  syslog("OTA: success, rebooting", 1);
  saveResetReason("Firmware upgrade completed. Rebooting.");
  fw_ver = onlineVersion;
  _update_start = false;
  saveConfig();
  if (spiffsMounted) LittleFS.end();
  delay(500);
  ESP.restart();
  clientSecureBusy = false;
  return true;
}

/* Minimal finish: clear flags and optionally reboot. No static file list download in MVP. */
bool finishUpdate(bool restore) {
  (void)restore;
  syslog("Finish update: clearing flags", 1);
  _update_finish = false;
  _restore_finish = false;
  saveConfig();
  delay(500);
  return true;
}
