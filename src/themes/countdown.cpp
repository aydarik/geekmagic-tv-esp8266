#include "countdown.h"
#include "config.h"
#include "display.h"
#include "utils.h"

CountdownState countdownState;

void drawGauge(const int x, const int y, const int r, const long sec, const bool passed) {
    if (!passed && sec > COUNTDOWN_GAUGE_OFFSET) return;
    const int rInner = r - 12;

    if (passed) {
        if (sec == 1)
            // Clear first if just passed
            tft.drawArc(x, y, r, rInner, 0, 360, TFT_BLACK, TFT_BLACK);

        const bool secEven = sec % 2 == 0;
        tft.drawArc(x, y, r, rInner, 120, 240, secEven ? TFT_RED : TFT_ORANGE, TFT_BLACK);
        tft.drawArc(x, y, r, rInner, 300, 60, !secEven ? TFT_RED : TFT_ORANGE, TFT_BLACK);
        return;
    }

    const int endAngle = (COUNTDOWN_GAUGE_OFFSET - sec) * 360 / COUNTDOWN_GAUGE_OFFSET;
    tft.drawArc(x, y, r, rInner, 0, endAngle, sec < 10 ? TFT_RED : TFT_ORANGE, TFT_BLACK);
}

void themeRenderCountdown(const bool forceClear, const time_t &now) {
    if (countdownState.datetime[0] == '\0') {
        if (forceClear) showMessage(F("No date-time"), 5);
        return;
    }

    const time_t targetTime = parseDateTime(countdownState.datetime);
    if (targetTime == 0) {
        if (forceClear) showMessage(F("Not valid\ndate-time string"), 5);
        return;
    }

    long diff = targetTime - now;
    const bool passed = diff < 0;
    if (passed) diff *= -1;

    const int minutes = diff / 60;
    const int seconds = diff % 60;

    if (forceClear) tft.fillScreen(TFT_BLACK);

    const bool hasSubject = countdownState.subject[0] != '\0';
    int currentY = -8;

    // Draw subject
    if (hasSubject) {
        if (forceClear) drawSubject(countdownState.subject);
        currentY = 44;
    }

    // Draw countdown
    char buffer[8];
    if (passed) sprintf(buffer, "-%d:%02d", minutes, seconds);
    else sprintf(buffer, "%d:%02d", minutes, seconds);

    const int clockY = (tft.width() + currentY) / 2;

    // Clear every 10 minutes, as the width shrinks
    if (!forceClear && !passed && seconds == 59 && (minutes + 1) % 10 == 0) {
        tft.fillRect(0, clockY - 30, tft.width(), 60, TFT_BLACK);
    }

    // Draw gauge
    drawGauge(tft.height() / 2, clockY, 100, diff, passed);

    if (passed) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
    } else if (minutes == 0 && seconds <= 10) {
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    }
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buffer, tft.height() / 2, clockY, FONT_DIGIT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Draw subject line
    if (hasSubject && forceClear) animateHLine(32);
}
