#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "main.h"
#include "config.h"
#include "display.h"
#include "webserver.h"
#include "settings.h"
#include "logger.h"
#include "button.h"
#include "utils.h"
#include "weather.h"

#define NTP_SERVER "pool.ntp.org"

Settings appSettings;

unsigned long lastDisplayUpdate = 0;
unsigned long lastWeatherUpdate = 0;

bool powerCycleCounterCleared = false; // Track if power cycle counter has been reset

bool tryConnectWiFi(const int maxAttempts) {
    Serial.printf("Attempting WiFi connection (max %d attempts)...\n", maxAttempts);

    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        Serial.printf("WiFi attempt %d/%d\n", attempt, maxAttempts);
        WiFi.mode(WIFI_STA);
        WiFi.begin();

        const unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_CONNECTION_TIMEOUT) {
            delay(1000);
            yield();
        }

        // Wait for IP address to be assigned after WiFi connection
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("WiFi associated, waiting for IP..."));
            const unsigned long ipWaitStart = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) &&
                   millis() - ipWaitStart < 10000) {
                // Wait up to 10 seconds for IP
                delay(1000);
                yield();
            }
        }

        if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
            Serial.println(F("WiFi connected!"));
            showMessage(WiFi.localIP().toString());
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

void startAPMode() {
    Serial.println(F("Entering failsafe AP mode"));
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_AP);
    yield();
    WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    strncpy(displayState.ipInfo, WiFi.softAPIP().toString().c_str(), sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0'; // Ensure null-termination
    displayUpdate(-1);
}

void setupWiFi() {
    Serial.println(F("Starting WiFi Setup..."));
    // Check if WiFi credentials are saved BEFORE attempting connection
    if (const String ssid = WiFi.SSID(); ssid.isEmpty() || ssid.length() == 0) {
        Serial.println(F("No saved WiFi credentials - going directly to failsafe AP"));
        startAPMode();
    } else {
        // Try to connect to saved WiFi credentials with retry
        Serial.println(F("Attempting to connect with saved credentials..."));
        if (tryConnectWiFi(WIFI_RETRY_ATTEMPTS)) {
            Serial.println(F("Connected successfully!"));
        } else {
            Serial.println(F("No saved WiFi credentials - going directly to failsafe AP"));
            startAPMode();
        }
    }
    Serial.println(F("WiFi setup completed"));
}

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([] {
        const String type = ArduinoOTA.getCommand() == U_FLASH ? F("firmware") : F("filesystem");
        Serial.println("OTA Start: " + type);
        showMessage(F("OTA Update..."), 0, -15);
        tft.drawRect(20, 120, 200, 20, TFT_WHITE);
        tft.fillRect(22, 122, 196, 16, TFT_BLACK);
    });

    ArduinoOTA.onEnd([] {
        Serial.println(F("OTA Complete"));
        showMessage(F("Success!\nRebooting..."));
        delay(2000);
    });

    ArduinoOTA.onProgress([](const unsigned int progress, const unsigned int total) {
        const int percent = progress * 100 / total;
        static int lastPercent = -1;
        if (percent != lastPercent) {
            const int offset = percent * 196 / 100;
            tft.fillRect(22, 122, offset, 16, TFT_BLUE);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](const ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        showMessage(F("OTA Failed!"));
    });

    ArduinoOTA.begin();
    Serial.println(F("OTA ready"));
}

void setupFilesystem() {
    if (!LittleFS.begin()) {
        Serial.println(F("LittleFS mount failed. Formatting LittleFS..."));
        showMessage(F("Formatting FS..."));
        LittleFS.format(); // Format LittleFS if mounting fails
        Serial.println(F("LittleFS formatted. Restarting..."));
        delay(2000);
        ESP.restart(); // Restart after formatting
    }

    Serial.println(F("LittleFS ready"));
}

void factoryReset() {
    showMessage(F("Performing\nfactory reset..."));

    WiFi.disconnect(true);
    yield();

    ESP.eraseConfig();
    yield();

    settingsReset(appSettings);
    powerCycleCounterReset();

    LittleFS.format();
    yield();

    Serial.println(F("Factory reset complete. Rebooting..."));
    showMessage(F("Success!\nRebooting..."));
    delay(2000);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    delay(100);

    loggerInit();
    logPrint("Starting...");
    logPrintf("Firmware Version: %d", FIRMWARE_VERSION);

    // Initialize EEPROM and boot counter
    settingsInit();
    powerCycleCounterIncrement(); // Increment power cycle counter

    displayInit();
    displaySetBrightness(DEFAULT_BRIGHTNESS);
    showMessage(F("Starting..."));

    // Load and validate settings
    settingsLoad(appSettings);

    // Check for user-initiated factory reset (5 quick power cycles)
    if (powerCycleCounterCheckReset()) {
        Serial.println(F("USER RESET: 5 quick power cycles detected!"));
        factoryReset();
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

    // Stop setup if in service mode
    if (displayState.theme < 0) return;

    // NTP initialization
    configTzTime(appSettings.tz, NTP_SERVER); // Set timezone and NTP server for system time
    yield();

    displayUpdate(1);
    lastDisplayUpdate = millis();

    logPrint("Setup complete");
}

void loop() {
    // Reset power cycle counter after 10 seconds of successful uptime
    // This prevents accidental factory reset from normal reboots
    if (!powerCycleCounterCleared && millis() > 10000) {
        powerCycleCounterReset();
        powerCycleCounterCleared = true;
        Serial.println(F("Power cycle counter cleared after successful boot"));
    }

    // Cycle pages only if not in service mode
    if (displayState.theme >= 0) {
        // Handle button presses
        const ButtonPress buttonPress = buttonUpdate();
        if (buttonPress == BUTTON_SHORT) {
            displayCycleNextPage();
            return;
        }
        if (buttonPress == BUTTON_LONG) {
            displayToggleBacklight();
            return;
        }
    }

    const unsigned long now = millis();

    ArduinoOTA.handle();
    webserverHandle();

    // Updates
    if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = now;
        displayUpdate(0, false);

        if (lastWeatherUpdate == 0 || now - lastWeatherUpdate > WEATHER_UPDATE_INTERVAL) {
            if (weatherUpdateTask()) {
                lastWeatherUpdate = now;
            }
        }
    }

    yield();
}
