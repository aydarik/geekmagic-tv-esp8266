#include "settings.h"
#include "logger.h"
#include <EEPROM.h>

// ---------------------------------------------------------------------------
// EEPROM layout
//   [0x0000] uint16_t  SETTINGS_MAGIC
//   [0x0002] Settings  (struct)
//   [POWER_CYCLE_COUNTER_ADDR] PowerCycleCounter (struct)
// ---------------------------------------------------------------------------

#define EEPROM_SIZE               512
#define SETTINGS_MAGIC            0xCAFE
#define SETTINGS_ADDR             0
#define POWER_CYCLE_COUNTER_MAGIC 0x5C01   // "Power Cycle"
#define POWER_CYCLE_COUNTER_ADDR  (SETTINGS_ADDR + 2 + sizeof(Settings))

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void settingsInit() {
    EEPROM.begin(EEPROM_SIZE);
}

// ---------------------------------------------------------------------------
// Settings — EEPROM
// ---------------------------------------------------------------------------

static bool settingsValidate(Settings &s) {
    if (s.version != FIRMWARE_VERSION) return false;

    bool needFix = false;
    if (s.brightness < 0 || s.brightness > 100) {
        s.brightness = DEFAULT_BRIGHTNESS;
        needFix = true;
    }
    const int themeId = static_cast<int>(s.defaultTheme);
    if (themeId < 1 || themeId > THEME_COUNT) {
        s.defaultTheme = DEFAULT_THEME;
        needFix = true;
    }
    if (needFix) settingsSave(s);
    return true;
}

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
    s.owmApiKey[0]   = '\0';
    s.owmLocation[0] = '\0';
    settingsSave(s);
}

void settingsLoad(Settings &s) {
    uint16_t magic;
    EEPROM.get(SETTINGS_ADDR, magic);
    if (magic == SETTINGS_MAGIC) {
        EEPROM.get(SETTINGS_ADDR + 2, s);
        if (!settingsValidate(s)) settingsReset(s);
        else logPrint("Settings loaded from EEPROM");
    } else {
        settingsReset(s);
    }
}

void settingsSave(const Settings &s) {
    constexpr uint16_t magic = SETTINGS_MAGIC;
    EEPROM.put(SETTINGS_ADDR, magic);
    EEPROM.put(SETTINGS_ADDR + 2, s);
    EEPROM.commit();
}

// ---------------------------------------------------------------------------
// Power cycle counter — EEPROM
// ---------------------------------------------------------------------------

uint8_t powerCycleCounterGet() {
    uint16_t magic;
    EEPROM.get(POWER_CYCLE_COUNTER_ADDR, magic);
    if (magic == POWER_CYCLE_COUNTER_MAGIC) {
        PowerCycleCounter counter;
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
        counter.magic      = POWER_CYCLE_COUNTER_MAGIC;
        counter.cycleCount = 1;
    }
    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();
}

void powerCycleCounterReset() {
    PowerCycleCounter counter;
    counter.magic      = POWER_CYCLE_COUNTER_MAGIC;
    counter.cycleCount = 0;
    EEPROM.put(POWER_CYCLE_COUNTER_ADDR, counter);
    EEPROM.commit();
}

bool powerCycleCounterCheckReset() {
    return powerCycleCounterGet() >= POWER_CYCLE_THRESHOLD;
}

