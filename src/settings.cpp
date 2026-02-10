#include "settings.h"

#define EEPROM_SIZE 512
#define SETTINGS_MAGIC 0xCAFE
#define SETTINGS_ADDR 0
#define BOOT_COUNTER_MAGIC 0xB007
#define BOOT_COUNTER_ADDR (SETTINGS_ADDR + sizeof(uint16_t) + sizeof(Settings))
#define BOOT_FAILURE_THRESHOLD 5  // Reset EEPROM after 5 consecutive boot failures
#define POWER_CYCLE_COUNTER_MAGIC 0x5C01  // 5C = "Power Cycle"
#define POWER_CYCLE_COUNTER_ADDR (BOOT_COUNTER_ADDR + sizeof(BootCounter))
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

    if (settings.theme < 0 || settings.theme > 10) {
        Serial.println(F("Settings theme out of range"));
        return false;
    }

    return true;
}

// Reset settings to factory defaults
void settingsReset(Settings &settings) {
    Serial.println(F("Resetting settings to factory defaults"));

    settings.version = FIRMWARE_VERSION;
    settings.brightness = 70;
    settings.theme = 1; // 1 - clock, 2 - message, 3 - image
    settings.lastImage[0] = '\0';
    settings.gmtOffset = 3600; // Default to +1 hour (CET)
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
            settingsSave(settings); // Save valid defaults
        } else {
            Serial.println(F("Settings loaded and validated successfully"));
        }
    } else {
        Serial.println(F("No valid settings found - initializing defaults"));
        settingsReset(settings);
        settingsSave(settings); // Save defaults on first boot
    }
}

void settingsSave(const Settings &settings) {
    constexpr uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, settings);
    EEPROM.commit();

    Serial.println(F("Settings saved"));
}

// Boot counter functions for failure detection
void bootCounterInit() {
    // Boot counter is already initialized by EEPROM.begin()
    // Just increment the failure counter
    bootCounterIncrement();
}

uint8_t bootCounterGet() {
    BootCounter counter;
    uint16_t magic;

    EEPROM.get(BOOT_COUNTER_ADDR, magic);

    if (magic == BOOT_COUNTER_MAGIC) {
        EEPROM.get(BOOT_COUNTER_ADDR, counter);
        return counter.failCount;
    }

    return 0;
}

void bootCounterIncrement() {
    BootCounter counter;
    uint16_t magic;

    EEPROM.get(BOOT_COUNTER_ADDR, magic);

    if (magic == BOOT_COUNTER_MAGIC) {
        EEPROM.get(BOOT_COUNTER_ADDR, counter);
        counter.failCount++;
    } else {
        // Initialize boot counter
        counter.magic = BOOT_COUNTER_MAGIC;
        counter.failCount = 1;
        counter.lastBootTime = 0;
    }

    EEPROM.put(BOOT_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.printf("Boot failure count: %d\n", counter.failCount);
}

void bootCounterReset() {
    BootCounter counter;
    counter.magic = BOOT_COUNTER_MAGIC;
    counter.failCount = 0;
    counter.lastBootTime = millis();

    EEPROM.put(BOOT_COUNTER_ADDR, counter);
    EEPROM.commit();

    Serial.println(F("Boot counter reset"));
}

bool bootCounterCheckFailsafe() {
    uint8_t failCount = bootCounterGet();

    if (failCount >= BOOT_FAILURE_THRESHOLD) {
        Serial.printf("FAILSAFE: Boot failure threshold reached (%d failures)\n", failCount);
        return true;
    }

    return false;
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
