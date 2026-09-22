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

constexpr char NTP_SERVER[] = "pool.ntp.org";

Settings appSettings;
Secrets  appSecrets;

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
// OTA setup
// ---------------------------------------------------------------------------

static void setupOTA() {
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
            tft.fillRect(22, 122, percent * 196 / 100, 16, TFT_BLUE);
            lastPercent = percent;
        }
    });

    ArduinoOTA.onError([](const ota_error_t error) {
        Serial.printf("OTA Error[%u]\n", error);
        showMessage(F("OTA Failed!"));
    });

    ArduinoOTA.begin();
    Serial.println(F("OTA ready"));
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

    WiFi.disconnect(true);
    yield();
    ESP.eraseConfig();
    yield();

    settingsReset(appSettings);
    secretsReset(appSecrets);
    powerCycleCounterReset();

    LittleFS.format();
    yield();

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

    setupFilesystem();       // Must come before settingsLoad (uses LittleFS)

    settingsLoad(appSettings);
    secretsLoad(appSecrets);

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
    setupOTA();

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

    // Button handling (skip in AP service mode)
    if (displayState.theme != Theme::SERVICE_AP) {
        const ButtonPress bp = buttonUpdate();
        if (bp == BUTTON_SHORT) { displayCycleNextPage(); return; }
        if (bp == BUTTON_LONG)  { displayToggleBacklight(); return; }
    }

    const unsigned long now = millis();

    ArduinoOTA.handle();
    webserverHandle(); // Cleans up dead WebSocket clients

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
