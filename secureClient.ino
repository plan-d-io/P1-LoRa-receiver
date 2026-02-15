/*
 * secureClient.ino - NetworkClientSecure with hardcoded GitHub CA for version check, OTA, and TLS bundle restore.
 * No cert bundle from LittleFS yet; only setCACert(github_root_ca).
 */
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <LittleFS.h>

/* Root CA for raw.githubusercontent.com. Assessment: with setInsecure() HTTPS works, so the
 * failure is certificate verification only. The server's chain no longer uses "DigiCert Global
 * Root CA" (below); it likely uses DigiCert Global Root G2. To fix: (1) On your PC run
 *   openssl s_client -connect raw.githubusercontent.com:443 -showcerts </dev/null 2>/dev/null
 * (Windows: use <nul 2>nul). Copy the LAST certificate block (-----BEGIN...-----END-----)
 * and replace the string below with it. Or (2) download DigiCert Global Root G2 PEM from
 * https://www.digicert.com/kb/digicert-root-certificates.htm and use that as github_root_ca. */
 static const char* github_root_ca = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIFgTCCBGmgAwIBAgIQOXJEOvkit1HX02wQ3TE1lTANBgkqhkiG9w0BAQwFADB7\n" \
    "MQswCQYDVQQGEwJHQjEbMBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYD\n" \
    "VQQHDAdTYWxmb3JkMRowGAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UE\n" \
    "AwwYQUFBIENlcnRpZmljYXRlIFNlcnZpY2VzMB4XDTE5MDMxMjAwMDAwMFoXDTI4\n" \
    "MTIzMTIzNTk1OVowgYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5\n" \
    "MRQwEgYDVQQHEwtKZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBO\n" \
    "ZXR3b3JrMS4wLAYDVQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0\n" \
    "aG9yaXR5MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAgBJlFzYOw9sI\n" \
    "s9CsVw127c0n00ytUINh4qogTQktZAnczomfzD2p7PbPwdzx07HWezcoEStH2jnG\n" \
    "vDoZtF+mvX2do2NCtnbyqTsrkfjib9DsFiCQCT7i6HTJGLSR1GJk23+jBvGIGGqQ\n" \
    "Ijy8/hPwhxR79uQfjtTkUcYRZ0YIUcuGFFQ/vDP+fmyc/xadGL1RjjWmp2bIcmfb\n" \
    "IWax1Jt4A8BQOujM8Ny8nkz+rwWWNR9XWrf/zvk9tyy29lTdyOcSOk2uTIq3XJq0\n" \
    "tyA9yn8iNK5+O2hmAUTnAU5GU5szYPeUvlM3kHND8zLDU+/bqv50TmnHa4xgk97E\n" \
    "xwzf4TKuzJM7UXiVZ4vuPVb+DNBpDxsP8yUmazNt925H+nND5X4OpWaxKXwyhGNV\n" \
    "icQNwZNUMBkTrNN9N6frXTpsNVzbQdcS2qlJC9/YgIoJk2KOtWbPJYjNhLixP6Q5\n" \
    "D9kCnusSTJV882sFqV4Wg8y4Z+LoE53MW4LTTLPtW//e5XOsIzstAL81VXQJSdhJ\n" \
    "WBp/kjbmUZIO8yZ9HE0XvMnsQybQv0FfQKlERPSZ51eHnlAfV1SoPv10Yy+xUGUJ\n" \
    "5lhCLkMaTLTwJUdZ+gQek9QmRkpQgbLevni3/GcV4clXhB4PY9bpYrrWX1Uu6lzG\n" \
    "KAgEJTm4Diup8kyXHAc/DVL17e8vgg8CAwEAAaOB8jCB7zAfBgNVHSMEGDAWgBSg\n" \
    "EQojPpbxB+zirynvgqV/0DCktDAdBgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rID\n" \
    "ZsswDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQFMAMBAf8wEQYDVR0gBAowCDAG\n" \
    "BgRVHSAAMEMGA1UdHwQ8MDowOKA2oDSGMmh0dHA6Ly9jcmwuY29tb2RvY2EuY29t\n" \
    "L0FBQUNlcnRpZmljYXRlU2VydmljZXMuY3JsMDQGCCsGAQUFBwEBBCgwJjAkBggr\n" \
    "BgEFBQcwAYYYaHR0cDovL29jc3AuY29tb2RvY2EuY29tMA0GCSqGSIb3DQEBDAUA\n" \
    "A4IBAQAYh1HcdCE9nIrgJ7cz0C7M7PDmy14R3iJvm3WOnnL+5Nb+qh+cli3vA0p+\n" \
    "rvSNb3I8QzvAP+u431yqqcau8vzY7qN7Q/aGNnwU4M309z/+3ri0ivCRlv79Q2R+\n" \
    "/czSAaF9ffgZGclCKxO/WIu6pKJmBHaIkU4MiRTOok3JMrO66BQavHHxW/BBC5gA\n" \
    "CiIDEOUMsfnNkjcZ7Tvx5Dq2+UUTJnWvu6rvP3t3O9LEApE9GQDTF1w52z97GA1F\n" \
    "zZOFli9d31kWTz9RvdVFGD/tSo7oBmF0Ixa1DVBzJ0RHfxBdiSprhTEUxOipakyA\n" \
    "vGp4z7h/jnZymQyd/teRCBaho1+V\n" \
    "-----END CERTIFICATE-----\n";

#define TLSBUNDLE_PATH "/x509_crt_bundle.bin"

