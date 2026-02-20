#include <WString.h>
#include "config.h"
#include "display.h"
#include "utils.h"
#include "fonts/Roboto_Regular24.h"

int utf8Length(const char *text) {
    int count = 0;
    while (*text) {
        // Count only bytes that are NOT continuation bytes (10xxxxxx)
        if ((*text & 0xC0) != 0x80) count++;
        text++;
    }
    return count;
}

// Helper function to wrap text
size_t wrapText(char *text, char *lines[], const size_t maxLines) {
    if (!text || *text == '\0') return 0;

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
        } else if (currentWidth + wordWidth + 1 <= MAX_LINE_CHARS) {
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

// Helper function to split text
size_t splitString(const String &s, String lines[], const size_t maxLines) {
    if (s.length() == 0) return 0;

    size_t count = 0;
    int start = 0;
    while (count < maxLines) {
        const int newlineIndex = s.indexOf('\n', start);
        if (newlineIndex == -1) {
            lines[count++] = s.substring(start); // last line
            break;
        }
        lines[count++] = s.substring(start, newlineIndex);
        start = newlineIndex + 1;
    }

    return count;
}

void animateHLine(const int y) {
    const int32_t centerX = tft.width() / 2;
    const int32_t offset = centerX / ANIMATION_STEPS;

    tft.startWrite();
    for (int i = 1; i <= ANIMATION_STEPS; ++i) {
        tft.drawFastHLine(centerX - i * offset, y, i * offset * 2, TFT_SILVER);
        delay(ANIMATION_STEP_DELAY);
    }
    tft.endWrite();
}

void drawSubject(const char *text) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.loadFont(Roboto_Regular24);
    tft.drawString(text, tft.width() / 2, 0, FONT_DEFAULT);
    tft.unloadFont();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

// Parse "YYYY-MM-DD HH:MM:SS"
time_t parseDateTime(const String &s) {
    const char *str = s.c_str();

    // Validate fixed characters
    if (str[4] != '-' || str[7] != '-' ||
        (str[10] != ' ' && str[10] != 'T') ||
        str[13] != ':') {
        return 0;
    }

    if (s.length() > 16 && str[16] != ':') {
        return 0;
    }

    auto toInt2 = [](const char a, const char b) -> int {
        if (!isdigit(a) || !isdigit(b)) return -1;
        return (a - '0') * 10 + (b - '0');
    };

    auto toInt4 = [](const char *p) -> int {
        for (int i = 0; i < 4; i++) {
            if (!isdigit(p[i])) return -1;
        }
        return (p[0] - '0') * 1000 +
               (p[1] - '0') * 100 +
               (p[2] - '0') * 10 +
               (p[3] - '0');
    };

    const int year = toInt4(str);
    const int month = toInt2(str[5], str[6]);
    const int day = toInt2(str[8], str[9]);
    const int hour = toInt2(str[11], str[12]);
    const int min = toInt2(str[14], str[15]);

    int sec = 0;
    if (s.length() > 16) {
        sec = toInt2(str[17], str[18]);
    }

    // Basic validation
    if (year < 1970 || month < 1 || month > 12 ||
        day < 1 || day > 31 ||
        hour < 0 || hour > 23 ||
        min < 0 || min > 59 ||
        sec < 0 || sec > 59) {
        return 0;
    }

    tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    return mktime(&tm);
}

void showMessage(const String &msg, const int timeout, const int offsetY) {
    displayState.theme = 0;
    tft.fillScreen(TFT_BLACK);

    tft.setTextFont(FONT_DEFAULT);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    String wrapped[MAX_LINES];
    const size_t count = splitString(msg, wrapped, MAX_LINES);

    constexpr int lineHeight = 34;
    const int centerX = tft.width() / 2;
    int currentY = tft.height() / 2 - count * lineHeight / 2 + offsetY;
    for (unsigned int i = 0; i <= count; i++) {
        tft.drawString(wrapped[i], centerX, currentY);
        currentY += lineHeight;
    }

    if (timeout > 0) {
        displayState.timeout = time(nullptr) + timeout;
    }
}
