#include <ctime>
#include "analog.h"
#include "settings.h"
#include "display.h"

extern Settings appSettings;

enum class DrawType : int8_t {
    TICK   = 0,
    HOUR   = 1,
    MINUTE = 2,
    SECOND = 3,
};

static void drawArrow(const int value, const DrawType type, const bool clear = false) {
    constexpr float width = DISPLAY_SIZE - ANALOG_MARK_MARGIN * 2;
    const float dist = value / 60.0f * width * 4;

    float px, py;

    if (dist <= width / 2.0f) {
        // Top side: from center to right corner
        px = DISPLAY_CENTER + dist;
        py = ANALOG_MARK_MARGIN;
    } else if (dist <= width / 2.0f + width) {
        // Right side: from top corner to bottom corner
        px = DISPLAY_SIZE - ANALOG_MARK_MARGIN;
        py = ANALOG_MARK_MARGIN + (dist - width / 2.0f);
    } else if (dist <= width / 2.0f + width + width) {
        // Bottom side: from right corner to left corner
        px = DISPLAY_SIZE - ANALOG_MARK_MARGIN - (dist - (width / 2.0f + width));
        py = DISPLAY_SIZE - ANALOG_MARK_MARGIN;
    } else if (dist <= width * 4 - width / 2.0f) {
        // Left side: from bottom corner to top corner
        px = ANALOG_MARK_MARGIN;
        py = DISPLAY_SIZE - ANALOG_MARK_MARGIN - (dist - (width / 2.0f + width + width));
    } else {
        // Remaining top side: from left corner to center top
        px = ANALOG_MARK_MARGIN + (dist - (width * 4 - width / 2.0f));
        py = ANALOG_MARK_MARGIN;
    }

    float dx = px - DISPLAY_CENTER;
    float dy = py - DISPLAY_CENTER;

    if (const float len = sqrtf(dx * dx + dy * dy); len > 0.001f) {
        dx /= len;
        dy /= len;
    }

    const int tickLen = value % 5 == 0 ? ANALOG_MARK_LONG_LEN : ANALOG_MARK_SHORT_LEN;

    const int32_t x1 = static_cast<int16_t>(roundf(px - dx * tickLen / 2.0f));
    const int32_t y1 = static_cast<int16_t>(roundf(py - dy * tickLen / 2.0f));
    const int32_t x2 = static_cast<int16_t>(roundf(px + dx * tickLen / 2.0f));
    const int32_t y2 = static_cast<int16_t>(roundf(py + dy * tickLen / 2.0f));

    if (type == DrawType::TICK) {
        tft.drawLine(x1, y1, x2, y2, clear ? TFT_BLACK : TFT_SILVER);
    } else {
        uint32_t arrowColor = TFT_WHITE;
        float scale = 1.0f;
        int thickness = 2;

        // Configure styles based on the type of hand
        if (type == DrawType::SECOND) {
            arrowColor = TFT_RED;
        } else if (type == DrawType::MINUTE) {
            thickness = 4;       // Medium thickness
        } else if (type == DrawType::HOUR) {
            arrowColor = TFT_SILVER; // Differentiate hour color slightly
            scale = 0.60f;       // Shortest hand
            thickness = 8;       // Thickest
        }

        if (clear) {
            arrowColor = TFT_BLACK;
        }

        // Scale the hand length so they don't all go straight to the edge
        const int32_t endX = DISPLAY_CENTER + (x2 - DISPLAY_CENTER) * scale;
        const int32_t endY = DISPLAY_CENTER + (y2 - DISPLAY_CENTER) * scale;

        const int32_t ox = abs(x2 - DISPLAY_CENTER) > abs(y2 - DISPLAY_CENTER) ? 0 : 1;
        const int32_t oy = abs(x2 - DISPLAY_CENTER) > abs(y2 - DISPLAY_CENTER) ? 1 : 0;

        tft.startWrite();
        for (int i = -thickness; i <= thickness; ++i) {
            tft.drawLine(DISPLAY_CENTER + ox * i, DISPLAY_CENTER + oy * i, endX + ox * i, endY + oy * i, arrowColor);
        }

        // Restore the tick mark if we just erased a hand over it
        if (clear) {
            tft.drawLine(x1, y1, x2, y2, TFT_SILVER);
        }
        tft.endWrite();
    }
}

void themeRenderAnalog(const bool forceClear, const time_t &now) {
    tm timeinfo;
    localtime_r(&now, &timeinfo);

    const int sec = timeinfo.tm_sec;
    const int min = timeinfo.tm_min;
    const int hr = timeinfo.tm_hour;

    // Map the hour (0-11) and current minutes to a 0-59 scale for the perimeter mapping
    const int hourVal = hr % 12 * 5 + min / 12;

    // Keep track of previous positions to clear them smoothly
    static int prevSec = -1;
    static int prevMin = -1;
    static int prevHourVal = -1;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
        prevSec = -1;
        prevMin = -1;
        prevHourVal = -1;
    } else {
        // Clear previous hands if they have moved
        if (appSettings.showSec && prevSec != -1 && sec != prevSec) {
            drawArrow(prevSec, DrawType::SECOND, true);
        }
        if (prevMin != -1 && min != prevMin) {
            drawArrow(prevMin, DrawType::MINUTE, true);
        }
        if (prevHourVal != -1 && hourVal != prevHourVal) {
            drawArrow(prevHourVal, DrawType::HOUR, true);
        }
    }

    // Determine if any screen update is actually needed right now
    const bool needsUpdate = forceClear ||
                       (appSettings.showSec && sec != prevSec) ||
                       min != prevMin ||
                       hourVal != prevHourVal;

    if (!needsUpdate) return;

    if (forceClear) {
        tft.startWrite();
        for (int tick = 0; tick < 60; tick++) {
            drawArrow(tick, DrawType::TICK);
        }
        tft.endWrite();
    }

    // Always redraw current hands from bottom to top so they overlap properly
    drawArrow(hourVal, DrawType::HOUR);
    drawArrow(min, DrawType::MINUTE);
    if (appSettings.showSec) {
        drawArrow(sec, DrawType::SECOND);
    }

    // Update trackers
    prevSec = sec;
    prevMin = min;
    prevHourVal = hourVal;
}