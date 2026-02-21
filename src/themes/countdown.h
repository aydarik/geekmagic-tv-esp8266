#ifndef THEME_COUNTDOWN_H
#define THEME_COUNTDOWN_H

#include <ctime>

#define COUNTDOWN_GAUGE_OFFSET 60

#define COUNTDOWN_SBJ_BUFFER_SIZE 32
#define COUNTDOWN_DATETIME_BUFFER_SIZE 32

struct CountdownState {
    char subject[COUNTDOWN_SBJ_BUFFER_SIZE];
    char datetime[COUNTDOWN_DATETIME_BUFFER_SIZE];
};

void themeRenderCountdown(bool forceClear, const time_t &now);

#endif
