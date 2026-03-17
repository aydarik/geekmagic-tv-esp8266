#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

// Firmware model
#define FIRMWARE_MODEL "aydarik"
// Firmware version - increment when Settings structure changes
#define FIRMWARE_VERSION 1

// Semantic version string (replaced by GitHub Action during release builds)
#ifndef FIRMWARE_VERSION_STRING
#define FIRMWARE_VERSION_STRING "dev"
#endif

struct Settings {
    uint16_t version; // Firmware version for compatibility check
    int brightness;
    char tz[64];
    bool showIP;
    bool showSec;
    bool showWeather;
    char owmApiKey[64];
    char owmLocation[64];
};

// Power cycle reset structure (user-initiated factory reset)
struct PowerCycleCounter {
    uint16_t magic; // Magic number to validate power cycle counter (0x5C01)
    uint8_t cycleCount; // Number of quick power cycles
};

void settingsInit();

void settingsLoad(Settings &settings);

void settingsSave(const Settings &settings);

void settingsReset(Settings &settings);

bool settingsValidate(const Settings &settings);

// Power cycle counter functions (user-initiated factory reset)
uint8_t powerCycleCounterGet();

void powerCycleCounterIncrement();

void powerCycleCounterReset();

bool powerCycleCounterCheckReset();

#endif
