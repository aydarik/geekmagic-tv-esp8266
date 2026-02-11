#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Define buffer sizes for DisplayState char arrays
#define DISPLAY_IP_BUFFER_SIZE 24
#define DISPLAY_IMG_PATH_BUFFER_SIZE 64
#define DISPLAY_MSG_BUFFER_SIZE 512

struct DisplayState {
    int theme; // 0 - AP mode, 1 - clock, 2 - message, 3 - image
    char ipInfo[DISPLAY_IP_BUFFER_SIZE]; // IP address or network info to show at top
    char image[DISPLAY_IMG_PATH_BUFFER_SIZE]; // Image path
    char message[DISPLAY_MSG_BUFFER_SIZE]; // Custom message to display
};

void displayInit();

void displaySetBrightness(int brightness);

void displayTest();

void displayUpdate(bool forceClear = true);

void displayShowMessage(const String &msg);

void displayShowAPScreen(const char *ssid, const char *password, const char *ip);

void displayCycleNextPage();

void displayToggleBacklight();

extern DisplayState displayState;
extern TFT_eSPI tft;

#endif
