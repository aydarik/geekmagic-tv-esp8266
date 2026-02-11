#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "config.h"
#include "display.h"
#include "webserver.h"
#include "settings.h"
#include "logger.h"
#include "button.h"

#define NTP_SERVER "pool.ntp.org"

Settings appSettings;

WiFiManager wifiManager;

unsigned long lastDisplayUpdate = 0;

bool powerCycleCounterCleared = false; // Track if power cycle counter has been reset

bool tryConnectWiFi(int maxAttempts) {
    Serial.printf("Attempting WiFi connection (max %d attempts)...\n", maxAttempts);

    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        Serial.printf("WiFi attempt %d/%d\n", attempt, maxAttempts);

        WiFi.mode(WIFI_STA);
        WiFi.begin();

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECTION_TIMEOUT) {
            delay(1000);
        }

        // Wait for IP address to be assigned after WiFi connection
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("WiFi associated, waiting for IP..."));
            unsigned long ipWaitStart = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) &&
                   millis() - ipWaitStart < 10000) {
                // Wait up to 10 seconds for IP
                delay(1000);
            }
        }

        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            Serial.println(F("WiFi connected!"));
            displayShowMessage(WiFi.localIP().toString());
            delay(2000);
            return true;
        }

        // Exponential backoff between retries (except on last attempt)
        if (attempt < maxAttempts) {
            int delayMs = WIFI_RETRY_DELAY_MS * (1 << (attempt - 1)); // 2s, 4s, 8s, 16s...
            delayMs = min(delayMs, 30000); // Cap at 30 seconds
            Serial.printf("Retry in %d ms...\n", delayMs);
            delay(delayMs);
        }
    }
    return false;
}

void setupWiFi() {
    Serial.println(F("=== WiFi Setup Start ==="));

    // Check if WiFi credentials are saved BEFORE attempting connection
    const String ssid = WiFi.SSID();
    // Flag to indicate if we need to proceed to the Failsafe AP section
    bool needsFailsafeAP = false;
    if (ssid.isEmpty() || ssid.length() == 0) {
        Serial.println(F("No saved WiFi credentials - going directly to failsafe AP"));
        needsFailsafeAP = true;
    } else {
        // Try to connect to saved WiFi credentials with retry
        Serial.println(F("Attempting to connect with saved credentials..."));
        if (tryConnectWiFi(WIFI_RETRY_ATTEMPTS)) {
            Serial.println(F("Connected successfully!"));
            return; // Exit setupWiFi as connection is established
        }

        // Connection failed, try WiFiManager config portal
        Serial.println(F("WiFi connection failed - attempting WiFiManager config portal"));
        displayShowMessage(F("WiFi Failed!\nStarting AP..."));
        delay(2000);
        // Set timeout - don't reset settings, let WiFiManager try saved credentials first
        wifiManager.setConfigPortalTimeout(WIFI_TIMEOUT);
        Serial.printf("Starting WiFiManager autoConnect (timeout: %d seconds)...\n", WIFI_TIMEOUT);
        displayShowMessage(F("Config Portal\nStarting..."));
        yield();

        // Try autoConnect with error handling
        Serial.println(F("Calling wifiManager.autoConnect()..."));
        const bool connectedViaManager = wifiManager.autoConnect(WIFI_AP_NAME, WIFI_AP_PASSWORD);
        yield();

        Serial.printf("autoConnect returned: %s\n", connectedViaManager ? "true" : "false");
        if (!connectedViaManager) {
            needsFailsafeAP = true;
        } else {
            Serial.println(F("WiFiManager connected successfully!"));
            displayShowMessage(WiFi.localIP().toString());
            delay(2000);
            return; // Exit setupWiFi as connection is established via manager
        }
    }

    // This block is executed only if needsFailsafeAP is true
    if (needsFailsafeAP) {
        Serial.println(F("Entering failsafe AP mode"));

        // Ensure WiFi is in AP mode
        WiFi.disconnect(true);
        yield();
        WiFi.mode(WIFI_AP);
        yield();

        Serial.printf("Attempting to start AP: SSID='%s', Password='%s'\n", WIFI_AP_NAME, WIFI_AP_PASSWORD);
        bool apStarted = WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
        Serial.printf("AP Start result: %s\n", apStarted ? "SUCCESS" : "FAILED");
        if (!apStarted) {
            // If AP failed to start, try one more time after delay
            Serial.println(F("AP start failed, retrying after delay..."));
            delay(2000);
            WiFi.mode(WIFI_OFF);
            delay(500);
            WiFi.mode(WIFI_AP);
            delay(500);
            apStarted = WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);
            Serial.printf("Retry AP Start result: %s\n", apStarted ? "SUCCESS" : "FAILED");
        }

        Serial.printf("Failsafe AP started\n");
        Serial.printf("  SSID: %s\n", WIFI_AP_NAME);
        Serial.printf("  Password: %s\n", WIFI_AP_PASSWORD);
        Serial.printf("  IP: %s\n", WiFi.softAPIP().toString().c_str());
        displayShowAPScreen(WIFI_AP_NAME, WIFI_AP_PASSWORD, WiFi.softAPIP().toString().c_str());
    }

    Serial.println(F("=== WiFi Setup Complete ==="));
}

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([] {
        const String type = ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem";
        Serial.println("OTA Start: " + type);
        displayShowMessage(F("OTA Update..."));
    });

    ArduinoOTA.onEnd([] {
        Serial.println(F("OTA Complete"));
        displayShowMessage(F("Success!\nRebooting..."));
        delay(2000);
    });

    ArduinoOTA.onProgress([](const unsigned int progress, const unsigned int total) {
        const int percent = progress * 100 / total;
        Serial.printf("Progress: %u%%\n", percent);

        static int lastPercent = -1;
        if (percent != lastPercent) {
            tft.fillRect(20, 130, 200, 20, TFT_BLACK);
            tft.drawRect(20, 130, 200, 20, TFT_WHITE);
            tft.fillRect(22, 132, percent * 196 / 100, 16, TFT_BLUE);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](const ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        displayShowMessage(F("OTA Failed!"));
    });

    ArduinoOTA.begin();
    Serial.println(F("OTA ready"));
}

