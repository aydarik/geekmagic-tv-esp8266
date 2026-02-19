#include <ctime>
#include "clock.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "utils.h"
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
    tft.fillRect(0, tft.height() - 45, tft.width(), 30, TFT_BLACK);
}

void themeRenderClock(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Calculate seconds
    const int sec = timeinfo.tm_sec;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
    }

    const int centerX = tft.width() / 2;
    tft.setTextDatum(TC_DATUM);

    const bool hasNote = clockState.note[0] != '\0';
    const int clockY = (hasNote ? 50 : 63) + (appSettings.showIP ? 7 : 0);

    // Draw seconds
    if (appSettings.showSec) {
        char currentSeconds[4];
        getFormattedSeconds(currentSeconds, sizeof(currentSeconds), timeinfo);
        tft.drawString(currentSeconds, centerX + 71, clockY + 26, FONT_DEFAULT);
    }

    // Draw or clear a note
    if (hasNote) {
        if (clockState.noteTimeout != 0 && now > clockState.noteTimeout) {
            clockState.note[0] = '\0';
            clockState.noteTimeout = 0;
            clearNote();
            themeRenderClock(true, now);
            return;
        }

        String lines[MAX_LINES];
        const size_t count = splitString(String(clockState.note), lines, MAX_LINES);
        const unsigned int idx = sec * count / 60;
        const unsigned int idxPrev = (sec - 1 < 0 ? 59 : sec - 1) * count / 60;
        if ((idxPrev != idx || sec == 0) && !forceClear) {
            clearNote(); // Clear old note first
        }
        if (idx != idxPrev || sec == 0 || forceClear) {
            tft.loadFont(Roboto_Regular24);
            tft.drawString(lines[idx], centerX, tft.height() - 40);
            tft.unloadFont();
        }
    }

    if (!forceClear && sec != 0) {
        // Stop here, no need to update the rest
        return;
    }

    // Display IP Info at the top (small font)
    if (appSettings.showIP) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(displayState.ipInfo, centerX, 5, FONT_MICRO);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    // Draw time
    char currentTime[8];
    getFormattedTime(currentTime, sizeof(currentTime), timeinfo);
    tft.drawString(currentTime, appSettings.showSec ? centerX - 20 : centerX, clockY, FONT_DIGIT);

    if (!forceClear && strcmp(currentTime, "00:00") != 0) {
        // Stop here, no need to update the date
        return;
    }

    // Draw date
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate), timeinfo);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(currentDate, centerX, clockY + 75, FONT_DEFAULT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