/* Build version URL for plan-d-io/P1-LoRa-receiver (branch from config). */
static String getVersionUrl() {
  String base = "https://raw.githubusercontent.com/plan-d-io/P1-LoRa-receiver/";
  if (_dev_fleet) base += "develop/version";
  else if (_alpha_fleet) base += "alpha/version";
  else if (_v2_fleet) base += "V2-0/version";
  else base += "main/version";
  return base;
}

/* Host and path for version check (same repo). Use with begin(client, host, port, path, true) for SNI. */
static const char* GITHUB_RAW_HOST = "raw.githubusercontent.com";
static String getVersionPath() {
  String path = "/plan-d-io/P1-LoRa-receiver/";
  if (_dev_fleet) path += "develop/version";
  else if (_alpha_fleet) path += "alpha/version";
  else if (_v2_fleet) path += "V2-0/version";
  else path += "main/version";
  return path;
}

void setupSecureClientWithGitHubCA() {
  if (secureClient != nullptr) {
    syslog("Secure client already created", 0);
    return;
  }
  syslog("Creating NetworkClientSecure with GitHub CA", 1);
  secureClient = new NetworkClientSecure;
  if (!secureClient) {
    syslog("Failed to allocate NetworkClientSecure", 3);
    bundleLoaded = false;
    return;
  }
  /* Set CA for GitHub. We use DigiCert Global Root G2 (raw.githubusercontent.com's current chain).
   * If verification fails, get the live root: run
   *   openssl s_client -connect raw.githubusercontent.com:443 -showcerts </dev/null 2>/dev/null
   * and copy the last certificate block (-----BEGIN...-----END-----) into github_root_ca below. */
  secureClient->setCACert(github_root_ca);
  bundleLoaded = true;
  syslog("Secure client ready (GitHub CA)", 1);
}

/* Test HTTPS connection by fetching the version file from GitHub. Log result to syslog and Serial.
 * Use full URL with begin(client, url) as in official BasicHttpsClient example. */
bool testSecureConnection() {
  if (!secureClient || !bundleLoaded) {
    syslog("HTTPS test skipped: secure client not ready", 2);
    return false;
  }
  String url = getVersionUrl();
  syslog("HTTPS test: GET " + url, 0);
  if (httpDebug) {
    Serial.println("[HTTPS] GET " + url);
    IPAddress ip;
    if (WiFi.hostByName(GITHUB_RAW_HOST, ip)) {
      Serial.println("[HTTPS] DNS (WiFi): " + ip.toString());
    } else {
      Serial.println("[HTTPS] DNS (WiFi) failed");
    }
  }
  bool ok = false;
  if (https.begin(*secureClient, url)) {
    https.setConnectTimeout(20000);
    https.setTimeout(15000);
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        payload.trim();
        syslog("HTTPS test OK, HTTP " + String(httpCode) + ", body: " + payload, 1);
        if (httpDebug) Serial.println("[HTTPS] OK body: " + payload);
        ok = true;
        secureClientError = 0;
      } else {
        syslog("HTTPS test unexpected code " + String(httpCode), 2);
        if (httpDebug) Serial.println("[HTTPS] code " + String(httpCode));
      }
    } else {
      syslog("HTTPS test failed: error " + String(httpCode) + " " + String(https.errorToString(httpCode)), 2);
      if (httpDebug) Serial.println("[HTTPS] error " + String(httpCode) + " " + String(https.errorToString(httpCode)));
      secureClientError++;
    }
    https.end();
  } else {
    syslog("HTTPS test: begin() failed", 2);
    if (httpDebug) Serial.println("[HTTPS] begin failed");
  }
  return ok;
}

/* Restore TLS bundle from GitHub into LittleFS (for future use). Uses GitHub CA only. */
void restoreTLSBundle() {
  if (!secureClient) {
    syslog("Restore: secure client not ready", 3);
    return;
  }
  secureClient->setCACert(github_root_ca);
  syslog("Restore: downloading TLS bundle to LittleFS", 1);
  String fileUrl = "https://raw.githubusercontent.com/plan-d-io/P1-dongle/main/data/x509_crt_bundle.bin";
  if (httpDebug) Serial.println("[Restore] " + fileUrl);
  File f = LittleFS.open(TLSBUNDLE_PATH, "w");
  if (!f) {
    syslog("Restore: could not open " + String(TLSBUNDLE_PATH) + " for write", 3);
    return;
  }
  bool writtenOk = false;
  if (https.begin(*secureClient, fileUrl)) {
    https.setConnectTimeout(20000);
    https.setTimeout(30000);
    int httpCode = https.GET();
    if (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND)) {
      long contentLength = https.getSize();
      syslog("Restore: bundle size " + String(contentLength), 0);
      size_t written = https.writeToStream(&f);
      if (written == (size_t)contentLength) {
        syslog("Restore: written " + String(written) + " bytes", 1);
        writtenOk = true;
      } else {
        syslog("Restore: wrote " + String(written) + "/" + String(contentLength), 2);
      }
    } else {
      syslog("Restore: HTTP " + String(httpCode) + " " + String(https.errorToString(httpCode)), 2);
    }
    https.end();
  } else {
    syslog("Restore: begin() failed", 2);
  }
  f.close();
  if (!writtenOk) {
    LittleFS.remove(TLSBUNDLE_PATH);
    syslog("Restore: removed partial file", 1);
    return;
  }
  _restore_finish = false;
  _reinit_spiffs = false;
  saveResetReason("Rebooting after TLS bundle restore");
  saveConfig();
  syslog("Restore: rebooting", 1);
  delay(500);
  ESP.restart();
}
