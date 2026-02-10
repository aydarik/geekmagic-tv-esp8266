#include "webserver.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>   // For WiFi.softAPIP()
#include <WiFiManager.h> // Include WiFiManager here

#include "generated/index_html.h"

ESP8266WebServer server(WEB_SERVER_PORT);

char currentImage[DISPLAY_PATH_BUFFER_SIZE];

extern Settings appSettings;

// File upload buffer
File uploadFile;

void handleAppJson() {
    String json = "{";
    json += "\"theme\":" + String(appSettings.theme) + ",";
    json += "\"brt\":" + String(appSettings.brightness) + ",";
    json += "\"img\":\"" + String(appSettings.lastImage) + "\",";
    json += "\"gmtOffset\":" + String(appSettings.gmtOffset);
    json += "}";
    server.send(200, "application/json", json);
}

void handleSpaceJson() {
    FSInfo fs_info;
    LittleFS.info(fs_info);

    String json = "{";
    json += "\"total\":" + String(fs_info.totalBytes) + ",";
    json += "\"free\":" + String(fs_info.totalBytes - fs_info.usedBytes);
    json += "}";
    server.send(200, "application/json", json);
}

void handleBrtJson() {
    String json = "{\"brt\":\"" + String(appSettings.brightness) + "\"}";
    server.send(200, "application/json", json);
}

void handleVersionJson() {
    String json = "{\"version\":\"" + String(FIRMWARE_VERSION_STRING) + "\"}";
    server.send(200, "application/json", json);
}

void handleSet() {
    bool updated = false;

    if (server.hasArg("brt")) {
        appSettings.brightness = server.arg("brt").toInt();
        displaySetBrightness(appSettings.brightness);
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("theme")) {
        appSettings.theme = server.arg("theme").toInt();
        displayUpdate();
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("img")) {
        strncpy(currentImage, server.arg("img").c_str(), sizeof(currentImage));
        currentImage[sizeof(currentImage) - 1] = '\0'; // Ensure null-termination
        strncpy(appSettings.lastImage, currentImage, sizeof(appSettings.lastImage));
        appSettings.theme = 3;
        displayUpdate();
        // Use sizeof for appSettings.lastImage
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("gmt")) {
        appSettings.gmtOffset = server.arg("gmt").toInt();
        timeClient.setTimeOffset(appSettings.gmtOffset);
        settingsSave(appSettings);
        updated = true;
    }

    if (server.hasArg("clear")) {
        if (server.arg("clear") == "image") {
            Dir dir = LittleFS.openDir(IMAGE_DIR);
            while (dir.next()) {
                LittleFS.remove(dir.fileName());
            }
            updated = true;
        }
    }

    server.send(200, "text/plain", updated ? "OK" : "No action");
}

void handleTest() {
    displayTest();
    server.send(200, "text/plain", "OK");
}

void handleFileUpload() {
    const HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        const String filename = upload.filename;
        Serial.printf("Upload start: %s\n", filename.c_str());

        String dir = IMAGE_DIR;
        if (server.hasArg("dir")) {
            dir = server.arg("dir");
        }

        const String filepath = dir + filename;
        uploadFile = LittleFS.open(filepath, "w");

        if (!uploadFile) {
            Serial.println(F("Failed to open file for writing"));
            logPrintf("ERROR: Failed to open file %s for writing!", filepath.c_str());
        } else {
            logPrintf("INFO: Opened file %s for writing.", filepath.c_str());
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            if (const size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
                bytesWritten != upload.currentSize) {
                logPrintf("WARNING: Only %u of %u bytes written to file!", bytesWritten, upload.currentSize);
            } else {
                logPrintf("INFO: Wrote %u bytes to file.", bytesWritten);
            }
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            logPrintf("INFO: File %s closed. Total size: %u bytes", upload.filename.c_str(), upload.totalSize);
            Serial.printf("Upload complete: %s (%u bytes)\n",
                          upload.filename.c_str(), upload.totalSize);
        }
    }
}

void handleUploadDone() {
    server.send(200, "text/plain", "OK");

    // After upload, verify file size on LittleFS
    String filename = server.upload().filename;
    String dir = IMAGE_DIR;
    if (server.hasArg("dir")) {
        dir = server.arg("dir");
    }

    const String filepath = dir + filename;
    if (File uploadedFile = LittleFS.open(filepath, "r")) {
        logPrintf("INFO: Actual file size on LittleFS for %s: %u bytes", filepath.c_str(), uploadedFile.size());
        uploadedFile.close();
    } else {
        logPrintf("ERROR: Could not open %s after upload to check size.", filepath.c_str());
    }
}

void handleDelete() {
    if (server.hasArg("file")) {
        if (const String filepath = server.arg("file"); LittleFS.remove(filepath)) {
            server.send(200, "text/plain", "Deleted");
        } else {
            server.send(404, "text/plain", "Not found");
        }
    } else {
        server.send(400, "text/plain", "Missing file parameter");
    }
}

