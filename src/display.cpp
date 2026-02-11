#include "display.h"
#include "config.h"
#include "logger.h"
#include "settings.h"
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <ESP8266WiFi.h>
#include <vector>
#include <ctime> // For time and date functions

// Font size definitions for clarity
#define FONT_INFO 1      // Small font for IP addresses, etc.
#define FONT_MESSAGE 2   // Font for messages and labels
#define FONT_DEFAULT 4   // Default font size for various things
#define FONT_TIME 7      // Large font for the main clock time (7-segment, digits only)

TFT_eSPI tft = TFT_eSPI();

DisplayState displayState;

extern Settings appSettings;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void getFormattedTime(char *buffer, size_t bufferSize) {
    time_t now;
    tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // H:M\0
    strftime(buffer, bufferSize, "%H:%M", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void getFormattedDate(char *buffer, size_t bufferSize) {
    time_t now;
    tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // d-m-Y\0
    strftime(buffer, bufferSize, "%d-%m-%Y", &timeinfo);
    buffer[bufferSize - 1] = '\0'; // Ensure null-termination
}

void displayInit() {
    tft.init();
    tft.setRotation(0);
    tft.invertDisplay(true); // Match ESPHome invert_colors: true

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    displayState.theme = 1;
    displayState.ipInfo[0] = '\0';
    displayState.image[0] = '\0';
    displayState.message[0] = '\0';

    // Backlight on (inverted PWM: low value = bright)
    pinMode(PIN_BACKLIGHT, OUTPUT);
    analogWriteFreq(1000); // Zet PWM frequency
    analogWriteRange(1023); // 10bit

    logPrint(F("Display init complete"));
}

void displaySetBrightness(int brightness) {
    brightness = constrain(brightness, 0, 100);

    // ESP8266 PWM range is 0-1023
    // Hardware is inverted: LOW = bright, HIGH = off
    int pwmValue;

    if (brightness == 0) {
        pwmValue = 1023; // Off
    } else if (brightness == 100) {
        pwmValue = 0; // Full bright
    } else {
        // Map 1-99 to 1023-0 (inverted)
        pwmValue = map(brightness, 0, 100, 1023, 0);
    }

    analogWrite(PIN_BACKLIGHT, pwmValue);
    logPrintf("Brightness: %d%%", brightness);
}

int getWiFiSignalPercent() {
    const long rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

void displayTest() {
    logPrint(F("Testing colors..."));
    tft.fillScreen(TFT_RED);
    delay(500);
    tft.fillScreen(TFT_GREEN);
    delay(500);
    tft.fillScreen(TFT_BLUE);
    delay(500);
    tft.fillScreen(TFT_WHITE);
    delay(500);
    tft.fillScreen(TFT_BLACK);

    logPrint(F("Testing text..."));
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("HELLO WORLD!", 120, 120, FONT_DEFAULT);
    delay(2000);

    displayUpdate();
}

void displayRenderClock() {
    int currentX = tft.width() / 2;
    int currentY = 10; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (appSettings.showIP) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        constexpr int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        const int ipLineHeight = tft.fontHeight();
        tft.drawString(String(displayState.ipInfo), currentX, currentY, ipFont);
        currentY += ipLineHeight; // Add spacing
    }

    // Draw time
    currentY += 50;
    tft.setTextDatum(TC_DATUM);
    constexpr int timeFont = FONT_TIME;
    tft.setTextFont(timeFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char currentTime[8];
    getFormattedTime(currentTime, sizeof(currentTime));
    tft.drawString(String(currentTime), currentX, currentY, timeFont);

    // Draw date
    currentY += 80;
    constexpr int dateFont = FONT_DEFAULT;
    tft.setTextFont(dateFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate));
    tft.drawString(String(currentDate), currentX, currentY, dateFont);
}

void displayRenderMessage() {
    displayShowMessage(String(displayState.message));
}

void displayRenderAPMode(const char *ssid, const char *pass) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);

    int currentX = tft.width() / 2;
    int currentY = 10; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        constexpr int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        const int ipLineHeight = tft.fontHeight();
        tft.drawString(String(displayState.ipInfo), currentX, currentY, ipFont);
        currentY += ipLineHeight;
    }

    constexpr int headerFont = FONT_DEFAULT;
    constexpr int labelFont = FONT_MESSAGE;
    constexpr int valueFont = FONT_DEFAULT;

    // Draw "AP Mode" header
    currentY += 40;
    tft.setTextFont(headerFont);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("AP Mode", currentX, currentY, headerFont);

    // Draw SSID label
    currentY += 40;
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("SSID:", currentX, currentY, labelFont);

    // Draw SSID value
    currentY += 25;
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(ssid, currentX, currentY, valueFont);

    // Draw Password label
    currentY += 40;
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Password:", currentX, currentY, labelFont);

    // Draw Password value
    currentY += 25;
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(pass, currentX, currentY, valueFont);
}

