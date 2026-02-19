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
    int currentY = -8;

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

    const int clockY = (tft.width() + currentY) / 2;
    // Redraw every minute, as the width may change
    if (!forceClear && ((!passed && seconds == 59) || (passed && minutes == 0 && seconds == 1))) {
        tft.fillRect(0, clockY - 30, tft.width(), 60, TFT_BLACK);
    }

    tft.setTextDatum(MC_DATUM);

    if (passed) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
    } else if (minutes == 0 && seconds <= 10) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    }
    tft.drawString(buffer, tft.height() / 2, clockY, FONT_DIGIT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Draw subject line
    if (hasSubject && forceClear) {
        drawHLine(32);
    }
}
