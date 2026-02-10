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
#define FONT_TIME 6      // Large font for the main clock time (7-segment, digits only)

TFT_eSPI tft = TFT_eSPI();
DisplayState displayState;
int scrollPos = 240;

extern Settings appSettings;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

// Helper function to wrap text
std::vector<String> wrapText(const String &text, const int font, const int maxWidth) {
    std::vector<String> lines;
    if (text.isEmpty()) {
        lines.emplace_back("");
        return lines;
    }

    // Set the font for text width calculations
    tft.setTextFont(font);

    String currentLine = "";
    String word = "";
    // Temporarily increase buffer size to handle longer lines during word accumulation
    currentLine.reserve(text.length() + 10);
    word.reserve(text.length() + 10);

    for (size_t i = 0; i < text.length(); i++) {
        // Check if adding the word exceeds maxWidth
        if (char c = text.charAt(i); c == ' ') {
            if (tft.textWidth(currentLine + word) > maxWidth) {
                // Use textWidth with current font set
                // If current line is not empty, add it and start a new line with the word
                if (!currentLine.isEmpty()) {
                    lines.push_back(currentLine);
                    currentLine = word; // Start new line with the current word
                } else {
                    // Word itself is longer than maxWidth, force break within word if needed
                    // For simplicity, for now just add the too-long word on its own line
                    lines.push_back(word);
                    currentLine = "";
                }
            } else {
                currentLine += word;
            }
            currentLine += " "; // Add space after word
            word = "";
        } else {
            word += c;
        }
    }

    // Add the last word/part of word
    if (!word.isEmpty()) {
        if (tft.textWidth(currentLine + word) > maxWidth) {
            // Use textWidth with current font set
            if (!currentLine.isEmpty()) {
                lines.push_back(currentLine);
            }
            lines.push_back(word);
        } else {
            currentLine += word;
        }
    }

    // Add the last line if not empty
    if (!currentLine.isEmpty()) {
        lines.push_back(currentLine);
    }

    return lines;
}

void getFormattedDate(char *buffer, size_t bufferSize) {
    time_t now;
    tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // DD-MM-YYYY\0
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

    displayState.line1[0] = '\0';
    displayState.line2[0] = '\0';
    displayState.ipInfo[0] = '\0';
    displayState.apMode = false;
    displayState.apSSID[0] = '\0';
    displayState.apPassword[0] = '\0';

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
    logPrintf("Brightness: %d%%, PWM: %d", brightness, pwmValue);
}

int getWiFiSignalPercent() {
    const long rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
}

void displayTest() {
    appSettings.theme = 2;

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
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("HELLO WORLD!", 120, 120, FONT_DEFAULT);
    delay(2000);

    appSettings.theme = 1;
}

void displayUpdate() {
    if (appSettings.theme == 3 && appSettings.lastImage[0] != '\0') {
        logPrint(String(F("Rendering image: ")) + appSettings.lastImage);
        displayRenderImage(appSettings.lastImage);
    } else if (appSettings.theme == 2) {
        logPrintf("Rendering message screen: %s", displayState.line2);
        displayRenderMessage();
    } else if (displayState.apMode) {
        logPrint(F("Rendering AP mode screen"));
        displayRenderAPMode();
    } else {
        logPrint(F("Rendering clock"));
        displayRenderClock();
    }
}

