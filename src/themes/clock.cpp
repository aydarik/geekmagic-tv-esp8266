#include <ctime>
#include "clock.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "utils.h"
#include "weather.h"
#include "fonts/Roboto_Regular24.h"

extern Settings appSettings;

ClockState clockState;

void getFormattedTime(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%H:%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedSeconds(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%S", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedDate(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%d-%m-%Y", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void clearNote() {
    tft.startWrite();
    for (int i = 1; i <= 30 / 2; ++i) {
        tft.drawFastHLine(0, 195 + i, 240, TFT_BLACK);
        tft.drawFastHLine(0, 225 - i, 240, TFT_BLACK);
        delay(ANIMATION_STEP_DELAY);
    }
    tft.endWrite();
}

void themeRenderClock(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Calculate seconds
    const int sec = timeinfo.tm_sec;

    if (forceClear) tft.fillScreen(TFT_BLACK);

    const int centerX = tft.width() / 2;
    tft.setTextDatum(TC_DATUM);

    const bool hasNote = clockState.note[0] != '\0';
    const int clockY = (hasNote ? 50 : 63) + (appSettings.showIP ? 7 : 0) + (appSettings.showWeather ? 17 : 0);

    // Draw seconds
    if (appSettings.showSec) {
        char currentSeconds[4];
        getFormattedSeconds(currentSeconds, sizeof(currentSeconds), timeinfo);
        tft.drawString(currentSeconds, centerX + 71, clockY + 27, FONT_DEFAULT);
    }

    // Draw or clear a note
    if (hasNote) {
        if (clockState.noteTimeout != 0 && now > clockState.noteTimeout) {
            clockState.note[0] = '\0';
            clockState.noteTimeout = 0;
            clockState.noteRotations = 0;
            clearNote();
            themeRenderClock(true, now);
            return;
        }

        String lines[MAX_LINES];
        const size_t count = splitString(String(clockState.note), lines, MAX_LINES);
        const unsigned int rotations = clockState.noteRotations > count ? clockState.noteRotations : count;
        const unsigned int idx = sec * rotations / 60 % count;
        const unsigned int idxPrev = (sec == 0 ? 59 : sec - 1) * rotations / 60 % count;

        // Clear always on second 0 in case there is a single line note change
        if ((idxPrev != idx || sec == 0) && !forceClear) {
            clearNote(); // Clear old note first
        }
        if (idx != idxPrev || sec == 0 || forceClear) {
            tft.loadFont(Roboto_Regular24);
            tft.drawString(lines[idx], centerX, tft.height() - 40);
            tft.unloadFont();
        }
    }

    // Stop here, no need to update the rest
    if (!forceClear && sec != 0) return;

    // Display IP Info at the top (small font)
    if (forceClear && appSettings.showIP) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(displayState.ipInfo, centerX, appSettings.showWeather ? 40 : 5, FONT_MICRO);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    // Weather information
    if (forceClear && appSettings.showWeather) {
        renderWeather();
        tft.setTextDatum(TC_DATUM);
    }

    // Draw time
    char currentTime[8];
    getFormattedTime(currentTime, sizeof(currentTime), timeinfo);
    tft.drawString(currentTime, appSettings.showSec ? centerX - 20 : centerX, clockY, FONT_DIGIT);

    // Stop here, no need to update the date
    if (!forceClear && strcmp(currentTime, "00:00") != 0) return;

    // Draw date
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate), timeinfo);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    const int32_t offset = appSettings.showWeather && hasNote ? 65 : appSettings.showWeather || hasNote ? 70 : 75;
    tft.drawString(currentDate, centerX, clockY + offset, FONT_DEFAULT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
