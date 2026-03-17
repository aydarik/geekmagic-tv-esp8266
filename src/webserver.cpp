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
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "generated/index_html.h"
#include "generated/ota_html.h"

ESP8266WebServer server(WEB_SERVER_PORT);

extern Settings appSettings;
extern NotificationState notificationState;
extern CountdownState countdownState;
extern ClockState clockState;

// File upload buffer
File uploadFile;

void handleAppJson() {
    JsonDocument doc;
    doc["theme"] = displayState.theme;
    doc["img"] = displayState.image;
    doc["tz"] = appSettings.tz;
    doc["showIP"] = appSettings.showIP;
    doc["showSec"] = appSettings.showSec;
    doc["showWeather"] = appSettings.showWeather;
    doc["owmLoc"] = appSettings.owmLocation;
    doc["owmKey"] = appSettings.owmApiKey;
    if (displayState.timeout != 0) {
        doc["timeout"] = displayState.timeout;
    }

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleSpaceJson() {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    JsonDocument doc;
    doc["total"] = fs_info.totalBytes;
    doc["free"] = fs_info.totalBytes - fs_info.usedBytes;
    // 4 blocks × 4096 bytes -> LittleFS overhead

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleMemoryJson() {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    JsonDocument doc;
    doc["heap"] = ESP.getFreeHeap();
    doc["fragm"] = ESP.getHeapFragmentation();

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleBrtJson() {
    JsonDocument doc;
    doc["brt"] = appSettings.brightness;

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleVersionJson() {
    JsonDocument doc;
    doc["m"] = FIRMWARE_MODEL;
    doc["v"] = FIRMWARE_VERSION_STRING;

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleMessageJson() {
    JsonDocument doc;
    doc["msg"] = notificationState.message;
    doc["sbj"] = notificationState.subject;
    doc["style"] = notificationState.style;

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

void handleNoteJson() {
    JsonDocument doc;
    doc["note"] = clockState.note;
    if (clockState.noteRotations > 0)
        doc["rpm"] = clockState.noteRotations;

    server.setContentLength(measureJson(doc));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(doc, server.client());
}

inline int hexToInt(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

void urlDecode(const char *input, char *output, const size_t output_size) {
    const size_t max_size = output_size - 1;
    size_t written = 0;
    while (*input && written < max_size) {
        if (*input == '+') {
            output[written++] = ' ';
            input++;
        } else if (*input == '%' && isxdigit(*(input + 1)) && isxdigit(*(input + 2))) {
            output[written++] = hexToInt(*(input + 1)) << 4 | hexToInt(*(input + 2));
            input += 3;
        } else {
            output[written++] = *input++;
        }
    }
    output[written] = '\0';
}

void handleSet() {
    if (server.hasArg("msg")) {
        urlDecode(server.arg("msg").c_str(), notificationState.message, NOTIFICATION_MSG_BUFFER_SIZE);
        urlDecode(server.arg("sbj").c_str(), notificationState.subject, NOTIFICATION_SBJ_BUFFER_SIZE);
        urlDecode(server.arg("style").c_str(), notificationState.style, NOTIFICATION_STYLE_BUFFER_SIZE);
        displayUpdate(2);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0)
            displayState.timeout = time(nullptr) + timeout;
    } else if (server.hasArg("note")) {
        const bool hadNote = clockState.note[0] != '\0';
        urlDecode(server.arg("note").c_str(), clockState.note, CLOCK_NOTE_SIZE);
        const boolean hasNote = clockState.note[0] != '\0';
        clockState.noteRotations = server.arg("rpm").toInt();
        if (clockState.noteRotations > 60) clockState.noteRotations = 60; // Not more than every second
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0)
            clockState.noteTimeout = time(nullptr) + timeout;
        else clockState.noteTimeout = 0;
        const String force = server.arg("force");
        if ((displayState.theme == 1 && hadNote != hasNote)
            || force.equalsIgnoreCase("true")
            || force.equals("1")
        )
            displayUpdate();
    } else if (server.hasArg("cnt")) {
        urlDecode(server.arg("sbj").c_str(), countdownState.subject, COUNTDOWN_SBJ_BUFFER_SIZE);
        urlDecode(server.arg("cnt").c_str(), countdownState.datetime, COUNTDOWN_DATETIME_BUFFER_SIZE);
        displayUpdate(4);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0)
            if (const time_t datetime = parseDateTime(countdownState.datetime); datetime > time(nullptr))
                displayState.timeout = datetime + timeout;
    } else if (server.hasArg("brt")) {
        appSettings.brightness = server.arg("brt").toInt();
        displaySetBrightness(appSettings.brightness);
        settingsSave(appSettings);
    } else if (server.hasArg("theme")) {
        displayUpdate(server.arg("theme").toInt());
    } else if (server.hasArg("img")) {
        urlDecode(server.arg("img").c_str(), displayState.image, DISPLAY_IMG_PATH_BUFFER_SIZE);
        displayUpdate(3);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0)
            displayState.timeout = time(nullptr) + timeout;
    } else if (server.hasArg("ip")) {
        appSettings.showIP = server.arg("ip") != "false";
        if (displayState.theme == 1) displayUpdate();
        settingsSave(appSettings);
    } else if (server.hasArg("sec")) {
        appSettings.showSec = server.arg("sec") != "false";
        if (displayState.theme == 1) displayUpdate();
        settingsSave(appSettings);
    } else if (server.hasArg("weather")) {
        appSettings.showWeather = server.arg("weather") != "false";
        if (displayState.theme == 1) displayUpdate();
        settingsSave(appSettings);
    } else if (server.hasArg("tz")) {
        strncpy(appSettings.tz, server.arg("tz").c_str(), sizeof(appSettings.tz));
        appSettings.tz[sizeof(appSettings.tz) - 1] = '\0'; // Ensure null-termination
        setenv("TZ", appSettings.tz, 1);
        tzset();
        if (displayState.theme == 1) displayUpdate();
        settingsSave(appSettings);
    } else if (server.hasArg("owmLoc") && server.hasArg("owmKey")) {
        strncpy(appSettings.owmLocation, server.arg("owmLoc").c_str(), sizeof(appSettings.owmLocation));
        appSettings.owmLocation[sizeof(appSettings.owmLocation) - 1] = '\0';
        strncpy(appSettings.owmApiKey, server.arg("owmKey").c_str(), sizeof(appSettings.owmApiKey));
        appSettings.owmApiKey[sizeof(appSettings.owmApiKey) - 1] = '\0';
        if (displayState.theme == 1) displayUpdate();
        settingsSave(appSettings);
    } else {
        server.send(400, CONTENT_TYPE_TEXT, F("No action"));
        return;
    }
    server.send(200, CONTENT_TYPE_TEXT, F("OK"));
}

void handleTest() {
    displayTest();
    server.send(200, CONTENT_TYPE_TEXT, F("OK"));
}

void handleFileUpload() {
    const String dir = server.hasArg("dir") ? server.arg("dir") : "/";
    if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);

    const HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        const String filename = upload.filename;
        const String filepath = dir + filename;
        uploadFile = LittleFS.open(filepath, "w");
        if (!uploadFile)
            logPrint("Failed to open file for writing!");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            if (const size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                bytesWritten != upload.currentSize) {
                logPrintf("Only %u of %u bytes written!", bytesWritten, upload.currentSize);
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            logPrintf("File uploaded: %u bytes", upload.totalSize);
        }
    }
}

void handleUploadDone() {
    server.send(200, CONTENT_TYPE_TEXT, F("OK"));
}

void handleDelete() {
    if (server.hasArg("file")) {
        char imagePath[DISPLAY_IMG_PATH_BUFFER_SIZE];
        urlDecode(server.arg("file").c_str(), imagePath, DISPLAY_IMG_PATH_BUFFER_SIZE);
        if (LittleFS.remove(imagePath)) {
            server.send(200, CONTENT_TYPE_TEXT, F("Deleted"));
            logPrintf("File deleted", imagePath);
        } else server.send(404, CONTENT_TYPE_TEXT, F("Not found"));
    } else server.send(400, CONTENT_TYPE_TEXT, F("Missing file parameter"));
}

void streamDirRecursiveHtml(const char *dirname) {
    File root = LittleFS.open(dirname, "r");
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            const size_t len = strlen(file.fullName()) + 2;
            char childPath[len];
            snprintf(childPath, len, "/%s", file.fullName());
            streamDirRecursiveHtml(childPath);
        } else {
            const char *fileName = file.name();
            const size_t fileSize = file.size();

            auto fnameLower = String(fileName);
            fnameLower.toLowerCase();

            server.sendContent(F("<tr><td>"));
            if (strcmp(displayState.image, file.fullName()) == 0)
                server.sendContent(F("&#x2714; "));
            server.sendContent(F("<a href='"));
            server.sendContent(dirname);
            server.sendContent(F("/"));
            server.sendContent(fileName);
            server.sendContent(F("'>"));
            server.sendContent(fileName);
            server.sendContent(F("</a></td><td class='size'>"));
            server.sendContent(String(fileSize));
            server.sendContent(F("</td><td><div class='button-group'>"));

            // Delete button
            server.sendContent(F("<button class='button' onclick=\"deleteImage('"));
            server.sendContent(dirname);
            server.sendContent(F("/"));
            server.sendContent(fileName);
            server.sendContent(F("')\">DEL</button>"));

            // Set button for JPGs
            if (fnameLower.endsWith(F(".jpg"))) {
                server.sendContent(F("<button class='button' onclick=\"displayImage('"));
                server.sendContent(dirname);
                server.sendContent(F("/"));
                server.sendContent(fileName);
                server.sendContent(F("')\">SET</button>"));
            }

            server.sendContent(F("</div></td></tr>\n"));
        }
        file = root.openNextFile();
    }
}

