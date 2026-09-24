#include <ctime>
#include "big_clock.h"
#include "settings.h"
#include "display.h"

extern Settings appSettings;

static void getFormattedHours(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%H", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

static void getFormattedMinutes(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

static void drawSecond(const int second, const bool isArrow = false, const bool clear = false) {
    constexpr float width = DISPLAY_SIZE - BIG_CLOCK_MARK_MARGIN * 2;
    const float dist = second / 60.0f * width * 4;

    float px, py;

    if (dist <= width / 2.0f) {
        // Top side: from center to right corner
        px = DISPLAY_CENTER + dist;
        py = BIG_CLOCK_MARK_MARGIN;
    } else if (dist <= width / 2.0f + width) {
        // Right side: from top corner to bottom corner
        px = DISPLAY_SIZE - BIG_CLOCK_MARK_MARGIN;
        py = BIG_CLOCK_MARK_MARGIN + (dist - width / 2.0f);
    } else if (dist <= width / 2.0f + width + width) {
        // Bottom side: from right corner to left corner
        px = DISPLAY_SIZE - BIG_CLOCK_MARK_MARGIN - (dist - (width / 2.0f + width));
        py = DISPLAY_SIZE - BIG_CLOCK_MARK_MARGIN;
    } else if (dist <= width * 4 - width / 2.0f) {
        // Left side: from bottom corner to top corner
        px = BIG_CLOCK_MARK_MARGIN;
        py = DISPLAY_SIZE - BIG_CLOCK_MARK_MARGIN - (dist - (width / 2.0f + width + width));
    } else {
        // Remaining top side: from left corner to center top
        px = BIG_CLOCK_MARK_MARGIN + (dist - (width * 4 - width / 2.0f));
        py = BIG_CLOCK_MARK_MARGIN;
    }

    float dx = px - DISPLAY_CENTER;
    float dy = py - DISPLAY_CENTER;

    if (const float len = sqrtf(dx * dx + dy * dy); len > 0.001f) {
        dx /= len;
        dy /= len;
    }

    const int tickLen = second % 5 == 0 ? BIG_CLOCK_MARK_LONG_LEN : BIG_CLOCK_MARK_SHORT_LEN;

    const int32_t x1 = static_cast<int16_t>(roundf(px - dx * tickLen / 2.0f));
    const int32_t y1 = static_cast<int16_t>(roundf(py - dy * tickLen / 2.0f));
    const int32_t x2 = static_cast<int16_t>(roundf(px + dx * tickLen / 2.0f));
    const int32_t y2 = static_cast<int16_t>(roundf(py + dy * tickLen / 2.0f));

    if (isArrow) {
        const uint32_t arrowColor = clear ? TFT_BLACK : TFT_RED;
        constexpr int thickness = 2;

        const int32_t ox = abs(x2 - DISPLAY_CENTER) > abs(y2 - DISPLAY_CENTER) ? 0 : 1;
        const int32_t oy = abs(x2 - DISPLAY_CENTER) > abs(y2 - DISPLAY_CENTER) ? 1 : 0;

        tft.startWrite();
        for (int i = -thickness; i <= thickness; ++i) {
            tft.drawLine(DISPLAY_CENTER + ox * i, DISPLAY_CENTER + oy * i, x2 + ox * i, y2 + oy * i, arrowColor);
        }

        // Restore the tick mark if we just erased a hand over it
        if (clear) {
            tft.drawLine(x1, y1, x2, y2, TFT_SILVER);
        }
        tft.endWrite();
    } else {
        tft.drawLine(x1, y1, x2, y2, clear ? TFT_BLACK : TFT_SILVER);
    }
}

void themeRenderBigClock(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    const int sec = timeinfo.tm_sec;

    // Keep track of previous position to clear it smoothly
    static int prevSec = -1;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
        prevSec = -1;
    } else if (appSettings.showSec && prevSec != -1 && sec != prevSec) {
        // Clear previous hand if it has moved
        drawSecond(prevSec, true, true);
    }

    // Stop here, no need to update the rest
    if (!forceClear && sec != 0 && !appSettings.showSec) return;

    constexpr int hourY = 18;
    constexpr int minuteY = 126;
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);

    // Draw hours
    char currentHour[4];
    getFormattedHours(currentHour, sizeof(currentHour), timeinfo);
    tft.drawString(currentHour, DISPLAY_CENTER, hourY, FONT_DIGIT);

    // Draw minutes
    char currentMinute[4];
    getFormattedMinutes(currentMinute, sizeof(currentMinute), timeinfo);
    tft.drawString(currentMinute, DISPLAY_CENTER, minuteY, FONT_DIGIT);

    // Revert font size
    tft.setTextSize(1);

    if (appSettings.showSec) {
        if (forceClear) {
            tft.startWrite();
            for (int tick = 0; tick < 60; tick++) {
                delay(ANIMATION_STEP_DELAY / 2);
                drawSecond(tick);
            }
            tft.endWrite();
        }
        drawSecond(sec, true);
    }

    // Update trackers
    prevSec = sec;
}