void setupFilesystem() {
    if (!LittleFS.begin()) {
        Serial.println(F("LittleFS mount failed. Formatting LittleFS..."));
        displayShowMessage(F("Formatting FS..."));
        LittleFS.format(); // Format LittleFS if mounting fails
        Serial.println(F("LittleFS formatted. Restarting..."));
        delay(3000);
        ESP.restart(); // Restart after formatting
    }

    if (!LittleFS.exists(IMAGE_DIR)) {
        LittleFS.mkdir(IMAGE_DIR);
    }

    Serial.println(F("LittleFS ready"));
}

void setup() {
    Serial.begin(115200);
    delay(100);

    loggerInit();
    logPrint(F("Starting..."));
    logPrintf("Firmware Version: %d", FIRMWARE_VERSION);

    // Initialize EEPROM and boot counter
    settingsInit();
    powerCycleCounterIncrement(); // Increment power cycle counter

    displayInit();
    displaySetBrightness(50);
    displayShowMessage(F("Starting..."));

    // Load and validate settings
    settingsLoad(appSettings);

    // Check for user-initiated factory reset (5 quick power cycles)
    if (powerCycleCounterCheckReset()) {
        Serial.println(F("USER RESET: 5 quick power cycles detected!"));
        displayShowMessage(F("Performing\nfactory reset..."));

        // Factory reset sequence
        WiFi.disconnect(true);
        delay(500);
        wifiManager.resetSettings();
        delay(500);

        ESP.eraseConfig();
        delay(500);

        settingsReset(appSettings);
        delay(500);

        LittleFS.format();
        delay(500);

        powerCycleCounterReset();
        delay(500);

        Serial.println(F("Factory reset complete. Rebooting..."));
        displayShowMessage(F("Success!\nRebooting..."));
        delay(2000);
        ESP.restart();
        return;
    }

    buttonInit(); // Initialize GPIO button

    displaySetBrightness(appSettings.brightness); // Restore saved brightness

    setupFilesystem();
    setupWiFi();
    webserverInit();
    setupOTA();

    strncpy(displayState.ipInfo, WiFi.localIP().toString().c_str(), sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0';

    if (displayState.theme == 0) {
        return;
    }

    // NTP initialization
    configTzTime(appSettings.tz, NTP_SERVER); // Set timezone and NTP server for system time

    displayUpdate();
    lastDisplayUpdate = millis();

    logPrint("Setup complete");
    logPrintf("IP: %s", WiFi.localIP().toString().c_str());
}

void loop() {
    // Reset power cycle counter after 10 seconds of successful uptime
    // This prevents accidental factory reset from normal reboots
    if (!powerCycleCounterCleared && millis() > 10000) {
        powerCycleCounterReset();
        powerCycleCounterCleared = true;
        Serial.println(F("Power cycle counter cleared after successful boot"));
    }

    // Don't cycle pages if in AP mode
    if (displayState.theme != 0) {
        // Handle button presses
        ButtonPress buttonPress = buttonUpdate();
        if (buttonPress == BUTTON_SHORT) {
            displayCycleNextPage();
            return;
        }
        if (buttonPress == BUTTON_LONG) {
            displayToggleBacklight();
            return;
        }
    }

    ArduinoOTA.handle();
    webserverHandle();

    // Automatic screen updates for clock rendering
    if (displayState.theme == 1 && millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        displayUpdate(false);
        lastDisplayUpdate = millis();
    }

    // Delay to save some CPU
    delay(100);
}