void handleFileList() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, CONTENT_TYPE_HTML, F(""));

    server.sendContent(F("<table><thead><tr><th>Path</th><th>Size</th><th>Actions</th></tr></thead><tbody>\n"));
    streamDirRecursiveHtml(server.hasArg("dir") ? server.arg("dir").c_str() : "/");
    server.sendContent(F("</tbody></table>\n"));
    server.sendContent(F("")); // End of chunked response
}

// Function to handle factory reset
void handleFactoryReset() {
    server.send(200, CONTENT_TYPE_TEXT, F("Factory Reset triggered. Clearing data and restarting..."));
    delay(100); // Give time for response to send
    factoryReset();
}

void handleOTAForm() {
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.sendHeader(F("Cache-Control"), F("max-age=600"));
    server.send_P(200, CONTENT_TYPE_HTML, reinterpret_cast<const char *>(src_generated_ota_html_gz),
                  src_generated_index_html_gz_len);
}

void handleOTAUpload() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        showMessage(F("OTA Update..."));
        const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace))
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            Update.printError(Serial);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.end(true)) {
            Update.printError(Serial);
            showMessage(F("OTA Failed!"));
        }
    }
}

void handleOTADone() {
    const bool shouldReboot = !Update.hasError();
    server.send(200, CONTENT_TYPE_TEXT, shouldReboot ? F("OK - Rebooting...") : F("FAIL"));
    if (shouldReboot) {
        showMessage(F("Success!\nRebooting..."));
        delay(2000);
        ESP.restart();
    }
}

