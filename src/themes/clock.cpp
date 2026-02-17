#include <ctime>
#include "clock.h"
#include "config.h"
#include "display.h"
#include "settings.h"
#include "fonts/Roboto_Regular24.h"

extern Settings appSettings;

ClockState clockState;

void getFormattedTime(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    // H:M\0
    strftime(buffer, bufferSize, "%H:%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getSeconds(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    // H:M\0
    strftime(buffer, bufferSize, "%S", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedDate(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    // d-m-Y\0
    strftime(buffer, bufferSize, "%d-%m-%Y", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void themeRenderClock(const bool forceClear) {
    time_t now;
    tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

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
                tft.fillRect(0, tft.height() - 40, tft.width(), 30, TFT_BLACK);
                hasNote = false;
            }
        }
        if (hasNote && forceClear) {
            tft.loadFont(Roboto_Regular24);
            tft.drawString(clockState.note, centerX, tft.height() - 40);
            tft.unloadFont();
        }
    }

    // Draw seconds
    char currentSeconds[4];
    getSeconds(currentSeconds, sizeof(currentSeconds), timeinfo);
    if (appSettings.showSec) {
        int secondsY = currentY + 84;
        tft.drawString(currentSeconds, centerX + 71, secondsY, FONT_DEFAULT);
    }

    if (!forceClear && strcmp(currentSeconds, "00") != 0) {
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
