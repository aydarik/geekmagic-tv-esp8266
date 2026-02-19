#ifndef UTILS_H
#define UTILS_H

#include <cstddef>

#define MAX_LINES 7
#define MAX_LINE_CHARS 16
#define LINES_OFFSET 32

size_t wrapText(char *text, char *lines[], size_t maxLines);

size_t splitString(const String &s, String lines[], size_t maxLines);

#endif
