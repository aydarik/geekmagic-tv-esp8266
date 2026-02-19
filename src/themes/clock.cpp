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
    // H:M\0
    strftime(buffer, bufferSize, "%H:%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedSeconds(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    // H:M\0
    strftime(buffer, bufferSize, "%S", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedDate(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    // d-m-Y\0
    strftime(buffer, bufferSize, "%d-%m-%Y", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void clearNote() {
    tft.startWrite();
    for (int i = 1; i <= 15; i++) {
        tft.fillRect(0, tft.height() - 40, tft.width(), i * 2, TFT_BLACK);
        delay(20);
    }
    tft.endWrite();
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
    int currentY = 5; // Start from top with small margin
    tft.setTextDatum(TC_DATUM);

    // Draw note
    if (bool hasNote = clockState.note[0] != '\0') {
        if (clockState.noteTimeout != 0) {
            if (now > clockState.noteTimeout) {
                clockState.note[0] = '\0';
                clockState.noteTimeout = 0;
                clearNote();
                hasNote = false;
            }
        }
        if (hasNote) {
            String lines[MAX_LINES];
            const size_t count = splitString(String(clockState.note), lines, MAX_LINES);
            if (forceClear || count > 1) {
                const unsigned int idx = sec * count / 60;
                if (forceClear) {
                    tft.loadFont(Roboto_Regular24);
                    tft.drawString(lines[idx], centerX, tft.height() - 40);
                    tft.unloadFont();
                } else {
                    const unsigned int secPrev = sec - 1 < 0 ? 59 : sec - 1;
                    const unsigned int idxPrev = secPrev * count / 60;
                    if (idxPrev != idx) {
                        clearNote(); // Clear old note first
                        tft.loadFont(Roboto_Regular24);
                        tft.drawString(lines[idx], centerX, tft.height() - 40);
                        tft.unloadFont();
                    }
                }
            }
        }
    }

    // Draw seconds
    if (appSettings.showSec) {
        char currentSeconds[4];
        getFormattedSeconds(currentSeconds, sizeof(currentSeconds), timeinfo);
        tft.drawString(currentSeconds, centerX + 71, currentY + 84, FONT_DEFAULT);
    }

    if (!forceClear && sec != 0) {
        return;
    }

    // Display IP Info at the top (small font)
    if (appSettings.showIP) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(displayState.ipInfo, centerX, currentY, FONT_MICRO);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    // Draw time
    currentY += 57;
    char currentTime[8];
    getFormattedTime(currentTime, sizeof(currentTime), timeinfo);
    int timeX = centerX;
    if (appSettings.showSec) {
        timeX -= 20;
    }
    tft.drawString(currentTime, timeX, currentY, FONT_DIGIT);

    if (!forceClear && strcmp(currentTime, "00:00") != 0) {
        return;
    }

    // Draw date
    currentY += 75;
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate), timeinfo);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString(currentDate, centerX, currentY, FONT_DEFAULT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
