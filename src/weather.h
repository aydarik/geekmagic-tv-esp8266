#ifndef THEMES_WEATHER_H
#define THEMES_WEATHER_H

#define WEATHER_UPDATE_INTERVAL 900000UL

#include <Arduino.h>

void renderWeather(int32_t y);
void weatherUpdateTask(unsigned long now);

#endif
