#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "config.h"

// Firmware model identifier (used in /v.json)
constexpr char FIRMWARE_MODEL[] = "aydarik";

// Increment when Settings structure layout changes (triggers reset on boot)
constexpr int FIRMWARE_VERSION = 3;

// Semantic version string (replaced by GitHub Action during release builds)
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "dev"
#endif

// Paths on LittleFS
constexpr char SETTINGS_PATH[]    = "/config.json";
constexpr char SECRETS_PATH[]     = "/secrets.json";
constexpr char BOOT_COUNT_PATH[]  = "/boot_count.json";

// Number of quick power cycles before factory reset
constexpr int POWER_CYCLE_THRESHOLD = 5;

// Non-sensitive settings — persisted to /config.json
struct Settings {
    int  version;         // Must match FIRMWARE_VERSION
    int  brightness;
    Theme defaultTheme;
    char tz[64];
    bool showIP;
    bool showSec;
    bool showWeather;
};

// Sensitive settings — persisted to /secrets.json (gitignored)
struct Secrets {
    char owmApiKey[64];
    char owmLocation[64];
};

void settingsInit();

void settingsLoad(Settings &settings);
void settingsSave(const Settings &settings);
void settingsReset(Settings &settings);

void secretsLoad(Secrets &secrets);
void secretsSave(const Secrets &secrets);
void secretsReset(Secrets &secrets);

// Power cycle counter (user-initiated factory reset via 5 quick power cycles)
uint8_t powerCycleCounterGet();
void    powerCycleCounterIncrement();
void    powerCycleCounterReset();
bool    powerCycleCounterCheckReset();

#endif // SETTINGS_H
