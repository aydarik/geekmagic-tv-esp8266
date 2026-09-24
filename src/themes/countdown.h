#ifndef THEME_COUNTDOWN_H
#define THEME_COUNTDOWN_H

#include <ctime>

constexpr int COUNTDOWN_GAUGE_OFFSET         = 60;
constexpr int COUNTDOWN_SBJ_BUFFER_SIZE      = 32;
constexpr int COUNTDOWN_DATETIME_BUFFER_SIZE = 32;

struct CountdownState {
    char subject[COUNTDOWN_SBJ_BUFFER_SIZE];
    char datetime[COUNTDOWN_DATETIME_BUFFER_SIZE];
};

void themeRenderCountdown(bool forceClear, const time_t &now);

#endif
