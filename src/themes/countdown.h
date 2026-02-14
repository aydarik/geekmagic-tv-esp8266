#ifndef THEME_COUNTDOWN_H
#define THEME_COUNTDOWN_H

#define COUNTDOWN_SBJ_BUFFER_SIZE 32
#define COUNTDOWN_DATETIME_BUFFER_SIZE 32

struct CountdownState {
    char subject[COUNTDOWN_SBJ_BUFFER_SIZE];
    char datetime[COUNTDOWN_DATETIME_BUFFER_SIZE];
};

void themeRenderCountdown(bool forceClear);

#endif
