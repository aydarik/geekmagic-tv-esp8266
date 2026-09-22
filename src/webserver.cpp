#include "webserver.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "themes/notification.h"
#include "themes/countdown.h"
#include "themes/clock.h"
#include "main.h"
#include "logger.h"
#include "utils.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>

#include "generated/index_html.h"
#include "generated/ota_html.h"

static AsyncWebServer server(WEB_SERVER_PORT);
static AsyncWebSocket ws("/ws");

extern Settings  appSettings;
extern Secrets   appSecrets;
extern NotificationState notificationState;
extern CountdownState    countdownState;
extern ClockState        clockState;

// ---------------------------------------------------------------------------
// WebSocket — broadcast JSON events to all connected clients
// ---------------------------------------------------------------------------

void wsBroadcast(const JsonDocument &doc) {
    if (ws.count() == 0) return;
    char buf[128];
    const size_t len = serializeJson(doc, buf, sizeof(buf));
    ws.textAll(buf, len);
}

void wsBroadcastState() {
    if (ws.count() == 0) return;
    JsonDocument doc;
    if (displayState.theme != Theme::NONE)
        doc["theme"] = static_cast<int8_t>(displayState.theme);

    doc["brt"] = appSettings.brightness;

    doc["mem_heap"] = ESP.getFreeHeap();

    FSInfo fs_info;
    LittleFS.info(fs_info);
    doc["space_free"] = fs_info.totalBytes - fs_info.usedBytes;

    // doc["msg_text"]  = notificationState.message;
    // doc["msg_sbj"]   = notificationState.subject;
    // doc["msg_style"] = notificationState.style;
    //
    // doc["note_text"] = clockState.note;
    // if (clockState.noteRotations > 0)
    //     doc["note_rpm"] = clockState.noteRotations;

    wsBroadcast(doc);
}

void webserverHandle() {
    ws.cleanupClients();
}

// ---------------------------------------------------------------------------
// JSON response helpers
// ---------------------------------------------------------------------------

static void sendJson(AsyncWebServerRequest *request, const JsonDocument &doc) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

// ---------------------------------------------------------------------------
// GET /app.json
// ---------------------------------------------------------------------------

static void handleAppJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["ip"]          = WiFi.localIP().toString();
    doc["brt"]         = appSettings.brightness;
    doc["theme"]       = static_cast<int8_t>(displayState.theme);
    doc["defaultTheme"]= static_cast<int8_t>(appSettings.defaultTheme);
    doc["img"]         = displayState.image;
    doc["tz"]          = appSettings.tz;
    doc["showIP"]      = appSettings.showIP;
    doc["showSec"]     = appSettings.showSec;
    doc["showWeather"] = appSettings.showWeather;
    doc["owmLoc"]      = appSecrets.owmLocation;
    // Note: owmKey intentionally omitted from this endpoint
    if (displayState.timeout != 0)
        doc["timeout"] = displayState.timeout;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /space.json
// ---------------------------------------------------------------------------

