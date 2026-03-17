#include "settings.h"

#include "config.h"
#include "logger.h"

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
    if (settings.version != FIRMWARE_VERSION) return false;
    if (settings.brightness < 0 || settings.brightness > 100) return false;
    return true;
}

// Reset settings to factory defaults
void settingsReset(Settings &settings) {
    logPrint("Resetting settings...");

    settings.version = FIRMWARE_VERSION;
    settings.brightness = DEFAULT_BRIGHTNESS;

    // TZ default to Europe
    strncpy(settings.tz, DEFAULT_TIMEZONE, sizeof(settings.tz));
    settings.tz[sizeof(settings.tz) - 1] = '\0'; // Ensure null-termination

    settings.showIP = true;
    settings.showSec = true;

    settings.showWeather = false;
    settings.owmApiKey[0] = '\0';
    settings.owmLocation[0] = '\0';

    settingsSave(settings);
}

void settingsLoad(Settings &settings) {
    uint16_t magic;
    EEPROM.get(SETTINGS_ADDR, magic);
    if (magic == SETTINGS_MAGIC) {
        EEPROM.get(SETTINGS_ADDR + 2, settings);
        if (!settingsValidate(settings)) settingsReset(settings);
        else logPrint("Settings loaded");
    } else {
        settingsReset(settings);
    }
}

void settingsSave(const Settings &settings) {
    constexpr uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, settings);
    EEPROM.commit();
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
}

void powerCycleCounterReset() {
    PowerCycleCounter counter;
    counter.magic = POWER_CYCLE_COUNTER_MAGIC;
    counter.cycleCount = 0;
    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();
}

bool powerCycleCounterCheckReset() {
    if (const uint8_t cycleCount = powerCycleCounterGet(); cycleCount >= POWER_CYCLE_THRESHOLD)
        return true;
    return false;
}
