#include "display.h"
#include "config.h"
#include "logger.h"
#include "settings.h"
#include "utils.h"
#include "themes/clock.h"
#include "themes/big_clock.h"
#include "themes/analog.h"
#include "themes/ap.h"
#include "themes/notification.h"
#include "themes/countdown.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>

TFT_eSPI tft = TFT_eSPI();

DisplayState displayState;

extern Settings appSettings;

static Theme lastTheme = DEFAULT_THEME;
static bool backlightOn = true;

static bool tft_output(const int16_t x, const int16_t y, const uint16_t w, const uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void displayInit() {
    tft.init();
    tft.setTextWrap(false);
    tft.setTextFont(FONT_DEFAULT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    // Backlight on (inverted PWM: low value = bright)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    analogWriteFreq(1000);
    analogWriteRange(1023); // 10-bit
    backlightOn = analogRead(PIN_BACKLIGHT) < 1023;

    logPrint("Display init complete");
}

void displaySetBrightness(int brightness) {
    brightness = constrain(brightness, 0, 100);

    // ESP8266 PWM range is 0-1023; hardware is inverted: LOW = bright, HIGH = off
    int pwmValue;
    if (brightness == 0) {
        pwmValue = 1023;
    } else if (brightness == 100) {
        pwmValue = 0;
    } else {
        pwmValue = map(brightness, 0, 100, 1023, 0);
    }

    backlightOn = pwmValue < 1023;
    analogWrite(PIN_BACKLIGHT, pwmValue);
}

void displayTest() {
    tft.fillScreen(TFT_RED);   delay(500);
    tft.fillScreen(TFT_GREEN); delay(500);
    tft.fillScreen(TFT_BLUE);  delay(500);
    tft.fillScreen(TFT_WHITE); delay(500);
    tft.fillScreen(TFT_BLACK);
    showMessage(F("Display test\nsuccessfully\nfinished"), 3);
}

static void displayRenderImage(const bool forceClear) {
    if (!forceClear) return;

    const char *path = displayState.image;
    if (path[0] == '\0') {
        showMessage(F("No image\nselected yet"), 5);
        return;
    }
    if (!LittleFS.exists(path)) {
        showMessage(F("Image not found"), 5);
        return;
    }

    File jpgFile = LittleFS.open(path, "r");
    if (!jpgFile) {
        showMessage(F("Failed to open\nimage file"), 5);
        return;
    }

    tft.startWrite();
    const JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    tft.endWrite();
    jpgFile.close();

    if (res != JDR_OK) {
        showMessage(F("Failed to\ndecode JPEG"), 5);
    }
}

void displayUpdate(const Theme theme, const bool forceClear) {
    const time_t now = time(nullptr);

    // Apply explicit theme change
    if (theme != Theme::NONE) displayState.theme = theme;

    // Clamp to valid range (cast to int for comparison)
    const auto themeId = static_cast<int8_t>(displayState.theme);
    if (themeId > THEME_COUNT || themeId < -1)
        displayState.theme = DEFAULT_THEME;

    // Handle timeout
    if (displayState.timeout != 0) {
        if (forceClear || displayState.theme == Theme::SERVICE_AP) {
            displayState.timeout = 0;
        } else if (now > displayState.timeout) {
            displayState.timeout = 0;
            displayUpdate(appSettings.defaultTheme, true);
            return;
        }
    }

    if (displayState.theme != Theme::NONE)
        lastTheme = displayState.theme;

    switch (displayState.theme) {
        case Theme::SERVICE_AP:   themeRenderAPMode(forceClear);             break;
        case Theme::CLOCK:        themeRenderClock(forceClear, now);         break;
        case Theme::NOTIFICATION: themeRenderNotification(forceClear);       break;
        case Theme::IMAGE:        displayRenderImage(forceClear);            break;
        case Theme::COUNTDOWN:    themeRenderCountdown(forceClear, now);     break;
        case Theme::BIG_CLOCK:    themeRenderBigClock(forceClear, now);      break;
        case Theme::ANALOG:       themeRenderAnalog(forceClear, now);        break;
        default: break;
    }

    // Draw timeout progress bar (last 60 seconds)
    if (displayState.timeout > 0) {
        constexpr int minDelay = 60;
        if (const int diff = static_cast<int>(displayState.timeout - now); diff <= minDelay) {
            const int currentX = diff * tft.width() / minDelay;
            const int currentY = tft.height() - 8;
            tft.drawFastHLine(currentX, currentY, tft.width(), TFT_BLACK);
            tft.drawFastHLine(0, currentY, currentX, TFT_DARKGREY);
        }
    }
}

int8_t displayCycleNextPage(const bool async) {
    if (static_cast<int8_t>(displayState.theme) < 0) {
        return static_cast<int8_t>(displayState.theme);
    }

    static constexpr std::array cycleOrder = {
        Theme::CLOCK,
        Theme::BIG_CLOCK,
        Theme::ANALOG,
        Theme::NOTIFICATION,
        Theme::COUNTDOWN,
        Theme::IMAGE
    };

    // Find current index
    size_t nextIdx = 0;
    for (size_t i = 0; i < cycleOrder.size(); ++i) {
        if (cycleOrder[i] == lastTheme) {
            nextIdx = (i + 1) % cycleOrder.size();
            break;
        }
    }

    lastTheme = cycleOrder[nextIdx];
    if (async) {
        displayScheduleUpdate(lastTheme, true);
    } else {
        displayUpdate(lastTheme, true);
    }

    return static_cast<int8_t>(lastTheme);
}

bool displayToggleBacklight() {
    if (backlightOn and static_cast<int8_t>(displayState.theme) <= 0) {
        return backlightOn;
    }

    if (backlightOn) {
        for (int i = appSettings.brightness; i >= 0; --i) {
            delay(ANIMATION_STEP_DELAY);
            displaySetBrightness(i);
        }
    } else {
        for (int i = 0; i <= appSettings.brightness; ++i) {
            delay(ANIMATION_STEP_DELAY);
            displaySetBrightness(i);
        }
    }

    return backlightOn;
}

struct DisplaySchedule {
    volatile bool   pending    = false;
    volatile Theme  theme      = Theme::NONE;
    volatile bool   forceClear = true;
    volatile time_t timeout    = 0;
    volatile bool   runTest    = false;
};

static DisplaySchedule displaySchedule;

void displayScheduleUpdate(const Theme theme, const bool forceClear, const time_t timeout) {
    displaySchedule.theme      = theme;
    displaySchedule.forceClear = forceClear;
    displaySchedule.timeout    = timeout;
    displaySchedule.pending    = true;
}

void displayScheduleTest() {
    displaySchedule.runTest = true;
    displaySchedule.pending = true;
}

bool displayProcessPending() {
    if (!displaySchedule.pending) return false;

    displaySchedule.pending = false;

    if (displaySchedule.runTest) {
        displaySchedule.runTest = false;
        displayTest();
        return true;
    }

    displayUpdate(displaySchedule.theme, displaySchedule.forceClear);
    if (displaySchedule.timeout > 0) {
        displayState.timeout = displaySchedule.timeout;
    }
    return true;
}

