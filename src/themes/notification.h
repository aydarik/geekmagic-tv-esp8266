#ifndef THEME_NOTIFICATION_H
#define THEME_NOTIFICATION_H

#define NOTIFICATION_SBJ_BUFFER_SIZE 32
#define NOTIFICATION_MSG_BUFFER_SIZE 256
#define NOTIFICATION_STYLE_BUFFER_SIZE 8

#define GAUGE_UNIT_BUFFER_SIZE 16

struct NotificationState {
    char subject[NOTIFICATION_SBJ_BUFFER_SIZE];
    char message[NOTIFICATION_MSG_BUFFER_SIZE];
    char style[NOTIFICATION_STYLE_BUFFER_SIZE];
};

struct GaugeState {
    float current;
    float max;
    char unit[GAUGE_UNIT_BUFFER_SIZE];
};


void themeRenderNotification(bool forceClear);

#endif
