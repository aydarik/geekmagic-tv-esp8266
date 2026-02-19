#include "notification.h"
#include "config.h"
#include "display.h"
#include "utils.h"
#include "fonts/Roboto_Regular24.h"

NotificationState notificationState;

void themeRenderNotification(const bool forceClear) {
    if (!forceClear) {
        return;
    }

    if (notificationState.message[0] == '\0') {
        showMessage(F("No messages"), 5);
        return;
    }

    tft.fillScreen(TFT_BLACK);

    const bool hasSubject = notificationState.subject[0] != '\0';
    const int centerY = tft.height() / 2;
    const int centerX = tft.width() / 2;
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        drawSubject(notificationState.subject);
        currentY = 44;
    }

    // Draw message
    if (strcmp(notificationState.style, "big_num") == 0) {
        tft.setTextDatum(MC_DATUM);
        if (strlen(notificationState.message) < 5) {
            tft.setTextSize(2);
        }
        tft.drawString(notificationState.message, centerX, centerY + currentY / 2, FONT_DIGIT);
        tft.setTextSize(1);
    } else {
        char *wrapped[MAX_LINES];
        char *msg = strdup(notificationState.message);
        const size_t count = wrapText(msg, wrapped, MAX_LINES);

        int currentX = 5;
        if (strcmp(notificationState.style, "center") == 0) {
            tft.setTextDatum(TC_DATUM);
            currentX = centerX;
            currentY = centerY + currentY / 2 - LINES_OFFSET * count / 2;
        } else {
            if (!hasSubject) {
                currentY = 5;
            }
            tft.setTextDatum(TL_DATUM);
        }

        tft.loadFont(Roboto_Regular24);
        tft.startWrite();
        for (int i = 0; i < count; i++) {
            if (strcmp(wrapped[i], "---") == 0) {
                const int lineY = currentY + i * LINES_OFFSET + LINES_OFFSET / 3;
                tft.drawFastHLine(0, lineY, tft.width(), TFT_DARKGREY);
            } else {
                tft.drawString(wrapped[i], currentX, currentY + i * LINES_OFFSET);
            }
        }
        tft.endWrite();
        tft.unloadFont();
        free(msg);
    }

    // Draw subject line
    if (hasSubject) {
        drawHLine(32);
    }
}
