/*
 * syslog.ino - Log to Serial and to LittleFS /syslog.txt when mounted.
 * Rotate when file exceeds 5120 bytes (move to /syslog0.txt).
 * syslogMutex serializes file access with the /syslog HTTP handler to avoid crash on concurrent read/write.
 */
#include <freertos/semphr.h>
extern SemaphoreHandle_t syslogMutex;

void syslog(String msg, int level) {
  String logmsg;
  if (timeSet) {
    logmsg = printLocalTime(false) + " ";
  }
  if (level == 0 || level == 1) logmsg += "INFO: ";
  else if (level == 2) logmsg += "WARNING: ";
  else if (level == 3 || level == 4) logmsg += "ERROR: ";
  else logmsg += "MISC: ";
  logmsg += msg;
  Serial.println(logmsg);

  /* Append to file for level > 0 when LittleFS is mounted */
  if (level > 0 && spiffsMounted && syslogMutex != NULL) {
    xSemaphoreTake(syslogMutex, portMAX_DELAY);
    if (sizeFile(LittleFS, "/syslog.txt") > 5120) {
      deleteFile(LittleFS, "/syslog0.txt");
      renameFile(LittleFS, "/syslog.txt", "/syslog0.txt");
    }
    logmsg += "\r\n";
    appendFile(LittleFS, "/syslog.txt", logmsg.c_str());
    xSemaphoreGive(syslogMutex);
  }
}

void saveResetReason(String rReason) {
  if (timeSet) _last_reset = printLocalTime(false) + " ";
  else _last_reset = "";
  _last_reset += rReason;
}
