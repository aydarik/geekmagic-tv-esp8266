#ifndef THEME_CLOCK_H
#define THEME_CLOCK_H

#define CLOCK_NOTE_SIZE 128

struct ClockState {
    char note[CLOCK_NOTE_SIZE];
    time_t noteTimeout;
    int noteRotations;
};

void themeRenderClock(bool forceClear, const time_t &now);

#endif