static void handleSpaceJson(AsyncWebServerRequest *request) {
    FSInfo fs_info;
    LittleFS.info(fs_info);
    JsonDocument doc;
    doc["total"] = fs_info.totalBytes;
    doc["free"]  = fs_info.totalBytes - fs_info.usedBytes;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /memory.json
// ---------------------------------------------------------------------------

static void handleMemoryJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["heap"]  = ESP.getFreeHeap();
    doc["fragm"] = ESP.getHeapFragmentation();
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /brt.json
// ---------------------------------------------------------------------------

static void handleBrtJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["brt"] = appSettings.brightness;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /v.json
// ---------------------------------------------------------------------------

static void handleVersionJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["m"] = FIRMWARE_MODEL;
    doc["v"] = FIRMWARE_VERSION_STRING;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /message.json
// ---------------------------------------------------------------------------

static void handleMessageJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["msg"]   = notificationState.message;
    doc["sbj"]   = notificationState.subject;
    doc["style"] = notificationState.style;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /note.json
// ---------------------------------------------------------------------------

static void handleNoteJson(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["note"] = clockState.note;
    if (clockState.noteRotations > 0)
        doc["rpm"] = clockState.noteRotations;
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /set — device control endpoint
// All args are URL-decoded by ESPAsyncWebServer automatically.
// ---------------------------------------------------------------------------

static void handleSet(AsyncWebServerRequest *request) {
    if (request->hasParam("msg")) {
        const AsyncWebParameter *pMsg = request->getParam("msg");
        strncpy(notificationState.message, pMsg->value().c_str(), NOTIFICATION_MSG_BUFFER_SIZE - 1);
        notificationState.message[NOTIFICATION_MSG_BUFFER_SIZE - 1] = '\0';

        const AsyncWebParameter *pSbj = request->getParam("sbj");
        if (pSbj) {
            strncpy(notificationState.subject, pSbj->value().c_str(), NOTIFICATION_SBJ_BUFFER_SIZE - 1);
            notificationState.subject[NOTIFICATION_SBJ_BUFFER_SIZE - 1] = '\0';
        } else {
            notificationState.subject[0] = '\0';
        }

        const AsyncWebParameter *pStyle = request->getParam("style");
        if (pStyle) {
            strncpy(notificationState.style, pStyle->value().c_str(), NOTIFICATION_STYLE_BUFFER_SIZE - 1);
            notificationState.style[NOTIFICATION_STYLE_BUFFER_SIZE - 1] = '\0';
        } else {
            notificationState.style[0] = '\0';
        }

        time_t timeoutAt = 0;
        const AsyncWebParameter *pTimeout = request->getParam("timeout");
        if (pTimeout) {
            const int timeout = pTimeout->value().toInt();
            if (timeout > 0) timeoutAt = time(nullptr) + timeout;
        }
        displayScheduleUpdate(Theme::NOTIFICATION, true, timeoutAt);

    } else if (request->hasParam("note")) {
        const bool hadNote = clockState.note[0] != '\0';
        const AsyncWebParameter *pNote = request->getParam("note");
        strncpy(clockState.note, pNote->value().c_str(), CLOCK_NOTE_SIZE - 1);
        clockState.note[CLOCK_NOTE_SIZE - 1] = '\0';
        const bool hasNote = clockState.note[0] != '\0';

        const AsyncWebParameter *pRpm = request->getParam("rpm");
        clockState.noteRotations = pRpm ? pRpm->value().toInt() : 0;
        if (clockState.noteRotations > 60) clockState.noteRotations = 60;

        const AsyncWebParameter *pTimeout = request->getParam("timeout");
        if (pTimeout) {
            const int timeout = pTimeout->value().toInt();
            clockState.noteTimeout = timeout > 0 ? time(nullptr) + timeout : 0;
        } else {
            clockState.noteTimeout = 0;
        }

        const AsyncWebParameter *pForce = request->getParam("force");
        const String force = pForce ? pForce->value() : "";
        if (displayState.theme == Theme::CLOCK
            && (hadNote != hasNote
                || force.equalsIgnoreCase("true")
                || force == "1")) {
            displayScheduleUpdate(Theme::NONE, true);
        }

    } else if (request->hasParam("cnt")) {
        const AsyncWebParameter *pSbj = request->getParam("sbj");
        if (pSbj) {
            strncpy(countdownState.subject, pSbj->value().c_str(), COUNTDOWN_SBJ_BUFFER_SIZE - 1);
            countdownState.subject[COUNTDOWN_SBJ_BUFFER_SIZE - 1] = '\0';
        } else {
            countdownState.subject[0] = '\0';
        }

        const AsyncWebParameter *pCnt = request->getParam("cnt");
        strncpy(countdownState.datetime, pCnt->value().c_str(), COUNTDOWN_DATETIME_BUFFER_SIZE - 1);
        countdownState.datetime[COUNTDOWN_DATETIME_BUFFER_SIZE - 1] = '\0';

        time_t timeoutAt = 0;
        const AsyncWebParameter *pTimeout = request->getParam("timeout");
        if (pTimeout) {
            const int timeout = pTimeout->value().toInt();
            if (timeout > 0) {
                const time_t dt = parseDateTime(countdownState.datetime);
                if (dt > time(nullptr)) timeoutAt = dt + timeout;
            }
        }
        displayScheduleUpdate(Theme::COUNTDOWN, true, timeoutAt);

    } else if (request->hasParam("brt")) {
        appSettings.brightness = request->getParam("brt")->value().toInt();
        displaySetBrightness(appSettings.brightness);
        settingsSave(appSettings);

    } else if (request->hasParam("theme")) {
        const auto theme = static_cast<Theme>(request->getParam("theme")->value().toInt());
        const bool setDefault = request->hasParam("default") && request->getParam("default")->value() != "false";
        if (setDefault) {
            appSettings.defaultTheme = theme;
            settingsSave(appSettings);
        }
        displayScheduleUpdate(theme, true);

    } else if (request->hasParam("img")) {
        const AsyncWebParameter *pImg = request->getParam("img");
        strncpy(displayState.image, pImg->value().c_str(), DISPLAY_IMG_PATH_BUFFER_SIZE - 1);
        displayState.image[DISPLAY_IMG_PATH_BUFFER_SIZE - 1] = '\0';

        time_t timeoutAt = 0;
        const AsyncWebParameter *pTimeout = request->getParam("timeout");
        if (pTimeout) {
            const int timeout = pTimeout->value().toInt();
            if (timeout > 0) timeoutAt = time(nullptr) + timeout;
        }
        displayScheduleUpdate(Theme::IMAGE, true, timeoutAt);

    } else if (request->hasParam("ip")) {
        appSettings.showIP = request->getParam("ip")->value() != "false";
        if (displayState.theme == Theme::CLOCK) displayScheduleUpdate(Theme::NONE, true);
        settingsSave(appSettings);

    } else if (request->hasParam("sec")) {
        appSettings.showSec = request->getParam("sec")->value() != "false";
        if (displayState.theme == Theme::CLOCK || displayState.theme == Theme::BIG_CLOCK) displayScheduleUpdate(Theme::NONE, true);
        settingsSave(appSettings);

    } else if (request->hasParam("weather")) {
        appSettings.showWeather = request->getParam("weather")->value() != "false";
        if (displayState.theme == Theme::CLOCK) displayScheduleUpdate(Theme::NONE, true);
        settingsSave(appSettings);

    } else if (request->hasParam("tz")) {
        const AsyncWebParameter *pTz = request->getParam("tz");
        strncpy(appSettings.tz, pTz->value().c_str(), sizeof(appSettings.tz) - 1);
        appSettings.tz[sizeof(appSettings.tz) - 1] = '\0';
        setenv("TZ", appSettings.tz, 1);
        tzset();
        if (displayState.theme == Theme::CLOCK) displayScheduleUpdate(Theme::NONE, true);
        settingsSave(appSettings);

    } else if (request->hasParam("owmLoc") && request->hasParam("owmKey")) {
        const AsyncWebParameter *pLoc = request->getParam("owmLoc");
        const AsyncWebParameter *pKey = request->getParam("owmKey");
        strncpy(appSecrets.owmLocation, pLoc->value().c_str(), sizeof(appSecrets.owmLocation) - 1);
        appSecrets.owmLocation[sizeof(appSecrets.owmLocation) - 1] = '\0';
        strncpy(appSecrets.owmApiKey,   pKey->value().c_str(), sizeof(appSecrets.owmApiKey) - 1);
        appSecrets.owmApiKey[sizeof(appSecrets.owmApiKey) - 1] = '\0';
        if (displayState.theme == Theme::CLOCK) displayScheduleUpdate(Theme::NONE, true);
        secretsSave(appSecrets);

    } else {
        request->send(400, "text/plain", "No action");
        return;
    }

    request->send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------
// GET /test
// ---------------------------------------------------------------------------

static void handleTest(AsyncWebServerRequest *request) {
    displayScheduleTest();
    request->send(200, "text/plain", "OK");
}

// ---------------------------------------------------------------------------
// GET /delete
// ---------------------------------------------------------------------------

static void handleDelete(AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "Missing file parameter");
        return;
    }
    const String path = request->getParam("file")->value();
    if (LittleFS.remove(path)) {
        logPrintf("File deleted: %s", path.c_str());
        request->send(200, "text/plain", "Deleted");
    } else {
        request->send(404, "text/plain", "Not found");
    }
}

// ---------------------------------------------------------------------------
// GET /filelist — HTML table (compatible with legacy API)
// ---------------------------------------------------------------------------

static void streamDirRecursive(AsyncResponseStream *stream, const char *dirname) {
    File root = LittleFS.open(dirname, "r");
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            const size_t len = strlen(file.fullName()) + 2;
            char childPath[len];
            snprintf(childPath, len, "/%s", file.fullName());
            streamDirRecursive(stream, childPath);
        } else {
            const char *fileName = file.name();
            const size_t fileSize = file.size();
            auto fnameLower = String(fileName);
            fnameLower.toLowerCase();

            stream->print("<tr><td>");
            if (strcmp(displayState.image, file.fullName()) == 0)
                stream->print("&#x2714; ");
            stream->printf("<a href='%s/%s'>%s</a></td>", dirname, fileName, fileName);
            stream->printf("<td class='size'>%u</td>", fileSize);
            stream->print("<td><div class='button-group'>");
            stream->printf("<button class='button' onclick=\"deleteImage('%s/%s')\">DEL</button>", dirname, fileName);
            if (fnameLower.endsWith(".jpg"))
                stream->printf("<button class='button' onclick=\"displayImage('%s/%s')\">SET</button>", dirname, fileName);
            stream->print("</div></td></tr>\n");
        }
        file = root.openNextFile();
    }
}

