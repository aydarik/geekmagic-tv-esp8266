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

// Number of quick power cycles before factory reset
constexpr int POWER_CYCLE_THRESHOLD = 5;

// Settings — persisted to EEPROM
struct Settings {
    uint16_t version;      // Must match FIRMWARE_VERSION
    int      brightness;
    Theme    defaultTheme;
    char     tz[64];
    bool     showIP;
    bool     showSec;
    bool     showWeather;
    char     owmApiKey[64];
    char     owmLocation[64];
};

// Power cycle reset structure (user-initiated factory reset)
struct PowerCycleCounter {
    uint16_t magic;      // Magic number to validate (0x5C01)
    uint8_t  cycleCount;
};

void settingsInit();

void settingsLoad(Settings &settings);
void settingsSave(const Settings &settings);
void settingsReset(Settings &settings);

// Power cycle counter (user-initiated factory reset via 5 quick power cycles)
uint8_t powerCycleCounterGet();
void    powerCycleCounterIncrement();
void    powerCycleCounterReset();
bool    powerCycleCounterCheckReset();

#endif
