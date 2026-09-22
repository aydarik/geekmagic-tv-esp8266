#ifndef THEME_NOTIFICATION_H
#define THEME_NOTIFICATION_H

constexpr int NOTIFICATION_SBJ_BUFFER_SIZE   = 32;
constexpr int NOTIFICATION_MSG_BUFFER_SIZE   = 256;
constexpr int NOTIFICATION_STYLE_BUFFER_SIZE = 8;
constexpr int GAUGE_UNIT_BUFFER_SIZE         = 16;

struct NotificationState {
    char subject[NOTIFICATION_SBJ_BUFFER_SIZE];
    char message[NOTIFICATION_MSG_BUFFER_SIZE];
    char style[NOTIFICATION_STYLE_BUFFER_SIZE];
};

struct GaugeState {
    float current;
    float max;
    char  unit[GAUGE_UNIT_BUFFER_SIZE];
};

void themeRenderNotification(bool forceClear);

#endif // THEME_NOTIFICATION_H
