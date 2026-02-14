/*  If the TLS certificate bundle is missing, the dongle can restore it from the remote code
 *  repository by using a static root certificate to connect to this repository as a fall-back.
 *  As this usually happens when for some reason the SPIFFS file storage partition has become corrupt,
 *  the dongle will also redownload the other static files and then reboot.
 *  We use github as the code repo, but you can change it your own. 
 * */

/*The public root certificate for the repository*/ 
const char* github_root_ca= R"literal(
-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
)literal";


#define TLSBUNDLE "/x509_crt_bundle.bin"

void restoreSPIFFS(){
  listDir(SPIFFS, "/", 3);
  /*Load the static cert into the https client*/
  if(client){
    syslog("Setting up fallback TLS/SSL client", 2);
    client->setCACert(github_root_ca);
  }
  else{
    syslog("Failed to setup fallback TLS/SSL client", 3);
  }
  boolean repoOK = true;
  syslog("Checking remote repository", 0);
  String baseUrl = "https://github.com/plan-d-io/P1-dongle/raw/";
  if(_dev_fleet) baseUrl += "develop/data/x509_crt_bundle.bin";
  else if(_alpha_fleet) baseUrl += "alpha/data/cert/x509_crt_bundle.bin";
  else if(_v2_fleet) baseUrl += "V2-0/data/x509_crt_bundle.bin";
  else baseUrl += "main/data/cert/x509_crt_bundle.bin";
  String fileUrl = "https://raw.githubusercontent.com/plan-d-io/P1-dongle/main/data/x509_crt_bundle.bin";//"https://github.com/plan-d-io/P1-dongle/raw/develop/data/x509_crt_bundle.bin";
  String s = "/x509_crt_bundle.bin";
  if(repoOK){
    /*Reformat the SPIFFS*/
    syslog("Formatting", 0);
    bool formatted = SPIFFS.format();
    if(formatted){
      syslog("Success formatting", 0);
    }
    else{
      syslog("Error formatting", 3);
    }
    File f;
    if(SPIFFS.exists(TLSBUNDLE)){
      Serial.println("Removing old bundle");
      SPIFFS.remove(TLSBUNDLE);
    }
    /*Next, store a file to SPIFFS*/
    syslog("Downloading cert bundle", 0);
    Serial.println(fileUrl);
    f = LittleFS.open(TLSBUNDLE, "w");
    if(f){
      if (https.begin(*client, fileUrl)) {
        int httpCode = https.GET();
        Serial.println(httpCode);
        if (httpCode > 0) {
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
            long contentLength = https.getSize();
            Serial.print("File size: ");
            Serial.println(contentLength);
            Serial.println("Begin download");
            size_t written = https.writeToStream(&f);
            if (written == contentLength) {
              Serial.println("Written : " + String(written) + " successfully");
            }
          }
          else{
            syslog("Could not fetch file, HTTPS code " + String(httpCode), 2);
          }
        } 
        else {
          syslog("Could not connect to repository, HTTPS code " + String(httpCode) +" " +  String(https.errorToString(httpCode)), 2);
        }
        https.end();
      }
      f.close();
    }
    else{
      syslog("Could not open cert bundle file for writing", 2);
    }
    /*Check if the cert bundle is present*/
    File file = LittleFS.open(TLSBUNDLE, "r");
    if(file && file.size() > 0){
      syslog("Cert bundle present on SPIFFS", 1);
    }
    if(!file) {
        syslog("Could not load cert bundle from SPIFFS", 3);
    }
    file.close();
    bundleLoaded = true;
    /*Download the other static files*/
    _reinit_spiffs = false;
    saveResetReason("Rebooting after SPIFFS restore");
    saveConfig();
    SPIFFS.end();
    delay(500);
    ESP.restart();
  }
  else{
    //probably should do something here
  }
}
