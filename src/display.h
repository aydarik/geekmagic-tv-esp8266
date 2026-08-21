#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define DISPLAY_SIZE 240
#define DISPLAY_CENTER 120

#define DISPLAY_IP_BUFFER_SIZE 24
#define DISPLAY_IMG_PATH_BUFFER_SIZE 32
#define DISPLAY_MSG_BUFFER_SIZE 512

struct DisplayState {
    int theme;
    time_t timeout;
    char ipInfo[DISPLAY_IP_BUFFER_SIZE];
    char image[DISPLAY_IMG_PATH_BUFFER_SIZE];
};

void displayInit();

void displaySetBrightness(int brightness);

void displayTest();

void displayUpdate(int theme = 0, bool forceClear = true);

void displayCycleNextPage();

void displayToggleBacklight();

extern DisplayState displayState;
extern TFT_eSPI tft;

#endif