void displayRenderClock() {
    tft.fillScreen(TFT_BLACK);

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font) - only if changed
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    constexpr int ipFont = FONT_INFO; // Small font
    tft.setTextFont(ipFont);
    const int ipLineHeight = tft.fontHeight();
    const std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, tft.width() - 10);

    tft.startWrite();
    for (size_t i = 0; i < ipWrappedLines.size(); i++) {
        tft.drawString(ipWrappedLines[i], tft.width() / 2, i * ipLineHeight + currentY, ipFont);
    }
    tft.endWrite();
    currentY += ipWrappedLines.size() * ipLineHeight + 10; // Add spacing after IP

    // Display Time (displayState.line1) - Centered
    tft.setTextDatum(TC_DATUM);
    constexpr int timeFont = FONT_TIME;
    tft.setTextFont(timeFont);
    const int timeLineHeight = tft.fontHeight();
    const std::vector<String> timeWrappedLines = wrapText(String(displayState.line1), timeFont, tft.width());

    // Calculate remaining space and center time vertically in it
    const int remainingHeight = tft.height() - currentY;
    const int totalTextHeight = timeWrappedLines.size() * timeLineHeight;

    // Add date height to calculation
    constexpr int dateFont = FONT_DEFAULT;
    tft.setTextFont(dateFont);
    int dateLineHeight = tft.fontHeight();
    char currentDate[16];
    getFormattedDate(currentDate, sizeof(currentDate));
    const std::vector<String> dateWrappedLines = wrapText(String(currentDate), dateFont, tft.width());
    const int totalDateHeight = dateWrappedLines.size() * dateLineHeight;

    // Center the time+date block in remaining space
    const int timeBlockHeight = timeLineHeight / 2 + totalTextHeight + totalDateHeight;
    const int timeStartY = currentY + (remainingHeight - timeBlockHeight) / 2;
    currentY = timeStartY;

    // Draw time
    tft.setTextFont(timeFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Text color, background color

    // Calculate exact text width to minimize clearing area
    int maxTextWidth = 0;
    for (const auto &timeWrappedLine: timeWrappedLines) {
        if (const int textWidth = tft.textWidth(timeWrappedLine); textWidth > maxTextWidth)
            maxTextWidth = textWidth;
    }

    tft.startWrite();
    for (size_t i = 0; i < timeWrappedLines.size(); i++) {
        tft.drawString(timeWrappedLines[i], tft.width() / 2, i * timeLineHeight + currentY, timeFont);
    }
    tft.endWrite();
    currentY += totalTextHeight; // Move Y past the time block

    // Add some padding between time and date
    currentY += timeLineHeight / 2; // Roughly half a line height padding

    // Display Date (getFormattedDate()) - Centered, below time
    tft.setTextFont(dateFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); // Text with background color

    tft.startWrite();
    for (size_t i = 0; i < dateWrappedLines.size(); i++) {
        tft.drawString(dateWrappedLines[i], tft.width() / 2, i * dateLineHeight + currentY, dateFont);
    }
    tft.endWrite();
    // currentY += totalDateHeight; // Move Y past the date block
}

void displayRenderMessage() {
    displayShowMessage(String(displayState.line2));
}