static void handleFileList(AsyncWebServerRequest *request) {
    const char *dir = request->hasParam("dir") ? request->getParam("dir")->value().c_str() : "/";
    AsyncResponseStream *stream = request->beginResponseStream("text/html");
    stream->print("<table><thead><tr><th>Path</th><th>Size</th><th>Actions</th></tr></thead><tbody>\n");
    streamDirRecursive(stream, dir);
    stream->print("</tbody></table>\n");
    request->send(stream);
}

// ---------------------------------------------------------------------------
// GET /log
// ---------------------------------------------------------------------------

static void handleLog(AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    logPrintTo(*response);
    request->send(response);
}

// ---------------------------------------------------------------------------
// GET /factoryreset
// ---------------------------------------------------------------------------

static void handleFactoryReset(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Factory Reset triggered. Clearing data and restarting...");
    factoryReset();
}

// ---------------------------------------------------------------------------
// GET /scan — WiFi network scan
// ---------------------------------------------------------------------------

static void handleWiFiScan(AsyncWebServerRequest *request) {
    const int n = WiFi.scanNetworks(false, true);
    JsonDocument doc;
    for (int i = 0; i < n; i++) {
        JsonDocument entry;
        entry["ssid"] = WiFi.SSID(i);
        entry["rssi"] = WiFi.RSSI(i);
        doc.add(entry);
    }
    WiFi.scanDelete();
    sendJson(request, doc);
}

