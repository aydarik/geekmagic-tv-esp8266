#ifndef UTILS_H
#define UTILS_H

#include <cstddef>

#define MAX_LINES 7
#define MAX_LINE_CHARS 19
#define LINES_HEIGHT 34

size_t wrapText(char *text, char *lines[], size_t maxLines);

size_t splitString(const String &s, String lines[], size_t maxLines);

void animateHLine(int y = LINES_HEIGHT + LINES_HEIGHT / 4 - 4);

void drawSubject(const char *text);

time_t parseDateTime(const String &s);

void showMessage(const String &msg, int timeout = 0, int offsetY = 0);

#endif