void displayRenderAPMode() {
    tft.fillScreen(TFT_BLACK);

    int currentY = 5; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        constexpr int ipFont = FONT_INFO; // Small font
        tft.setTextFont(ipFont);
        const int ipLineHeight = tft.fontHeight();
        const std::vector<String> ipWrappedLines = wrapText(String(displayState.ipInfo), ipFont, tft.width() - 10);

        for (size_t i = 0; i < ipWrappedLines.size(); i++) {
            tft.drawString(ipWrappedLines[i], tft.width() / 2, i * ipLineHeight + currentY, ipFont);
        }
        currentY += ipWrappedLines.size() * ipLineHeight + 20; // Add spacing after IP
    }

    // Calculate remaining vertical space
    const int remainingHeight = tft.height() - currentY;

    // Display "AP Mode" header
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    constexpr int headerFont = FONT_DEFAULT;
    tft.setTextFont(headerFont);
    const int headerHeight = tft.fontHeight();

    // Display SSID label and value
    constexpr int labelFont = FONT_MESSAGE;
    constexpr int valueFont = FONT_DEFAULT;
    tft.setTextFont(labelFont);
    const int labelHeight = tft.fontHeight();
    tft.setTextFont(valueFont);
    const int valueHeight = tft.fontHeight();

    // Calculate total content height
    const int totalContentHeight = headerHeight + 10 + // "AP Mode" + spacing
                                   labelHeight + valueHeight + 10 + // SSID section + spacing
                                   labelHeight + valueHeight; // Password section

    // Center content vertically in remaining space
    int contentStartY = currentY + (remainingHeight - totalContentHeight) / 2;

    // Draw "AP Mode" header
    tft.setTextFont(headerFont);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("AP Mode", tft.width() / 2, contentStartY, headerFont);
    contentStartY += headerHeight + 10;

    // Draw SSID label
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("SSID:", tft.width() / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw SSID value
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    std::vector<String> ssidWrappedLines = wrapText(String(displayState.apSSID), valueFont, tft.width() - 20);
    // -20 for margins
    const int ssidLineY = contentStartY;
    for (size_t i = 0; i < ssidWrappedLines.size(); i++) {
        tft.drawString(ssidWrappedLines[i], tft.width() / 2, i * valueHeight + ssidLineY, valueFont);
    }
    contentStartY += ssidWrappedLines.size() * valueHeight + 10;

    // Draw Password label
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Password:", tft.width() / 2, contentStartY, labelFont);
    contentStartY += labelHeight;

    // Draw Password value
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    const std::vector<String> passwordWrappedLines = wrapText(String(displayState.apPassword), valueFont,
                                                              tft.width() - 20);
    // -20 for margins
    const int passwordLineY = contentStartY;
    for (size_t i = 0; i < passwordWrappedLines.size(); i++) {
        tft.drawString(passwordWrappedLines[i], tft.width() / 2, i * valueHeight + passwordLineY, valueFont);
    }
}

void displayBlankScreen() {
    tft.fillScreen(TFT_BLACK);
    logPrint(F("Display blanked to black."));
}

void displayRenderImage(const char *path) {
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

void displayShowMessage(const String &msg) {
    tft.fillScreen(TFT_BLACK); // Re-added for previous behavior
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

    std::vector<String> finalWrappedLines;
    for (const auto &linesToProces: linesToProcess) {
        std::vector<String> wrapped = wrapText(linesToProces, font, tft.width());
        finalWrappedLines.insert(finalWrappedLines.end(), wrapped.begin(), wrapped.end());
    }

    // Adjust startY to vertically center the block of text
    startY -= finalWrappedLines.size() * lineHeight / 2;

    // Draw each wrapped line
    for (size_t i = 0; i < finalWrappedLines.size(); i++) {
        tft.drawString(finalWrappedLines[i], tft.width() / 2, i * lineHeight + startY, font);
    }
}

void displayShowAPScreen(const char *ssid, const char *password, const char *ip) {
    logPrint(F("Switching to AP screen"));
    displayState.apMode = true;
    strncpy(displayState.apSSID, ssid, sizeof(displayState.apSSID));
    displayState.apSSID[sizeof(displayState.apSSID) - 1] = '\0'; // Ensure null-termination
    strncpy(displayState.apPassword, password, sizeof(displayState.apPassword));
    displayState.apPassword[sizeof(displayState.apPassword) - 1] = '\0'; // Ensure null-termination
    strncpy(displayState.ipInfo, ip, sizeof(displayState.ipInfo));
    displayState.ipInfo[sizeof(displayState.ipInfo) - 1] = '\0'; // Ensure null-termination
    displayUpdate();
}

void displayCycleNextPage() {
    // Don't cycle pages if in AP mode
    if (displayState.apMode) {
        logPrint(F("Page cycling disabled in AP mode"));
        return;
    }

    // Cycle: Clock -> Image (if available) -> Clock
    if (appSettings.theme == 1) {
        // Currently showing clock, try to switch to image if available
        if (appSettings.lastImage[0] != '\0' && LittleFS.exists(appSettings.lastImage)) {
            logPrint(F("Cycling to image page"));
            appSettings.theme = 3;
        } else {
            logPrint(F("No image available, staying on clock page"));
        }
    } else {
        // Currently showing image, switch back to clock
        logPrint(F("Cycling to clock page"));
        appSettings.theme = 1;
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
