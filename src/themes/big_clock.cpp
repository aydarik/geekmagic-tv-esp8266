#include <ctime>
#include "big_clock.h"
#include "config.h"
#include "settings.h"
#include "display.h"

extern Settings appSettings;

void getFormattedHours(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%H", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedMinutes(char *buffer, const size_t bufferSize, const tm &timeinfo) {
    strftime(buffer, bufferSize, "%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void drawSecondMark(const int second, const bool isArrow = false, const bool clear = false) {
    constexpr float width = 240 - BIG_CLOCK_MARK_MARGIN * 2;
    const float dist = second / 60.0f * width * 4;

    float px, py;

    if (dist <= width / 2.0f) {
        // Верхняя сторона: от центра к правому углу
        px = 120.0f + dist;
        py = BIG_CLOCK_MARK_MARGIN;
    } else if (dist <= width / 2.0f + width) {
        // Правая сторона: от верхнего угла до нижнего
        px = 240 - BIG_CLOCK_MARK_MARGIN;
        py = BIG_CLOCK_MARK_MARGIN + (dist - width / 2.0f);
    } else if (dist <= width / 2.0f + width + width) {
        // Нижняя сторона: от правого угла до левого
        px = 240 - BIG_CLOCK_MARK_MARGIN - (dist - (width / 2.0f + width));
        py = 240 - BIG_CLOCK_MARK_MARGIN;
    } else if (dist <= width * 4 - width / 2.0f) {
        // Левая сторона: от нижнего угла до верхнего
        px = BIG_CLOCK_MARK_MARGIN;
        py = 240 - BIG_CLOCK_MARK_MARGIN - (dist - (width / 2.0f + width + width));
    } else {
        // Остаток верхней стороны: от левого угла к центру верха
        px = BIG_CLOCK_MARK_MARGIN + (dist - (width * 4 - width / 2.0f));
        py = BIG_CLOCK_MARK_MARGIN;
    }

    /* Направление от центра к палочке */
    float dx = px - 120.0f;
    float dy = py - 120.0f;
    float len = sqrtf(dx * dx + dy * dy);

    if (len > 0.001f) {
        dx /= len;
        dy /= len;
    }

    const int tickLen = second % 5 == 0 ? BIG_CLOCK_MARK_LONG_LEN : BIG_CLOCK_MARK_SHORT_LEN;

    const int32_t x1 = static_cast<int16_t>(roundf(px - dx * tickLen / 2.0f));
    const int32_t y1 = static_cast<int16_t>(roundf(py - dy * tickLen / 2.0f));
    const int32_t x2 = static_cast<int16_t>(roundf(px + dx * tickLen / 2.0f));
    const int32_t y2 = static_cast<int16_t>(roundf(py + dy * tickLen / 2.0f));

    tft.drawLine(isArrow ? 120 : x1, isArrow ? 120 : y1, x2, y2,
                 clear ? TFT_BLACK : isArrow ? TFT_RED : TFT_SILVER);

    if (isArrow && clear) {
        tft.drawLine(x1, y1, x2, y2, TFT_SILVER);
    }
}

void themeRenderBigClock(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    const int sec = timeinfo.tm_sec;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
        if (appSettings.showSec) {
            for (int second = 0; second < 60; second++) {
                drawSecondMark(second);
            }
        }
    } else if (appSettings.showSec) {
        const int prevSec = sec > 0 ? sec - 1 : 59;
        drawSecondMark(prevSec, true, true);
    }

    // Stop here, no need to update the rest
    if (!forceClear && sec != 0 && !appSettings.showSec) return;

    constexpr int hourY = 17;
    constexpr int minuteY = 123;
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

    if (appSettings.showSec) {
        drawSecondMark(sec, true);
    }
}