void displayRenderImage() {
    const char *path = displayState.image;

    if (!LittleFS.exists(path)) {
        displayShowMessage(F("Image not found"));
        return;
    }

    File jpgFile = LittleFS.open(path, "r");
    if (!jpgFile) {
        const String errorMsg = String(F("Failed to open image file: ")) + path;
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
        return;
    }
    logPrint(String(F("INFO: Image file opened: ")) + path);

    // Direct image swap without clearing screen for smooth transitions
    // The new JPEG will overwrite the previous image directly
    // Keep CS low during entire transfer to reduce overhead and speed up rendering
    tft.startWrite();
    const JRESULT res = TJpgDec.drawFsJpg(0, 0, jpgFile);
    tft.endWrite();
    if (res != JDR_OK) {
        const String errorMsg = String(F("JPEG Decode Failed\nCode: ")) + String(res);
        displayShowMessage(errorMsg);
        logPrint(errorMsg);
    }

    jpgFile.close(); // Close the file after decoding attempt
}

void displayUpdate(bool forceClear) {
    if (displayState.theme == 1) {
        if (forceClear) {
            tft.fillScreen(TFT_BLACK);
        }
        displayRenderClock();
    } else if (displayState.theme == 2) {
        displayRenderMessage();
    } else if (displayState.theme == 3) {
        displayRenderImage();
    }
}

void displayShowMessage(const String &msg) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    int startY = tft.height() / 2; // Starting Y position, will be adjusted
    constexpr int font = FONT_DEFAULT;

    // Calculate line height based on the font
    tft.setTextFont(font); // Set font for height calculation
    const int lineHeight = tft.fontHeight();

    // Split message by newline characters first
    std::vector<String> linesToProcess;
    int prev = 0;
    for (size_t i = 0; i < msg.length(); i++) {
        if (msg.charAt(i) == '\n') {
            linesToProcess.push_back(msg.substring(prev, i));
            prev = i + 1;
        }
    }
    linesToProcess.push_back(msg.substring(prev)); // Add the last part

    constexpr int linesOffset = 8;

    // Adjust startY to vertically center the block of text
    const size_t linesCnt = linesToProcess.size();
    startY -= linesCnt * (lineHeight + linesOffset) / 2;

    // Draw each wrapped line
    tft.startWrite();
    for (size_t i = 0; i < linesCnt; i++) {
        tft.drawString(linesToProcess[i], tft.width() / 2, startY + i * (lineHeight + linesOffset), font);
    }
    tft.endWrite();
}

void displayShowAPScreen(const char *ssid, const char *password, const char *ip) {
    logPrint(F("Switching to AP screen"));
    displayState.theme = 0;

    strncpy(displayState.ipInfo, ip, sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0'; // Ensure null-termination

    displayRenderAPMode(ssid, password);
}

void displayCycleNextPage() {
    // Cycle: Clock -> Image (if available) -> Clock
    if (displayState.theme == 1) {
        // Currently showing clock, try to switch to image if available
        if (displayState.image[0] != '\0' && LittleFS.exists(displayState.image)) {
            logPrint(F("Cycling to image page"));
            displayState.theme = 3;
        } else {
            logPrint(F("No image available, staying on clock page"));
        }
    } else {
        // Currently showing image, switch back to clock
        logPrint(F("Cycling to clock page"));
        displayState.theme = 1;
    }

    // Immediately update the display
    displayUpdate();
}

// Track backlight state for toggle functionality
static bool backlightOn = true;

void displayToggleBacklight() {
    if (backlightOn) {
        // Turn off backlight
        logPrint(F("Backlight OFF"));
        displaySetBrightness(0);
        backlightOn = false;
    } else {
        // Turn on backlight
        logPrint(F("Backlight ON"));
        displaySetBrightness(appSettings.brightness);
        backlightOn = true;
    }
}
