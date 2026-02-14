#include "config.h"
#include "display.h"
#include "settings.h"
#include <ctime> // For time and date functions

extern Settings appSettings;

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
    int currentY = 10; // Start from top with small margin
    tft.setTextDatum(TC_DATUM);

    // Display IP Info at the top (small font)
    if (appSettings.showIP) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString(String(displayState.ipInfo), centerX, currentY, FONT_MICRO);
        currentY += 5; // Add spacing
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Draw time
    currentY += 52;
    char currentTime[8];
    getFormattedTime(currentTime, sizeof(currentTime), timeinfo);
    int timeX = centerX;
    if (appSettings.showSec) {
        timeX -= 20;
    }
    tft.drawString(String(currentTime), timeX, currentY, FONT_TIME);

    // Draw seconds
    if (appSettings.showSec) {
        char currentSeconds[4];
        getSeconds(currentSeconds, sizeof(currentSeconds), timeinfo);
        tft.drawString(String(currentSeconds), centerX + 71, currentY + 27, FONT_DEFAULT);
    }

    // Draw date
    currentY += 77;
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate), timeinfo);
    tft.drawString(String(currentDate), centerX, currentY, FONT_DEFAULT);
}