// ---------------------------------------------------------------------------
// GET /connect — join a new WiFi network
// ---------------------------------------------------------------------------

static void handleWiFiConnect(AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid")) {
        request->send(400, "text/plain", "Missing SSID");
        return;
    }
    const String ssid     = request->getParam("ssid")->value();
    const String password = request->hasParam("password") ? request->getParam("password")->value() : "";

    request->send(200, "text/plain", "Connecting...");

    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);
    WiFi.softAPdisconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        attempts++;
        delay(500);
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) showMessage(F("Success!\nRebooting..."));
    else showMessage(F("Failed :(\nRebooting..."));
    delay(2000);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// POST /doUpload — file upload handler
// ---------------------------------------------------------------------------

static File uploadFile;

static void handleFileUpload(AsyncWebServerRequest *request, const String &filename,
                              size_t index, uint8_t *data, size_t len, bool final) {
    const String dir = request->hasParam("dir") ? request->getParam("dir")->value() : "/";
    if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);

    if (index == 0) {
        const String filepath = dir + filename;
        uploadFile = LittleFS.open(filepath, "w");
        if (!uploadFile) logPrint("Failed to open file for writing!");
    }
    if (uploadFile && len > 0) {
        const size_t written = uploadFile.write(data, len);
        if (written != len) logPrintf("Only %u of %u bytes written!", written, len);
    }
    if (final && uploadFile) {
        uploadFile.close();
        logPrintf("File uploaded: %u bytes", index + len);
        request->send(200, "text/plain", "OK");
    }
}

