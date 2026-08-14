#include <ctime>
#include "big_clock.h"
#include "config.h"
#include "display.h"

void getFormattedHours(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%H", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedMinutes(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void themeRenderBigClock(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Stop here, no need to update the rest
    if (const int sec = timeinfo.tm_sec; !forceClear && sec != 0) return;

    if (forceClear) tft.fillScreen(TFT_BLACK);

    constexpr int hourY = 10;
    constexpr int minuteY = 125;
    const int centerX = tft.width() / 2;
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);

    // Draw hours
    char currentHour[4];
    getFormattedHours(currentHour, sizeof(currentHour), timeinfo);
    tft.drawString(currentHour, centerX, hourY, FONT_DIGIT);

    // Draw minutes
    char currentMinute[4];
    getFormattedMinutes(currentMinute, sizeof(currentMinute), timeinfo);
    tft.drawString(currentMinute, centerX, minuteY, FONT_DIGIT);

    // Revert font size
    tft.setTextSize(1);
}
