#include "countdown.h"
#include "config.h"
#include "display.h"
#include "fonts/Roboto_Regular24.h"

CountdownState countdownState;

// Parse "YYYY-MM-DD HH:MM:SS"
time_t parseDateTime(const String &s) {
    // Expected lengths:
    // 16 -> YYYY-MM-DD HH:MM
    // 19 -> YYYY-MM-DD HH:MM:SS
    if (s.length() != 16 && s.length() != 19) {
        return 0;
    }

    const char *str = s.c_str();

    // Validate fixed characters
    if (str[4] != '-' || str[7] != '-' ||
        (str[10] != ' ' && str[10] != 'T') ||
        str[13] != ':') {
        return 0;
    }

    if (s.length() == 19 && str[16] != ':') {
        return 0;
    }

    auto toInt2 = [](char a, char b) -> int {
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

    int year = toInt4(str);
    int month = toInt2(str[5], str[6]);
    int day = toInt2(str[8], str[9]);
    int hour = toInt2(str[11], str[12]);
    int min = toInt2(str[14], str[15]);
    int sec = 0;

    if (s.length() == 19) {
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

void themeRenderCountdown(const bool forceClear) {
    if (countdownState.datetime[0] == '\0') {
        if (forceClear) {
            displayShowMessage(F("No date-time"));
        }
        return;
    }

    const time_t targetTime = parseDateTime(countdownState.datetime);
    if (targetTime == 0) {
        if (forceClear) {
            displayShowMessage(F("Not valid\ndate-time string"));
        }
        return;
    }

    const time_t now = time(nullptr);
    long diff = targetTime - now;
    const bool passed = diff < 0;
    if (passed) {
        diff *= -1;
    }

    const int minutes = diff / 60;
    const int seconds = diff % 60;

    if (forceClear) {
        tft.fillScreen(TFT_BLACK);
    }

    const bool hasSubject = countdownState.subject[0] != '\0';
    const int centerY = tft.height() / 2;
    const int centerX = tft.width() / 2;
    int currentY = 0;

    // Draw subject
    if (hasSubject) {
        if (forceClear) {
            tft.setTextDatum(TC_DATUM);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
            tft.loadFont(Roboto_Regular24);
            tft.drawString(String(countdownState.subject), centerX, currentY, FONT_DEFAULT);
            tft.unloadFont();
        }
        currentY = 44;
    }

    // Draw countdown
    char buffer[8];
    if (passed) {
        sprintf(buffer, "-%d:%02d", minutes, seconds);
    } else {
        sprintf(buffer, "%d:%02d", minutes, seconds);
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    int fontMultiplier = 1;
    if (!passed && minutes < 10) {
        fontMultiplier = 2;
        tft.setTextSize(fontMultiplier);
    }

    const int timerY = centerY + currentY / 2;
    if (!forceClear && ((!passed && seconds == 59) || (passed && seconds == 0))) {
        tft.fillRect(0, timerY - 30 * fontMultiplier, tft.width(), 60 * fontMultiplier, TFT_BLACK);
    }
    tft.drawString(buffer, centerX, timerY, FONT_DIGIT);

    tft.setTextSize(1);

    // Draw subject line
    if (hasSubject && forceClear) {
        currentY = 32;
        for (int dx = 0; dx <= centerX; dx += 8) {
            tft.drawFastHLine(centerX - dx, currentY, dx * 2, TFT_SILVER);
            delay(20); // control animation speed
        }
    }
}