// ---------------------------------------------------------------------------
// OTA update handlers
// ---------------------------------------------------------------------------

static void handleOTAUpload(AsyncWebServerRequest *request, const String &filename,
                              size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
        showMessage(F("OTA Update..."));
        const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) Update.printError(Serial);
    }
    if (len > 0 && Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (!Update.end(true)) {
            Update.printError(Serial);
            showMessage(F("OTA Failed!"));
        }
        const bool ok = !Update.hasError();
        request->send(200, "text/plain", ok ? F("OK - Rebooting...") : F("FAIL"));
        if (ok) {
            showMessage(F("Success!\nRebooting..."));
            delay(2000);
            ESP.restart();
        }
    }
}

// ---------------------------------------------------------------------------
// Serve static files from LittleFS (images etc.)
// ---------------------------------------------------------------------------

static void handleStatic(AsyncWebServerRequest *request) {
    const String path = request->url();
    if (!LittleFS.exists(path)) {
        request->send(404, "text/plain", "File not found");
        return;
    }
    const String contentType = path.endsWith(".jpg") ? "image/jpeg" : "application/octet-stream";
    request->send(LittleFS, path, contentType);
}

// ---------------------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------------------

static void onWsEvent(AsyncWebSocket *srv, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
    (void)srv; (void)arg; (void)data; (void)len;
    if (type == WS_EVT_CONNECT) {
        JsonDocument doc;
        doc["theme"] = static_cast<int8_t>(displayState.theme);
        doc["brt"]   = appSettings.brightness;
        char buf[128];
        const size_t len = serializeJson(doc, buf, sizeof(buf));
        client->text(buf, len);
    }
}

// ---------------------------------------------------------------------------
// webserverInit
// ---------------------------------------------------------------------------

void webserverInit() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Root page (gzip compressed)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *resp = request->beginResponse_P(
            200, "text/html",
            reinterpret_cast<const uint8_t *>(src_generated_index_html_gz),
            src_generated_index_html_gz_len);
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "max-age=600");
        request->send(resp);
    });

    // OTA page (gzip compressed)
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *resp = request->beginResponse_P(
            200, "text/html",
            reinterpret_cast<const uint8_t *>(src_generated_ota_html_gz),
            src_generated_ota_html_gz_len);
        resp->addHeader("Content-Encoding", "gzip");
        resp->addHeader("Cache-Control", "max-age=600");
        request->send(resp);
    });

    // JSON endpoints
    server.on("/app.json",     HTTP_GET, handleAppJson);
    server.on("/space.json",   HTTP_GET, handleSpaceJson);
    server.on("/memory.json",  HTTP_GET, handleMemoryJson);
    server.on("/brt.json",     HTTP_GET, handleBrtJson);
    server.on("/v.json",       HTTP_GET, handleVersionJson);
    server.on("/message.json", HTTP_GET, handleMessageJson);
    server.on("/note.json",    HTTP_GET, handleNoteJson);

    // Control endpoints
    server.on("/set",          HTTP_GET, handleSet);
    server.on("/filelist",     HTTP_GET, handleFileList);
    server.on("/delete",       HTTP_GET, handleDelete);
    server.on("/test",         HTTP_GET, handleTest);
    server.on("/log",          HTTP_GET, handleLog);
    server.on("/factoryreset", HTTP_GET, handleFactoryReset);
    server.on("/scan",         HTTP_GET, handleWiFiScan);
    server.on("/connect",      HTTP_GET, handleWiFiConnect);

    // File upload
    server.on("/doUpload", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* handled in upload callback */ },
        handleFileUpload);

    // OTA upload
    server.on("/update", HTTP_POST,
        [](AsyncWebServerRequest *request) { /* handled in upload callback */ },
        handleOTAUpload);

    // Fallback: serve files from LittleFS
    server.onNotFound(handleStatic);

    server.begin();
    Serial.println(F("Async web server started"));
}
