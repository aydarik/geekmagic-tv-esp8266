#include <WString.h>
#include "utils.h"

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
size_t wrapText(char *text, char *lines[], const size_t maxLines) {
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
    if (s.length() == 0) {
        return 0;
    }

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
