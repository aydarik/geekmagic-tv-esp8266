#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "main.h"
#include "config.h"
#include "display.h"
#include "webserver.h"
#include "ota.h"
#include "settings.h"
#include "logger.h"
#include "button.h"
#include "utils.h"
#include "weather.h"

constexpr char NTP_SERVER[] = "pool.ntp.org";

Settings appSettings;

static unsigned long lastDisplayUpdate = 0;
static unsigned long lastWeatherUpdate = 0;
static bool powerCycleCounterCleared  = false;

// ---------------------------------------------------------------------------
// WiFi connection helpers
// ---------------------------------------------------------------------------

static bool tryConnectWiFi(const int maxAttempts) {
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

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("WiFi associated, waiting for IP..."));
            const unsigned long ipWaitStart = millis();
            while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && millis() - ipWaitStart < 10000) {
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

        if (attempt < maxAttempts) {
            int delayMs = WIFI_RETRY_DELAY_MS * (1 << (attempt - 1)); // exponential back-off
            delayMs = min(delayMs, 30000);
            Serial.printf("Retry in %d ms...\n", delayMs);
            delay(delayMs);
        }
    }
    return false;
}

static void startAPMode() {
    Serial.println(F("Entering failsafe AP mode"));
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_AP);
    yield();
    WiFi.softAP(WIFI_AP_NAME, WIFI_AP_PASSWORD);

    strncpy(displayState.ipInfo, WiFi.softAPIP().toString().c_str(), sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0';
    displayUpdate(Theme::SERVICE_AP);
}

static void setupWiFi() {
    Serial.println(F("Starting WiFi Setup..."));
    const String ssid = WiFi.SSID();
    if (ssid.isEmpty()) {
        Serial.println(F("No saved credentials — starting AP mode"));
        startAPMode();
    } else {
        if (!tryConnectWiFi(WIFI_RETRY_ATTEMPTS)) {
            Serial.println(F("Connection failed — starting AP mode"));
            startAPMode();
        }
    }
    Serial.println(F("WiFi setup completed"));
}

// ---------------------------------------------------------------------------
// Filesystem setup
// ---------------------------------------------------------------------------

static void setupFilesystem() {
    if (!LittleFS.begin()) {
        Serial.println(F("LittleFS mount failed — formatting..."));
        showMessage(F("Formatting FS..."));
        LittleFS.format();
        Serial.println(F("Formatted. Restarting..."));
        delay(2000);
        ESP.restart();
    }
    Serial.println(F("LittleFS ready"));
}

// ---------------------------------------------------------------------------
// Factory reset
// ---------------------------------------------------------------------------

void factoryReset() {
    showMessage(F("Performing\nfactory reset..."));

    // 1. EEPROM ops first — safe, no flash/WiFi stack dependency
    settingsReset(appSettings);
    showMessage(F("Settings reset"));
    delay(1000);

    powerCycleCounterReset();
    showMessage(F("Power cycles reset"));
    delay(1000);

    // 2. Unmount FS before formatting to avoid corruption
    LittleFS.end();
    LittleFS.format();
    showMessage(F("LittleFS formatted"));
    delay(1000);

    // 3. Clear WiFi credentials — must init the stack first or SDK crashes.
    //    WiFi.mode() initialises the SDK; disconnect(true) then safely erases
    //    saved SSID/password from flash and turns WiFi off.
    WiFi.mode(WIFI_STA);
    delay(100);
    yield();
    WiFi.disconnect(true);
    delay(200);

    Serial.println(F("Factory reset complete. Rebooting..."));
    showMessage(F("Success!\nRebooting..."));
    delay(2000);
    ESP.restart();
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(100);

    loggerInit();
    logPrint("Starting...");
    logPrintf("Firmware Version: %d", FIRMWARE_VERSION);

    settingsInit();
    powerCycleCounterIncrement();

    displayInit();
    displaySetBrightness(DEFAULT_BRIGHTNESS);
    showMessage(F("Starting..."));

    settingsLoad(appSettings);

    setupFilesystem();

    // Factory reset via 5 quick power cycles
    if (powerCycleCounterCheckReset()) {
        Serial.println(F("USER RESET: 5 quick power cycles detected!"));
        factoryReset();
        return;
    }

    buttonInit();
    displaySetBrightness(appSettings.brightness);

    setupWiFi();
    webserverInit();
    otaInit();

    strncpy(displayState.ipInfo, WiFi.localIP().toString().c_str(), sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0';

    // Stop here if in service (AP) mode
    if (displayState.theme == Theme::SERVICE_AP) return;

    configTzTime(appSettings.tz, NTP_SERVER);

    delay(2000);
    displayUpdate(appSettings.defaultTheme);
    lastDisplayUpdate = millis();

    logPrint("Setup complete");
}

void loop() {
    // Clear power cycle counter after 10 s of stable uptime
    if (!powerCycleCounterCleared && millis() > 10000) {
        powerCycleCounterReset();
        powerCycleCounterCleared = true;
        Serial.println(F("Power cycle counter cleared after successful boot"));
    }

    // Button handling (skip in AP service mode or during OTA)
    if (displayState.theme != Theme::SERVICE_AP && !otaIsInProgress()) {
        const ButtonPress bp = buttonUpdate();
        if (bp == BUTTON_SHORT) { displayCycleNextPage(); return; }
        if (bp == BUTTON_LONG)  { displayToggleBacklight(); return; }
    }

    const unsigned long now = millis();

    otaHandle();
    webserverHandle(); // Cleans up dead WebSocket clients

    if (otaIsInProgress()) {
        yield();
        return;
    }

    if (displayProcessPending()) {
        lastDisplayUpdate = millis();
    }

    if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = now;
        displayUpdate(Theme::NONE, false);

        if (lastWeatherUpdate == 0 || now - lastWeatherUpdate > WEATHER_UPDATE_INTERVAL) {
            if (weatherUpdateTask()) lastWeatherUpdate = now;
        }

        wsBroadcastState();
    }

    yield();
}
