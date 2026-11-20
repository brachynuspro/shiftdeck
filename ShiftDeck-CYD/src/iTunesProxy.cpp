/*-------------------------------------------------------------------------------------------------
** iTunesProxy.cpp - iTunes Search API proxy implementation
** ------------------------------------------------------------------------------------------------
*/
#include "iTunesProxy.h"
#include "CertBundle.h"
#include "logTags.h"
#include "SCLogger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#define TAG "iTunes"

static String itunesUrlEncode(const String& str) {
    String encoded;
    encoded.reserve(str.length() * 3);
    for (size_t i = 0; i < str.length(); i++) {
        char c = str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    return encoded;
}

static String buildSearchQuery(const String& artist, const String& album) {
    String a = artist; a.trim();
    String b = album; b.trim();
    if (a.isEmpty() && b.isEmpty()) return "";
    if (a.isEmpty()) return itunesUrlEncode(b);
    if (b.isEmpty()) return itunesUrlEncode(a);
    return itunesUrlEncode(a + " " + b);
}

String iTunesProxy::fetchArtworkUrl(const String& artist, const String& album) {
    String query = buildSearchQuery(artist, album);
    if (query.isEmpty()) {
        spLogW(TAG, "fetchArtworkUrl: empty query, skipping");
        return "";
    }

    String url = String("https://itunes.apple.com/search?term=") + query +
                 "&media=music&entity=album&limit=1";

    spLogI(TAG, "Searching iTunes: %s + %s", artist.c_str(), album.c_str());

    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start);
    client.setTimeout(5);

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("User-Agent", "ShiftDeck/1.0");
    http.setTimeout(4000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        spLogW(TAG, "iTunes search HTTP %d", code);
        http.end();
        return "";
    }

    String body = http.getString();
    http.end();

    if (body.isEmpty()) {
        spLogW(TAG, "iTunes search: empty body");
        return "";
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        spLogW(TAG, "iTunes JSON parse failed: %s", err.c_str());
        return "";
    }

    JsonArray results = doc["results"].as<JsonArray>();
    if (results.isNull() || results.size() == 0) {
        spLogW(TAG, "iTunes: no results for '%s + %s'", artist.c_str(), album.c_str());
        return "";
    }

    JsonObject first = results[0].as<JsonObject>();
    String artUrl = first["artworkUrl100"] | first["artworkUrl60"] | "";
    if (artUrl.isEmpty()) {
        spLogW(TAG, "iTunes: result has no artworkUrl");
        return "";
    }

    // Downscale to 120x120 for the 120px artwork card on the CYD.
    // Decoding a 600x600 JPEG in a 200KB heap without PSRAM can hang
    // the decoder (Huffman / quant tables balloon with the source
    // resolution). 120x120 keeps the heap budget under control and
    // gives the panel a 1:1 source-to-pixel mapping at native card
    // resolution.
    int slash = artUrl.lastIndexOf('/');
    if (slash > 0) {
        int dot = artUrl.lastIndexOf('.');
        if (dot > slash) {
            artUrl = artUrl.substring(0, slash + 1) + "120x120bb" + artUrl.substring(dot);
        }
    }

    spLogI(TAG, "iTunes artwork URL: %s", artUrl.c_str());
    return artUrl;
}

bool iTunesProxy::downloadJpeg(const String& url, const String& filename) {
    if (url.isEmpty()) return false;

    // Use a fixed filename so the view layer doesn't need to track
    // the iTunes URL (which differs from the Spotify URL stored in
    // album metadata). Overwriting on every new track is fine and
    // avoids the complexity of evicting LRU entries on a
    // constrained device.
    String outFile = "/art_current.jpg";
    if (!filename.isEmpty()) outFile = filename;

    spLogI(TAG, "Downloading: %s -> %s", url.c_str(), outFile.c_str());

    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start);
    client.setTimeout(5);

    HTTPClient http;
    http.begin(client, url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(4000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        spLogW(TAG, "iTunes JPEG download HTTP %d for %s", code, url.c_str());
        http.end();
        return false;
    }

    FsLock fs;
    if (!fs.ok) {
        http.end();
        return false;
    }
    File f = LittleFS.open(outFile, "w");
    if (!f) {
        spLogW(TAG, "iTunes JPEG: cannot open %s for writing", outFile.c_str());
        http.end();
        return false;
    }

    size_t written = http.writeToStream(&f);
    f.close();
    http.end();

    if (written == 0) {
        spLogW(TAG, "iTunes JPEG: empty, removing %s", outFile.c_str());
        LittleFS.remove(outFile);
        return false;
    }

    spLogI(TAG, "iTunes JPEG downloaded: %s (%u bytes)", outFile.c_str(), (unsigned)written);
    return true;
}
