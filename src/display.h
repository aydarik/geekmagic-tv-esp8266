#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define DISPLAY_IP_BUFFER_SIZE 24
#define DISPLAY_IMG_PATH_BUFFER_SIZE 32
#define DISPLAY_MSG_BUFFER_SIZE 512

struct DisplayState {
    int theme; // -1 - AP mode, 1 - clock, 2 - message, 3 - image, 4 - countdown
    time_t timeout;
    char ipInfo[DISPLAY_IP_BUFFER_SIZE]; // IP address or network info to show at top
    char image[DISPLAY_IMG_PATH_BUFFER_SIZE]; // Image path
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
