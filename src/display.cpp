#include "display.h"
#include "config.h"
#include "logger.h"
#include "settings.h"
#include "utils.h"
#include "themes/clock.h"
#include "themes/ap.h"
#include "themes/notification.h"
#include "themes/countdown.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>

TFT_eSPI tft = TFT_eSPI();

DisplayState displayState;

extern Settings appSettings;

bool tft_output(const int16_t x, const int16_t y, const uint16_t w, const uint16_t h, uint16_t *bitmap) {
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
    analogWriteFreq(1000); // Zet PWM frequency
    analogWriteRange(1023); // 10bit

    logPrint("Display init complete");
}

void displaySetBrightness(int brightness) {
    brightness = constrain(brightness, 0, 100);

    // ESP8266 PWM range is 0-1023
    // Hardware is inverted: LOW = bright, HIGH = off
    int pwmValue;

    if (brightness == 0) {
        pwmValue = 1023; // Off
    } else if (brightness == 100) {
        pwmValue = 0; // Full bright
    } else {
        // Map 1-99 to 1023-0 (inverted)
        pwmValue = map(brightness, 0, 100, 1023, 0);
    }

    analogWrite(PIN_BACKLIGHT, pwmValue);
}

void displayTest() {
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_WHITE);
    delay(500);
    tft.fillScreen(TFT_BLACK);
    showMessage(F("Display test\nsuccessfully\nfinished"), 3);
}

void displayRenderImage(const bool forceClear) {
    if (!forceClear) return;

    const char *path = displayState.image;
    if (path[0] == '\0') {
        showMessage(F("No image\nselected yet"), 5);
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

    // Direct image swap without clearing screen for smooth transitions
    // The new JPEG will overwrite the previous image directly
    // Keep CS low during entire transfer to reduce overhead and speed up rendering
    tft.startWrite();
    const JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    tft.endWrite();
    if (res != JDR_OK) {
        showMessage(F("Failed to\ndecode JPEG"), 5);
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayUpdate(const int theme, const bool forceClear) {
    time_t now;
    time(&now);

    // If set manually
    if (theme != 0) displayState.theme = theme;

    if (displayState.timeout != 0) {
        if (forceClear || displayState.theme < 0) {
            displayState.timeout = 0;
        } else if (now > displayState.timeout) {
            displayUpdate(1, true);
            return;
        }
    }

    switch (displayState.theme) {
        case -1: themeRenderAPMode(forceClear);
            break;
        case 1: themeRenderClock(forceClear, now);
            break;
        case 2: themeRenderNotification(forceClear);
            break;
        case 3: displayRenderImage(forceClear);
            break;
        case 4: themeRenderCountdown(forceClear, now);
            break;
        default: break;
    }

    if (displayState.timeout > 0) {
        constexpr int minDelay = 60;
        if (const int diff = displayState.timeout - now; diff <= minDelay) {
            const int currentX = diff * tft.width() / minDelay;
            const int currentY = tft.height() - 8;
            tft.drawFastHLine(currentX, currentY, tft.width(), TFT_BLACK);
            tft.drawFastHLine(0, currentY, currentX, TFT_DARKGREY);
        }
    }
}

void displayCycleNextPage() {
    // Cycle: Clock -> Image (if available) -> Clock
    if (displayState.theme == 1) {
        // Currently showing clock, try to switch to image if available
        if (displayState.image[0] != '\0' && LittleFS.exists(displayState.image)) {
            displayUpdate(3);
        }
    } else {
        // Currently showing image, switch back to clock
        displayUpdate(1);
    }
}

// Track backlight state for toggle functionality
static bool backlightOn = true;

void displayToggleBacklight() {
    if (backlightOn) {
        displaySetBrightness(0);
        backlightOn = false;
    } else {
        displaySetBrightness(appSettings.brightness);
        backlightOn = true;
    }
}
