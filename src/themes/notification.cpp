#include "notification.h"
#include "config.h"
#include "display.h"
#include "fonts/Roboto_Regular24.h"

NotificationState notificationState;

int utf8Length(const char *text) {
    int count = 0;
    while (*text) {
        // Count only bytes that are NOT continuation bytes (10xxxxxx)
        if ((*text & 0xC0) != 0x80) {
            count++;
        }
        text++;
    }
    return count;
}

// Helper function to wrap text
size_t wrapText(char *text, char *lines[], const size_t maxLines, const size_t maxSize) {
    if (!text || *text == '\0')
        return 0;

    size_t count = 0;
    char *lineStart = text;
    size_t currentWidth = 0;
    char *p = text;

    while (*p && count < maxLines) {
        // Handle newline
        if (*p == '\n') {
            *p = '\0';
            lines[count++] = lineStart;
            lineStart = p + 1;
            currentWidth = 0;
            p++;
            continue;
        }

        // Find next word
        char *wordStart = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        const char saved = *p;
        *p = '\0';

        const size_t wordWidth = utf8Length(wordStart);
        if (currentWidth == 0) {
            currentWidth = wordWidth;
        } else if (currentWidth + wordWidth + 1 <= maxSize) {
            currentWidth += wordWidth + 1;
        } else {
            // Wrap line BEFORE current word
            *(wordStart - 1) = '\0'; // Terminate previous line
            lines[count++] = lineStart;
            lineStart = wordStart;
            currentWidth = wordWidth;
        }

        *p = saved;

        // Move past space
        if (*p == ' ') p++;
    }

    // Add last line
    if (*lineStart && count < maxLines)
        lines[count++] = lineStart;

    return count;
}

void themeRenderNotification() {
    if (notificationState.message[0] == '\0') {
        displayShowMessage(F("No messages"));
        return;
    }

    tft.fillScreen(TFT_BLACK);

    const bool hasSubject = notificationState.subject[0] != '\0';
    const int centerY = tft.height() / 2;
    const int centerX = tft.width() / 2;
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.loadFont(Roboto_Regular24);
        tft.drawString(notificationState.subject, centerX, currentY);
        tft.unloadFont();
        currentY = 44;
    }

    // Draw message
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (strcmp(notificationState.style, "big_num") == 0) {
        tft.setTextDatum(MC_DATUM);
        if (strlen(notificationState.message) < 5) {
            tft.setTextSize(2);
        }
        tft.drawString(notificationState.message, centerX, centerY + currentY / 2, FONT_DIGIT);
        tft.setTextSize(1);
    } else {
        const size_t maxLines = MAX_LINES - (hasSubject ? 1 : 0);
        char *wrapped[maxLines];
        char *msg = strdup(notificationState.message);
        const size_t count = wrapText(msg, wrapped, maxLines, MAX_LINE_CHARS);

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
        for (unsigned int i = 0; i < count; i++) {
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
        currentY = 32;
        for (int dx = 0; dx <= centerX; dx += 8) {
            tft.drawFastHLine(centerX - dx, currentY, dx * 2, TFT_SILVER);
            delay(20); // control animation speed
        }
    }
}