void handleApiUpdate() {
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        JsonDocument doc;
        deserializeJson(doc, body);

        // line1 from API now goes to displayState.line2 for custom messages
        if (!doc["line1"].isNull()) {
            strncpy(displayState.line2, doc["line1"].as<const char *>(), sizeof(displayState.line2));
            displayState.line2[sizeof(displayState.line2) - 1] = '\0'; // Ensure null-termination
        } else {
            // If line1 is not provided, clear the custom message
            displayState.line2[0] = '\0';
        }
        appSettings.theme = 2;
        displayUpdate();

        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "No JSON body");
    }
}

void handleReconfigureWiFi() {
    server.send(200, "text/plain", "WiFi Reconfiguration triggered. Device restarting to AP mode.");
    delay(100); // Give time for response to send

    // Set failsafe mode and display AP credentials on screen
    wifiFailsafeMode = true;
    displayShowAPScreen(WIFI_AP_NAME, WIFI_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    delay(1000); // Give time for display update

    wifiManager.startConfigPortal(WIFI_AP_NAME, WIFI_AP_PASSWORD); // Restart into AP mode with random password

    // After config portal exits, check if connected
    if (WiFi.status() == WL_CONNECTED) {
        wifiFailsafeMode = false;
        displayShowMessage("WiFi OK\n" + WiFi.localIP().toString());
        delay(2000);
    }
    // ESP.restart(); // WiFiManager.startConfigPortal() usually reboots itself
}

// Function to handle factory reset
void handleFactoryReset() {
    server.send(200, "text/plain", "Factory Reset triggered. Clearing data and restarting...");
    delay(100); // Give time for response to send

    logPrint(F("Performing factory reset..."));

    // Clear WiFi credentials FIRST (most important)
    logPrint(F("Clearing WiFi credentials..."));
    WiFi.disconnect(true); // Disconnect and erase WiFi credentials
    delay(100);

    // Use WiFiManager to reset settings (clears WiFi config sector)
    wifiManager.resetSettings();
    logPrint(F("WiFiManager settings cleared."));
    delay(100);

    // Also erase ESP8266 WiFi config sector for complete wipe
    ESP.eraseConfig();
    logPrint(F("ESP WiFi config erased."));
    delay(100);

    // Clear EEPROM settings (our custom settings)
    settingsInit(); // Ensure EEPROM is ready
    Settings defaultSettings;
    settingsReset(defaultSettings); // Use the proper reset function
    settingsSave(defaultSettings);
    logPrint(F("EEPROM settings cleared/reset."));

    // Clear boot counter
    bootCounterReset();
    logPrint(F("Boot counter reset."));

    // Format LittleFS (delete all files)
    logPrint(F("Formatting LittleFS..."));
    LittleFS.format();
    logPrint(F("LittleFS formatted."));

    logPrint(F("Factory reset complete. Restarting..."));
    delay(1000);

    ESP.restart(); // Restart the device
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
        } else {
            const int percent = Update.progress() * 100 / Update.size();
            Serial.printf("Progress: %d%%\n", percent);
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
        delay(1000);
        ESP.restart();
    }
}

void handleLog() {
    const String log = logGetAll();
    server.send(200, "text/plain", log);
}

void handleWiFiScan() {
    logPrint(F("Starting WiFi scan..."));

    // Scan for networks (async scan to avoid blocking AP mode)
    int numNetworks = WiFi.scanNetworks(false, true); // async=false, show_hidden=true

    String json = "[";
    for (int i = 0; i < numNetworks; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }
    json += "]";

    WiFi.scanDelete(); // Clear scan results
    logPrintf("WiFi scan complete. Found %d networks", numNetworks);

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
    server.send(200, "text/plain", "Connecting to " + ssid + "... Device will restart if successful.");
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

        // Credentials are now saved, restart to apply changes
        delay(1000);
        ESP.restart();
    } else {
        logPrintf("Failed to connect to %s", ssid.c_str());
        // Restart AP mode
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
        logPrint(F("Connection failed, AP mode restarted"));
    }
}

void handleRoot() {
    server.send_P(200, "text/html", reinterpret_cast<const char *>(data_index_html), data_index_html_len);
}

void webserverInit() {
    // GET endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/app.json", HTTP_GET, handleAppJson);
    server.on("/space.json", HTTP_GET, handleSpaceJson);
    server.on("/brt.json", HTTP_GET, handleBrtJson);
    server.on("/version.json", HTTP_GET, handleVersionJson);
    server.on("/set", HTTP_GET, handleSet);
    server.on("/test", HTTP_GET, handleTest);
    server.on("/delete", HTTP_GET, handleDelete);
    server.on("/log", HTTP_GET, handleLog);
    server.on("/reconfigurewifi", HTTP_GET, handleReconfigureWiFi);
    server.on("/factoryreset", HTTP_GET, handleFactoryReset);
    server.on("/scan", HTTP_GET, handleWiFiScan);
    server.on("/connect", HTTP_GET, handleWiFiConnect);

    // POST endpoints
    server.on("/api/update", HTTP_POST, handleApiUpdate);

    // File upload
    server.on("/doUpload", HTTP_POST, handleUploadDone, handleFileUpload);

    // OTA
    server.on("/update", HTTP_GET, handleOTAForm);
    server.on("/update", HTTP_POST, handleOTADone, handleOTAUpload);

    currentImage[0] = '\0'; // Initialize currentImage as empty

    server.begin();
    Serial.println(F("Web server started"));
}

void webserverHandle() {
    server.handleClient();
}
