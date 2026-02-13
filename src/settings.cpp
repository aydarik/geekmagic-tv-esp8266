#include "settings.h"

#define EEPROM_SIZE 512
#define SETTINGS_MAGIC 0xCAFE
#define SETTINGS_ADDR 0
#define POWER_CYCLE_COUNTER_MAGIC 0x5C01  // 5C = "Power Cycle"
#define POWER_CYCLE_COUNTER_ADDR (SETTINGS_ADDR + sizeof(Settings))
#define POWER_CYCLE_THRESHOLD 5  // Factory reset after 5 quick power cycles

void settingsInit() {
    EEPROM.begin(EEPROM_SIZE);
}

// Validate settings structure
bool settingsValidate(const Settings &settings) {
    // Check version compatibility
    if (settings.version != FIRMWARE_VERSION) {
        Serial.printf("Settings version mismatch: expected %d, got %d\n",
                      FIRMWARE_VERSION, settings.version);
        return false;
    }

    // Sanity checks on values
    if (settings.brightness < 0 || settings.brightness > 100) {
        Serial.println(F("Settings brightness out of range"));
        return false;
    }

    return true;
}

// Reset settings to factory defaults
void settingsReset(Settings &settings) {
    Serial.println(F("Resetting settings to factory defaults"));

    settings.version = FIRMWARE_VERSION;
    settings.brightness = 70;

    // TZ default to Europe
    strncpy(settings.tz, "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", sizeof(settings.tz));
    settings.tz[sizeof(settings.tz) - 1] = '\0'; // Ensure null-termination

    settings.showIP = true;
    settings.showSec = true;

    settingsSave(settings);
}

void settingsLoad(Settings &settings) {
    uint16_t magic;
    EEPROM.get(SETTINGS_ADDR, magic);

    if (magic == SETTINGS_MAGIC) {
        EEPROM.get(SETTINGS_ADDR + 2, settings);

        // Validate loaded settings
        if (!settingsValidate(settings)) {
            Serial.println(F("Settings validation failed - resetting to defaults"));
            settingsReset(settings);
        } else {
            Serial.println(F("Settings loaded and validated successfully"));
        }
    } else {
        Serial.println(F("No valid settings found - initializing defaults"));
        settingsReset(settings);
    }
}

void settingsSave(const Settings &settings) {
    constexpr uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, settings);
    EEPROM.commit();
    Serial.println(F("Settings saved"));
}

// Power cycle counter functions for user-initiated factory reset
uint8_t powerCycleCounterGet() {
    PowerCycleCounter counter;
    uint16_t magic;

    EEPROM.get(POWER_CYCLE_COUNTER_ADDR, magic);

    if (magic == POWER_CYCLE_COUNTER_MAGIC) {
        EEPROM.get(POWER_CYCLE_COUNTER_ADDR, counter);
        return counter.cycleCount;
    }

    return 0;
}

void powerCycleCounterIncrement() {
    PowerCycleCounter counter;
    uint16_t magic;

    EEPROM.get(POWER_CYCLE_COUNTER_ADDR, magic);

    if (magic == POWER_CYCLE_COUNTER_MAGIC) {
        EEPROM.get(POWER_CYCLE_COUNTER_ADDR, counter);
        counter.cycleCount++;
    } else {
        // Initialize power cycle counter
        counter.magic = POWER_CYCLE_COUNTER_MAGIC;
        counter.cycleCount = 1;
    }

    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.printf("Power cycle count: %d/%d\n", counter.cycleCount, POWER_CYCLE_THRESHOLD);
}

void powerCycleCounterReset() {
    PowerCycleCounter counter;
    counter.magic = POWER_CYCLE_COUNTER_MAGIC;
    counter.cycleCount = 0;

    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.println(F("Power cycle counter reset"));
}

bool powerCycleCounterCheckReset() {
    if (const uint8_t cycleCount = powerCycleCounterGet(); cycleCount >= POWER_CYCLE_THRESHOLD) {
        Serial.printf("USER RESET: Power cycle threshold reached (%d cycles)\n", cycleCount);
        return true;
    }

    return false;
}
