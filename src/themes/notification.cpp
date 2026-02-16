#include "notification.h"
#include "config.h"
#include "display.h"

NotificationState notificationState;

// Helper function to wrap text
size_t wrapText(char *text, char *lines[], const size_t maxLines) {
    if (!text || *text == '\0')
        return 0;

    const int maxWidth = tft.width();
    const int spaceWidth = tft.textWidth(" ");

    size_t count = 0;
    char *lineStart = text;
    int currentWidth = 0;
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
        char saved = *p;
        *p = '\0';

        int wordWidth = tft.textWidth(wordStart);

        if (currentWidth == 0) {
            currentWidth = wordWidth;
        } else if (currentWidth + spaceWidth + wordWidth <= maxWidth) {
            currentWidth += spaceWidth + wordWidth;
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

    const int font = FONT_DEFAULT;
    const bool hasSubject = notificationState.subject[0] != '\0';
    const int centerY = tft.height() / 2;
    const int centerX = tft.width() / 2;
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(notificationState.subject, centerX, currentY, font);
        currentY = 45;
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
        int currentX = 0;

        // Calculate line height based on the font
        tft.setTextFont(font); // Set font for height calculation
        const int linesOffset = tft.fontHeight() + 8;

        char *wrapped[MAX_LINES];
        char *msg = strdup(notificationState.message);
        const size_t count = wrapText(msg, wrapped, MAX_LINES);

        // Draw each wrapped line
        if (strcmp(notificationState.style, "center") == 0) {
            tft.setTextDatum(TC_DATUM);
            currentX = centerX;
            currentY = centerY + currentY / 2 - linesOffset * count / 2;
        } else {
            tft.setTextDatum(TL_DATUM);
        }

        tft.startWrite();
        for (unsigned int i = 0; i < count; i++) {
            tft.drawString(wrapped[i], currentX, currentY + i * linesOffset);
        }
        tft.endWrite();
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
