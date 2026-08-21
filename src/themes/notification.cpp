#include "notification.h"
#include "config.h"
#include "display.h"
#include "utils.h"
#include "fonts/NotoSans_Regular24.h"

NotificationState notificationState;

static GaugeState gaugeState;

static void parseValue(const char *input) {
    char buffer[32];
    strncpy(buffer, input, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Split unit if space exists
    char *spacePtr = strchr(buffer, ' ');
    if (spacePtr != nullptr) {
        *spacePtr = '\0'; // terminate value part
        strncpy(gaugeState.unit, spacePtr + 1, GAUGE_UNIT_BUFFER_SIZE - 1);
        gaugeState.unit[GAUGE_UNIT_BUFFER_SIZE - 1] = '\0';
    } else {
        gaugeState.unit[0] = '\0';
    }

    // Check for slash
    if (char *slashPtr = strchr(buffer, '/'); slashPtr != nullptr) {
        *slashPtr = '\0';
        gaugeState.current = atof(buffer);
        gaugeState.max = atof(slashPtr + 1);
    } else {
        gaugeState.current = atof(buffer);
        gaugeState.max = 0;
    }
}

static void drawGauge(const int x, const int y, const int r, const float current, const float max) {
    if (max <= 0.0f) return;

    float percent = current / max;
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 1.0f) percent = 1.0f;

    constexpr int startAngle = 55;
    constexpr int endAngle = 305;
    constexpr int totalAngle = endAngle - startAngle;
    tft.drawArc(x, y, r, r - 12, startAngle, endAngle, TFT_DARKGREY, TFT_BLACK);

    const bool hasSubject = notificationState.subject[0] != '\0';
    const int subjectOffset = hasSubject ? DISPLAY_CENTER / ANIMATION_STEPS : 0;

    const uint32_t gaugeColor = percent < 0.2f || percent > 0.8f ? TFT_RED : TFT_OLIVE;

    tft.startWrite();
    for (int i = 1; i <= ANIMATION_STEPS; ++i) {
        const int currentAngle = startAngle + totalAngle * percent * static_cast<float>(i) / ANIMATION_STEPS;
        tft.drawArc(x, y, r, r - 12, startAngle, currentAngle, gaugeColor, TFT_BLACK);
        delay(ANIMATION_STEP_DELAY);
    }
    tft.endWrite();
}

static void showNumber(const char *input, const int x, const int y) {
    parseValue(input);
    const bool hasGauge = gaugeState.max > 0;
    const bool hasUnit = gaugeState.unit[0] != '\0';

    // Draw current value
    char numBuffer[8];
    const float val = gaugeState.current;
    dtostrf(val, 0, isfinite(val) && val == truncf(val) ? 0 : 1, numBuffer);

    tft.setTextDatum(MC_DATUM);
    const int currentY = hasGauge ? y + 10 : hasUnit ? y - 16 : y;
    tft.drawString(numBuffer, x, currentY, FONT_DIGIT);

    // Draw unit
    if (hasUnit) {
        tft.loadFont(NotoSans_Regular24);
        tft.drawString(gaugeState.unit, x, currentY + 60);
        tft.unloadFont();
    }

    if (hasGauge) {
        drawGauge(x, y + 15, 100, gaugeState.current, gaugeState.max);
    }
}

static void showNotification(const int maxLines, const int y) {
    char *wrapped[maxLines];
    char *msg = strdup(notificationState.message);
    const size_t count = wrapText(msg, wrapped, maxLines);

    int currentX = 5;
    int currentY = y;
    if (strcmp(notificationState.style, "center") == 0) {
        tft.setTextDatum(TC_DATUM);
        currentX = DISPLAY_CENTER;
        currentY = DISPLAY_CENTER + currentY / 2 - LINES_HEIGHT * count / 2;
    } else {
        tft.setTextDatum(TL_DATUM);
    }

    tft.loadFont(NotoSans_Regular24);
    tft.startWrite();
    for (int i = 0; i < count; i++) {
        const int lineY = currentY + i * LINES_HEIGHT;
        if (strcmp(wrapped[i], "---") == 0) {
            tft.drawFastHLine(0, lineY + LINES_HEIGHT / 4, DISPLAY_SIZE, TFT_DARKGREY);
            currentY = currentY - LINES_HEIGHT / 2;
        } else {
            tft.drawString(wrapped[i], currentX, lineY);
        }
    }
    tft.endWrite();
    tft.unloadFont();
    free(msg);
}

void themeRenderNotification(const bool forceClear) {
    if (!forceClear) return;

    if (notificationState.message[0] == '\0') {
        showMessage(F("No messages"), 5);
        return;
    }

    tft.fillScreen(TFT_BLACK);

    const bool hasSubject = notificationState.subject[0] != '\0';
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        drawSubject(notificationState.subject);
        currentY = LINES_HEIGHT + LINES_HEIGHT / 2;
    }

    // Draw message
    if (strcmp(notificationState.style, "big_num") == 0) {
        showNumber(notificationState.message, DISPLAY_CENTER, DISPLAY_CENTER + currentY / 2);
    } else {
        showNotification(hasSubject ? MAX_LINES - 2 : MAX_LINES, currentY);
    }

    // Draw subject line
    if (hasSubject) animateHLine();
}
