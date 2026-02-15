/*
 * secureClient.ino - NetworkClientSecure with embedded cert bundle for HTTPS (EID, MQTT TLS, OTA).
 * The certificate bundle is embedded in flash via x509_crt_bundle.h. All outgoing HTTPS (EID push,
 * MQTT over TLS, OTA update, version check) use the bundle. The hardcoded GitHub root CA is used
 * only as a fallback when a connection to the GitHub update repo fails (e.g. cert chain could
 * not be validated with the bundle), so the device can still OTA.
 *
 * Cert bundle source (ESP-IDF): The bundle must be generated with ESP-IDF's gen_crt_bundle.py
 * and the .bin converted to a C array (e.g. xxd -i x509_crt_bundle.bin). Input: Mozilla/curl PEM.
 * The script outputs a binary (subject DER + public key per cert, sorted by subject). Do NOT
 * convert PEM to a C array directly — that causes "certificates exceed max" or verify failures.
 * Arduino core uses CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_MAX_CERTS (often 200); if your bundle has
 * more certs, use --filter cmn_crt_authorities.csv for a smaller bundle (~38 certs) to test.
 * See: https://github.com/espressif/arduino-esp32/issues/10949
 *
 * Why only some hosts verify with the bundle: The bundle stores each root's *subject* (DER from
 * Python cryptography). Verification compares the server chain's *issuer* (DER from the cert).
 * If the CA encodes issuer with a different RDN order than Python's subject serialization, the
 * byte comparison in esp_crt_bundle fails and you get "Failed to verify certificate" even though
 * the root is in the PEM. So Facebook may match, while GitHub (DigiCert) and Google (GTS) do not.
 * What to try: (1) Use gen_crt_bundle.py from the *same* ESP-IDF version as your Arduino core.
 * (2) Use ESP-IDF's default input (their cacert + cacrt_local.pem if present). (3) Try the
 * filtered bundle. (4) GitHub CA fallback is used when bundle verification fails for GitHub.
 */
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include "x509_crt_bundle.h"

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

/* Switch existing secure client to GitHub CA only. Use only as fallback when connection to
 * GitHub (version check or OTA) fails with the cert bundle (e.g. cert chain not validated). */
void useGitHubCAOnly() {
  if (!secureClient) return;
  secureClient->setCACertBundle(NULL, 0);
  secureClient->setCACert(github_root_ca);
  syslog("Secure client: using GitHub CA only (fallback)", 0);
}

/* Switch back to embedded cert bundle (e.g. after temporarily using GitHub CA). */
void useCertBundle() {
  if (!secureClient) return;
  secureClient->setCACertBundle(x509_crt_bundle, (size_t)x509_crt_bundle_len);
  secureClient->setCACert(nullptr);
  syslog("Secure client: using cert bundle", 0);
}

/* Create client if needed and set embedded cert bundle for arbitrary HTTPS (EID, MQTT TLS). */
void setupSecureClient() {
  if (secureClient != nullptr) {
    syslog("Secure client already created", 0);
    return;
  }
  syslog("Creating NetworkClientSecure with embedded cert bundle", 1);
  secureClient = new NetworkClientSecure;
  if (!secureClient) {
    syslog("Failed to allocate NetworkClientSecure", 3);
    bundleLoaded = false;
    return;
  }
  secureClient->setCACertBundle(x509_crt_bundle, (size_t)x509_crt_bundle_len);
  bundleLoaded = true;
  /* Debug: bundle format is [n x uint32 offsets][cert data]. First uint32 = offset to 1st cert = n*4, so cert count = first_u32/4. */
  if (x509_crt_bundle != nullptr && x509_crt_bundle_len >= 4u) {
    uint32_t firstOffset = (uint32_t)x509_crt_bundle[0] | ((uint32_t)x509_crt_bundle[1] << 8) | ((uint32_t)x509_crt_bundle[2] << 16) | ((uint32_t)x509_crt_bundle[3] << 24);
    unsigned int certCount = firstOffset / 4u;
    syslog("Cert bundle: ptr=" + String((uint32_t)(uintptr_t)x509_crt_bundle, HEX) + " len=" + String(x509_crt_bundle_len) + " certs=" + String(certCount), 0);
  } else {
    syslog("Cert bundle: ptr=" + String(x509_crt_bundle ? "non-null" : "null") + " len=" + String(x509_crt_bundle_len), 0);
  }
  syslog("Secure client ready (cert bundle, " + String(x509_crt_bundle_len) + " bytes)", 1);
}

/* Helper: perform one HTTPS GET to url, return true on HTTP 200/301/302. */
static bool doVersionGet(const String& url) {
  if (!https.begin(*secureClient, url)) return false;
  https.setConnectTimeout(20000);
  https.setTimeout(15000);
  int httpCode = https.GET();
  bool ok = (httpCode > 0 && (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND));
  if (ok) {
    String payload = https.getString();
    payload.trim();
    syslog("HTTPS test OK, HTTP " + String(httpCode) + ", body: " + payload, 1);
    if (httpDebug) Serial.println("[HTTPS] OK body: " + payload);
    secureClientError = 0;
  } else if (httpCode > 0) {
    syslog("HTTPS test unexpected code " + String(httpCode), 2);
  } else {
    syslog("HTTPS test failed: error " + String(httpCode) + " " + String(https.errorToString(httpCode)), 2);
    if (httpDebug) Serial.println("[HTTPS] error " + String(httpCode));
    secureClientError++;
  }
  https.end();
  return ok;
}

/* Test HTTPS connection by fetching the version file from GitHub. Uses cert bundle first;
 * if verification fails (bundle missing root for raw.githubusercontent.com), retries with
 * hardcoded GitHub CA so the device still works and can OTA to a firmware with updated bundle. */
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
  bool ok = doVersionGet(url);
  if (!ok) {
    syslog("HTTPS test: retrying with GitHub CA (bundle may not verify this host)", 1);
    useGitHubCAOnly();
    ok = doVersionGet(url);
    useCertBundle();
    if (ok) syslog("HTTPS test OK with GitHub CA fallback", 1);
  }
  return ok;
}
