#ifndef THEME_CLOCK_H
#define THEME_CLOCK_H

#define CLOCK_NOTE_SIZE 32

struct ClockState {
    char note[CLOCK_NOTE_SIZE];
    time_t noteTimeout;
};

void themeRenderClock(bool forceClear);

#endif
