#ifndef UTILS_H
#define UTILS_H

#include <cstddef>

#define MAX_LINES 7
#define MAX_LINE_CHARS 19
#define LINES_OFFSET 32

size_t wrapText(char *text, char *lines[], size_t maxLines);

size_t splitString(const String &s, String lines[], size_t maxLines);

void animateHLine(int y);

void drawSubject(const char *text);

time_t parseDateTime(const String &s);

void showMessage(const String &msg, int timeout = 0, int offsetY = 0);

#endif
