#include "webserver.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "themes/notification.h"
#include "themes/countdown.h"
#include "themes/clock.h"
#include "main.h"
#include "logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

#include "generated/index_html.h"

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
    if (displayState.timeout != 0) {
        doc["timeout"] = displayState.timeout;
    }

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

void handleSpaceJson() {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    JsonDocument doc;
    doc["total"] = fs_info.totalBytes;
    doc["free"] = fs_info.totalBytes - fs_info.usedBytes;
    doc["heap"] = ESP.getFreeHeap();
    doc["fragm"] = ESP.getHeapFragmentation();

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

void handleBrtJson() {
    JsonDocument doc;
    doc["brt"] = appSettings.brightness;

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

void handleVersionJson() {
    JsonDocument doc;
    doc["m"] = "aydarik";
    doc["v"] = FIRMWARE_VERSION_STRING;

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

void handleMessageJson() {
    JsonDocument doc;
    doc["msg"] = notificationState.message;
    doc["sbj"] = notificationState.subject;
    doc["style"] = notificationState.style;

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

void handleNoteJson() {
    JsonDocument doc;
    doc["note"] = clockState.note;

    server.setContentLength(measureJson(doc));
    server.send(200, "application/json", "");
    serializeJson(doc, server.client());
}

String urlDecode(const String &input) {
    String decoded = "";
    char temp[] = "0x00";

    for (unsigned int i = 0; i < input.length(); i++) {
        if (input[i] == '+') {
            decoded += ' ';
        } else if (input[i] == '%' && i + 2 < input.length()) {
            temp[2] = input[i + 1];
            temp[3] = input[i + 2];
            decoded += static_cast<char>(strtol(temp, nullptr, 16));
            i += 2;
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

void handleSet() {
    if (server.hasArg("msg")) {
        strncpy(notificationState.message, urlDecode(server.arg("msg")).c_str(), sizeof(notificationState.message));
        notificationState.message[sizeof(notificationState.message) - 1] = '\0'; // Ensure null-termination
        strncpy(notificationState.subject, urlDecode(server.arg("sbj")).c_str(), sizeof(notificationState.subject));
        notificationState.subject[sizeof(notificationState.subject) - 1] = '\0'; // Ensure null-termination
        strncpy(notificationState.style, urlDecode(server.arg("style")).c_str(), sizeof(notificationState.style));
        notificationState.style[sizeof(notificationState.style) - 1] = '\0'; // Ensure null-termination
        displayUpdate(2);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0) {
            displayState.timeout = time(nullptr) + timeout;
        }
    } else if (server.hasArg("note")) {
        const String decodedNote = urlDecode(server.arg("note"));
        const char* newNote = decodedNote.c_str();
        bool changed = true;
        if (strcmp(clockState.note, newNote) == 0) {
            changed = false;
        } else {
            strncpy(clockState.note, newNote, sizeof(clockState.note));
            clockState.note[sizeof(clockState.note) - 1] = '\0'; // Ensure null-termination
        }
        clockState.noteTimeout = 0;
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0) {
            clockState.noteTimeout = time(nullptr) + timeout;
        }
        if (displayState.theme == 1 && changed) {
            displayUpdate();
        }
    } else if (server.hasArg("cnt")) {
        strncpy(countdownState.subject, urlDecode(server.arg("sbj")).c_str(), sizeof(countdownState.subject));
        countdownState.subject[sizeof(countdownState.subject) - 1] = '\0'; // Ensure null-termination
        strncpy(countdownState.datetime, urlDecode(server.arg("cnt")).c_str(), sizeof(countdownState.datetime));
        countdownState.datetime[sizeof(countdownState.datetime) - 1] = '\0'; // Ensure null-termination
        displayUpdate(4);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0) {
            if (const time_t datetime = parseDateTime(countdownState.datetime); datetime > time(nullptr)) {
                displayState.timeout = datetime + timeout;
            }
        }
    } else if (server.hasArg("brt")) {
        appSettings.brightness = server.arg("brt").toInt();
        displaySetBrightness(appSettings.brightness);
        settingsSave(appSettings);
    } else if (server.hasArg("theme")) {
        displayUpdate(server.arg("theme").toInt());
    } else if (server.hasArg("img")) {
        strncpy(displayState.image, urlDecode(server.arg("img")).c_str(), sizeof(displayState.image));
        displayState.image[sizeof(displayState.image) - 1] = '\0'; // Ensure null-termination
        displayUpdate(3);
        if (const int timeout = server.arg("timeout").toInt(); timeout > 0) {
            displayState.timeout = time(nullptr) + timeout;
        }
    } else if (server.hasArg("ip")) {
        appSettings.showIP = server.arg("ip") != "false";
        if (displayState.theme == 1) {
            displayUpdate();
        }
        settingsSave(appSettings);
    } else if (server.hasArg("sec")) {
        appSettings.showSec = server.arg("sec") != "false";
        if (displayState.theme == 1) {
            displayUpdate();
        }
        settingsSave(appSettings);
    } else if (server.hasArg("tz")) {
        strncpy(appSettings.tz, server.arg("tz").c_str(), sizeof(appSettings.tz));
        appSettings.tz[sizeof(appSettings.tz) - 1] = '\0'; // Ensure null-termination
        setenv("TZ", appSettings.tz, 1);
        tzset();
        if (displayState.theme == 1) {
            displayUpdate();
        }
        settingsSave(appSettings);
    } else {
        server.send(400, "text/plain", "No action");
        return;
    }

    server.send(200, "text/plain", "OK");
}

void handleTest() {
    displayTest();
    server.send(200, "text/plain", "OK");
}

void handleFileUpload() {
    if (!server.hasArg("dir")) {
        displayShowMessage(F("Missing dir"));
        return;
    }

    const String dir = server.arg("dir");
    if (!LittleFS.exists(dir)) {
        LittleFS.mkdir(dir);
    }

    const HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        const String filename = upload.filename;
        Serial.printf("Upload start: %s\n", filename.c_str());

        const String filepath = dir + filename;
        uploadFile = LittleFS.open(filepath, "w");

        if (!uploadFile) {
            Serial.println(F("Failed to open file for writing"));
            logPrintf("ERROR! Failed to open file %s for writing!", filepath.c_str());
        } else {
            logPrintf("Opened file %s for writing.", filepath.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            if (const size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                bytesWritten != upload.currentSize) {
                logPrintf("WARNING! Only %u of %u bytes written to file!", bytesWritten, upload.currentSize);
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            logPrintf("File %s closed. Total size: %u bytes", upload.filename.c_str(), upload.totalSize);
            Serial.printf("Upload complete: %s (%u bytes)\n",
                          upload.filename.c_str(), upload.totalSize);
        }
    }
}

void handleUploadDone() {
    server.send(200, "text/plain", "OK");

    // After upload, verify file size on LittleFS
    const String filepath = server.arg("dir") + server.upload().filename;
    if (File uploadedFile = LittleFS.open(filepath, "r")) {
        logPrintf("Actual file size on LittleFS for %s: %u bytes", filepath.c_str(), uploadedFile.size());
        uploadedFile.close();
    } else {
        logPrintf("ERROR! Could not open %s after upload to check size.", filepath.c_str());
    }
}

void handleDelete() {
    if (server.hasArg("file")) {
        if (const String filepath = server.arg("file"); LittleFS.remove(filepath)) {
            server.send(200, "text/plain", F("Deleted"));
        } else {
            server.send(404, "text/plain", F("Not found"));
        }
    } else {
        server.send(400, "text/plain", F("Missing file parameter"));
    }
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

            server.sendContent(F("<tr><td><a href='"));
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
            if (fnameLower.endsWith(".jpg")) {
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
    server.send(200, "text/html", "");

    server.sendContent(F("<table><thead><tr><th>Path</th><th>Size</th><th>Actions</th></tr></thead><tbody>\n"));
    streamDirRecursiveHtml(server.hasArg("dir") ? server.arg("dir").c_str() : "/");
    server.sendContent(F("</tbody></table>"));
    server.sendContent(""); // End of chunked response
}

// Function to handle factory reset
void handleFactoryReset() {
    server.send(200, "text/plain", "Factory Reset triggered. Clearing data and restarting...");
    delay(100); // Give time for response to send
    factoryReset();
}

void handleOTAForm() {
    server.send(200, "text/html",
                "<!DOCTYPE html><html><body>"
                "<h1>SmartClock OTA Update</h1>"
                "<form method='POST' action='/update' enctype='multipart/form-data'>"
                "<input type='file' name='update'><br><br>"
                "<input type='submit' value='Update Firmware'>"
                "</form></body></html>");
}

void handleOTAUpload() {
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA Update Start: %s\n", upload.filename.c_str());
        displayShowMessage("OTA Update...");

        const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA Success: %u bytes\n", upload.totalSize);
            displayShowMessage("Success!");
        } else {
            Update.printError(Serial);
            displayShowMessage("OTA Failed!");
        }
    }
}

void handleOTADone() {
    const bool shouldReboot = !Update.hasError();
    server.send(200, "text/plain", shouldReboot ? "OK - Rebooting..." : "FAIL");

    if (shouldReboot) {
        delay(2000);
        ESP.restart();
    }
}

void handleLog() {
    const String log = logGetAll();
    server.send(200, "text/plain", log);
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

    String json;
    serializeJson(docRoot, json);
    server.send(200, "application/json", json);
}

void handleWiFiConnect() {
    if (!server.hasArg("ssid")) {
        server.send(400, "text/plain", "Missing SSID parameter");
        return;
    }

    const String ssid = server.arg("ssid");
    const String password = server.hasArg("password") ? server.arg("password") : "";

    logPrintf("Attempting to connect to WiFi: %s", ssid.c_str());
    server.send(200, "text/plain", "Connecting to " + ssid + "...");
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
        delay(500);
        attempts++;
        yield();
    }

    if (WiFi.status() == WL_CONNECTED) {
        logPrintf("Successfully connected to %s", ssid.c_str());
        logPrintf("IP address: %s", WiFi.localIP().toString().c_str());
        displayShowMessage(F("Success!\nRebooting..."));
    } else {
        logPrintf("Failed to connect to %s", ssid.c_str());
        displayShowMessage(F("Failed :(\nRebooting..."));
    }
    delay(2000);
    ESP.restart();
}

void handleStatic() {
    String path = server.uri();

    // Check if file exists in LittleFS
    if (!LittleFS.exists(path)) {
        server.send(404, "text/plain", "File not found");
        return;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        server.send(500, "text/plain", "Failed to open file");
        return;
    }

    // Determine content type based on file extension
    String contentType = "application/octet-stream";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
        contentType = "image/jpeg";
    } else if (path.endsWith(".png")) {
        contentType = "image/png";
    } else if (path.endsWith(".bmp")) {
        contentType = "image/bmp";
    } else if (path.endsWith(".gif")) {
        contentType = "image/gif";
    }

    // Stream the file to the client
    server.streamFile(file, contentType);
    file.close();
}

void handleRoot() {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "max-age=600");
    server.send_P(200, "text/html", reinterpret_cast<const char *>(src_generated_index_html_gz),
                  src_generated_index_html_gz_len);
}

void webserverInit() {
    // GET endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/app.json", HTTP_GET, handleAppJson);
    server.on("/space.json", HTTP_GET, handleSpaceJson);
    server.on("/brt.json", HTTP_GET, handleBrtJson);
    server.on("/v.json", HTTP_GET, handleVersionJson);
    server.on("/message.json", HTTP_GET, handleMessageJson);
    server.on("/note.json", HTTP_GET, handleNoteJson);

    server.on("/filelist", HTTP_GET, handleFileList);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/set", HTTP_GET, handleSet);

    server.on("/test", HTTP_GET, handleTest);
    server.on("/log", HTTP_GET, handleLog);
    server.on("/factoryreset", HTTP_GET, handleFactoryReset);
    server.on("/scan", HTTP_GET, handleWiFiScan);
    server.on("/connect", HTTP_GET, handleWiFiConnect);

    // File upload
    server.on("/doUpload", HTTP_POST, handleUploadDone, handleFileUpload);

    // OTA
    server.on("/update", HTTP_GET, handleOTAForm);
    server.on("/update", HTTP_POST, handleOTADone, handleOTAUpload);

    // Serve images from LittleFS (catches all unhandled routes)
    server.onNotFound(handleStatic);

    server.begin();
    Serial.println(F("Web server started"));
}

void webserverHandle() {
    server.handleClient();
}