void handleLog() {
    const String log = logGetAll();
    server.send(200, CONTENT_TYPE_TEXT, log);
}

void handleWiFiScan() {
    const int numNetworks = WiFi.scanNetworks(false, true);

    JsonDocument docRoot;
    for (int i = 0; i < numNetworks; i++) {
        JsonDocument doc;
        doc["ssid"] = WiFi.SSID(i);
        doc["rssi"] = WiFi.RSSI(i);
        docRoot.add(doc);
    }

    WiFi.scanDelete(); // Clear scan results

    server.setContentLength(measureJson(docRoot));
    server.send(200, CONTENT_TYPE_JSON, F(""));
    serializeJson(docRoot, server.client());
}

void handleWiFiConnect() {
    if (!server.hasArg("ssid")) {
        server.send(400, CONTENT_TYPE_TEXT, F("Missing SSID"));
        return;
    }

    const String ssid = server.arg("ssid");
    const String password = server.hasArg("password") ? server.arg("password") : "";

    server.send(200, CONTENT_TYPE_TEXT, F("Connecting..."));
    delay(100);

    // Enable persistent WiFi credentials storage
    WiFi.persistent(true);
    WiFi.setAutoReconnect(true);

    // Disconnect from AP mode and switch to STA mode
    WiFi.softAPdisconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    delay(100);

    // Connect to new WiFi with credentials
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait up to 20 seconds for connection
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

void handleStatic() {
    String path = server.uri();

    // Check if file exists in LittleFS
    if (!LittleFS.exists(path)) {
        server.send(404, CONTENT_TYPE_TEXT, F("File not found"));
        return;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        server.send(500, CONTENT_TYPE_TEXT, F("Failed to open file"));
        return;
    }

    server.streamFile(file, path.endsWith(F(".jpg")) ? F("image/jpeg") : F("application/octet-stream"));
    file.close();
}

void handleRoot() {
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.sendHeader(F("Cache-Control"), F("max-age=600"));
    server.send_P(200, CONTENT_TYPE_HTML, reinterpret_cast<const char *>(src_generated_index_html_gz),
                  src_generated_index_html_gz_len);
}

void webserverInit() {
    // GET endpoints
    server.on(F("/"), HTTP_GET, handleRoot);
    server.on(F("/app.json"), HTTP_GET, handleAppJson);
    server.on(F("/space.json"), HTTP_GET, handleSpaceJson);
    server.on(F("/memory.json"), HTTP_GET, handleMemoryJson);
    server.on(F("/brt.json"), HTTP_GET, handleBrtJson);
    server.on(F("/v.json"), HTTP_GET, handleVersionJson);
    server.on(F("/message.json"), HTTP_GET, handleMessageJson);
    server.on(F("/note.json"), HTTP_GET, handleNoteJson);

    server.on(F("/filelist"), HTTP_GET, handleFileList);
    server.on(F("/delete"), HTTP_GET, handleDelete);
    server.on(F("/set"), HTTP_GET, handleSet);

    server.on(F("/test"), HTTP_GET, handleTest);
    server.on(F("/log"), HTTP_GET, handleLog);
    server.on(F("/factoryreset"), HTTP_GET, handleFactoryReset);
    server.on(F("/scan"), HTTP_GET, handleWiFiScan);
    server.on(F("/connect"), HTTP_GET, handleWiFiConnect);

    // File upload
    server.on(F("/doUpload"), HTTP_POST, handleUploadDone, handleFileUpload);

    // OTA
    server.on(F("/update"), HTTP_GET, handleOTAForm);
    server.on(F("/update"), HTTP_POST, handleOTADone, handleOTAUpload);

    // Serve images from LittleFS (catches all unhandled routes)
    server.onNotFound(handleStatic);

    server.begin();
    Serial.println(F("Web server started"));
}

void webserverHandle() {
    server.handleClient();
}
