#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"

constexpr int DISPLAY_IP_BUFFER_SIZE       = 24;
constexpr int DISPLAY_IMG_PATH_BUFFER_SIZE = 32;

struct DisplayState {
    Theme  theme   = Theme::NONE;
    time_t timeout = 0;
    char   ipInfo[DISPLAY_IP_BUFFER_SIZE]      = {};
    char   image[DISPLAY_IMG_PATH_BUFFER_SIZE] = {};
};

void displayInit();
void displaySetBrightness(int brightness);
void displayTest();
void displayUpdate(Theme theme = Theme::NONE, bool forceClear = true);
void displayScheduleUpdate(Theme theme = Theme::NONE, bool forceClear = true, time_t timeout = 0);
void displayScheduleTest();
bool displayProcessPending();
void displayCycleNextPage();
void displayToggleBacklight();

extern DisplayState displayState;
extern TFT_eSPI     tft;

#endif
