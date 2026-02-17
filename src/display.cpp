#include "display.h"
#include "config.h"
#include "logger.h"
#include "settings.h"
#include "themes/clock.h"
#include "themes/ap.h"
#include "themes/notification.h"
#include "themes/countdown.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <ESP8266WiFi.h>
#include <vector>

TFT_eSPI tft = TFT_eSPI();

DisplayState displayState;

extern Settings appSettings;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void displayInit() {
    tft.init();
    tft.setTextWrap(false);

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    // Backlight on (inverted PWM: low value = bright)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    analogWriteFreq(1000); // Zet PWM frequency
    analogWriteRange(1023); // 10bit

    logPrint(F("Display init complete"));
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

    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("HELLO WORLD!", 120, 120, FONT_DEFAULT);
    delay(2000);

    displayUpdate();
}

void displayRenderImage(const bool forceClear) {
    if (!forceClear) {
        return;
    }

    const char *path = displayState.image;

    if (!LittleFS.exists(path)) {
        displayShowMessage(F("Image not found"));
        return;
    }

    File jpgFile = LittleFS.open(path, "r");
    if (!jpgFile) {
        displayShowMessage(F("Failed to open\nimage file"));
        return;
    }

    // Direct image swap without clearing screen for smooth transitions
    // The new JPEG will overwrite the previous image directly
    // Keep CS low during entire transfer to reduce overhead and speed up rendering
    tft.startWrite();
    const JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    tft.endWrite();
    if (res != JDR_OK) {
        displayShowMessage(F("Failed to\ndecode JPEG"));
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayUpdate(const int theme, const bool forceClear) {
    if (theme > 0) {
        displayState.theme = theme;
    }

    if (displayState.timeout != 0) {
        if (forceClear || displayState.theme < 1) {
            displayState.timeout = 0;
        } else if (time(nullptr) > displayState.timeout) {
            displayUpdate(1, true);
            return;
        }
    }

    switch (displayState.theme) {
        case 2:
            themeRenderNotification(forceClear);
            break;
        case 3:
            displayRenderImage(forceClear);
            break;
        case 4:
            themeRenderCountdown(forceClear);
            break;
        default:
            themeRenderClock(forceClear);
            break;
    }

    if (displayState.timeout != 0) {
        constexpr int minDelay = 60;
        if (const int diff = displayState.timeout - time(nullptr) + 1; diff <= minDelay) {
            const int currentX = diff * tft.width() / minDelay;
            const int currentY = tft.height() - 8;
            tft.drawFastHLine(0, currentY, currentX, TFT_DARKGREY);
            tft.drawFastHLine(currentX, currentY, tft.width(), TFT_BLACK);
        }
    }
}

void displayShowMessage(const String &msg) {
    displayState.theme = 0;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    int currentY = tft.height() / 2; // Starting Y position, will be adjusted
    constexpr int font = FONT_DEFAULT;
    constexpr int linesOffset = 8;

    // Calculate line height based on the font
    tft.setTextFont(font); // Set font for height calculation
    const unsigned int lineHeight = tft.fontHeight();

    // Split message by newline characters
    std::vector<String> linesToProcess;
    unsigned int prev = 0;
    for (unsigned int i = 0; i < msg.length(); i++) {
        if (msg.charAt(i) == '\n') {
            linesToProcess.push_back(msg.substring(prev, i));
            prev = i + 1;
        }
    }
    linesToProcess.push_back(msg.substring(prev)); // Add the last part

    // Adjust startY to vertically center the block of text
    const unsigned int linesCnt = linesToProcess.size();
    currentY -= linesCnt * (lineHeight + linesOffset) / 2;

    // Draw each wrapped line
    tft.startWrite();
    for (unsigned int i = 0; i < linesCnt; i++) {
        tft.drawString(linesToProcess[i], tft.width() / 2, currentY + i * (lineHeight + linesOffset), font);
    }
    tft.endWrite();
}

void displayShowAPScreen(const char *ssid, const char *password, const char *ip) {
    strncpy(displayState.ipInfo, ip, sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0'; // Ensure null-termination
    themeRenderAPMode(ssid, password);
    displayState.theme = -1;
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
        // Turn off backlight
        displaySetBrightness(0);
        backlightOn = false;
    } else {
        // Turn on backlight
        displaySetBrightness(appSettings.brightness);
        backlightOn = true;
    }
}
