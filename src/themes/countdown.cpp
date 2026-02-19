#include "countdown.h"
#include "config.h"
#include "display.h"
#include "utils.h"

CountdownState countdownState;

void themeRenderCountdown(const bool forceClear, const time_t &now) {
    if (countdownState.datetime[0] == '\0') {
        if (forceClear) {
            showMessage(F("No date-time"), 5);
        }
        return;
    }

    const time_t targetTime = parseDateTime(countdownState.datetime);
    if (targetTime == 0) {
        if (forceClear) {
            showMessage(F("Not valid\ndate-time string"), 5);
        }
        return;
    }

    long diff = targetTime - now;
    const bool passed = diff < 0;
    if (passed) {
        diff *= -1;
    }

    const int minutes = diff / 60;
    const int seconds = diff % 60;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
    }

    const bool hasSubject = countdownState.subject[0] != '\0';
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        if (forceClear) {
            drawSubject(countdownState.subject);
        }
        currentY = 44;
    }

    // Draw countdown
    char buffer[8];
    if (passed) {
        sprintf(buffer, "-%d:%02d", minutes, seconds);
    } else {
        sprintf(buffer, "%d:%02d", minutes, seconds);
    }

    if (!forceClear && ((!passed && seconds == 59) || (passed && minutes == 0 && seconds == 1))) {
        constexpr int offsetY = 50;
        tft.fillRect(0, currentY + offsetY, tft.width(), tft.height() - currentY - offsetY, TFT_BLACK);
    }

    tft.setTextDatum(MC_DATUM);
    tft.drawString(buffer, tft.height() / 2, tft.width() / 2, FONT_DIGIT);

    // Draw subject line
    if (hasSubject && forceClear) {
        drawHLine(32);
    }
}
