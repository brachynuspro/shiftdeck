/*-------------------------------------------------------------------------------------------------
** Updater.cpp - Over-the-air updates (see Updater.h)
** ------------------------------------------------------------------------------------------------
*/
#include "Updater.h"
#include "config.h"
#include "CertBundle.h"
#include "logTags.h"
#include "SCLogger.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

String Updater::latestVersion() {
    WiFiClientSecure c;
    c.setCACertBundle(x509_crt_bundle_start);
    c.setTimeout(8);
    HTTPClient http;
    if (!http.begin(c, UPDATE_BASE_URL "manifest-update.json")) return "";
    http.setTimeout(8000);
    String v;
    int st = http.GET();
    if (st == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) v = doc["version"] | "";
    }
    http.end();
    spLogI(LOGTAG_GENERAL, "Update check: HTTP %d, latest '%s', running %s", st, v.c_str(), VERSION);
    return v;
}

String Updater::install(void (*progress)(int, int)) {
    WiFiClientSecure c;
    c.setCACertBundle(x509_crt_bundle_start);
    c.setTimeout(15);
    httpUpdate.rebootOnUpdate(false);   // the caller shows "Reiniciando..." first
    httpUpdate.onProgress(progress);
    t_httpUpdate_return r = httpUpdate.update(c, UPDATE_BASE_URL "firmware/shiftdeck-app.bin");
    if (r == HTTP_UPDATE_OK) return "";
    String err = httpUpdate.getLastErrorString();
    spLogW(LOGTAG_GENERAL, "Update failed (%d): %s", httpUpdate.getLastError(), err.c_str());
    return err.length() ? err : String("Falha no download");
}
