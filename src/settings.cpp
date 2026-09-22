#include "settings.h"
#include "logger.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static bool loadJson(const char *path, JsonDocument &doc) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    const DeserializationError err = deserializeJson(doc, f);
    f.close();
    return err == DeserializationError::Ok;
}

static bool saveJson(const char *path, const JsonDocument &doc) {
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void settingsInit() {
    // LittleFS must already be mounted by the time this is called (main.cpp)
}

// ---------------------------------------------------------------------------
// Settings (non-sensitive) — /config.json
// ---------------------------------------------------------------------------

void settingsReset(Settings &s) {
    logPrint("Resetting settings to defaults");
    s.version      = FIRMWARE_VERSION;
    s.brightness   = DEFAULT_BRIGHTNESS;
    s.defaultTheme = DEFAULT_THEME;
    strncpy(s.tz, DEFAULT_TIMEZONE, sizeof(s.tz));
    s.tz[sizeof(s.tz) - 1] = '\0';
    s.showIP      = true;
    s.showSec     = true;
    s.showWeather = false;
    settingsSave(s);
}

void settingsLoad(Settings &s) {
    JsonDocument doc;
    if (loadJson(SETTINGS_PATH, doc) && doc["version"].as<int>() == FIRMWARE_VERSION) {
        s.version      = doc["version"]  | FIRMWARE_VERSION;
        s.brightness   = doc["brt"]      | DEFAULT_BRIGHTNESS;
        s.defaultTheme = static_cast<Theme>(doc["theme"].as<int8_t>());
        const char *tz = doc["tz"] | DEFAULT_TIMEZONE;
        strncpy(s.tz, tz, sizeof(s.tz));
        s.tz[sizeof(s.tz) - 1] = '\0';
        s.showIP      = doc["showIP"]      | true;
        s.showSec     = doc["showSec"]     | true;
        s.showWeather = doc["showWeather"] | false;
        logPrint("Settings loaded from /config.json");
    } else {
        settingsReset(s);
    }
}

void settingsSave(const Settings &s) {
    JsonDocument doc;
    doc["version"]     = s.version;
    doc["brt"]         = s.brightness;
    doc["theme"]       = static_cast<int8_t>(s.defaultTheme);
    doc["tz"]          = s.tz;
    doc["showIP"]      = s.showIP;
    doc["showSec"]     = s.showSec;
    doc["showWeather"] = s.showWeather;
    saveJson(SETTINGS_PATH, doc);
}

// ---------------------------------------------------------------------------
// Secrets (sensitive) — /secrets.json
// ---------------------------------------------------------------------------

void secretsReset(Secrets &sec) {
    sec.owmApiKey[0]   = '\0';
    sec.owmLocation[0] = '\0';
    secretsSave(sec);
}

void secretsLoad(Secrets &sec) {
    JsonDocument doc;
    if (loadJson(SECRETS_PATH, doc)) {
        const char *key = doc["owmKey"] | "";
        const char *loc = doc["owmLoc"] | "";
        strncpy(sec.owmApiKey,   key, sizeof(sec.owmApiKey));
        strncpy(sec.owmLocation, loc, sizeof(sec.owmLocation));
        sec.owmApiKey[sizeof(sec.owmApiKey) - 1]     = '\0';
        sec.owmLocation[sizeof(sec.owmLocation) - 1] = '\0';
        logPrint("Secrets loaded from /secrets.json");
    } else {
        secretsReset(sec);
    }
}

void secretsSave(const Secrets &sec) {
    JsonDocument doc;
    doc["owmKey"] = sec.owmApiKey;
    doc["owmLoc"] = sec.owmLocation;
    saveJson(SECRETS_PATH, doc);
}

// ---------------------------------------------------------------------------
// Power cycle counter — /boot_count.json
// ---------------------------------------------------------------------------

uint8_t powerCycleCounterGet() {
    JsonDocument doc;
    if (loadJson(BOOT_COUNT_PATH, doc)) {
        return doc["count"] | 0;
    }
    return 0;
}

void powerCycleCounterIncrement() {
    const uint8_t count = powerCycleCounterGet() + 1;
    JsonDocument doc;
    doc["count"] = count;
    saveJson(BOOT_COUNT_PATH, doc);
}

void powerCycleCounterReset() {
    JsonDocument doc;
    doc["count"] = 0;
    saveJson(BOOT_COUNT_PATH, doc);
}

bool powerCycleCounterCheckReset() {
    return powerCycleCounterGet() >= POWER_CYCLE_THRESHOLD;
}
