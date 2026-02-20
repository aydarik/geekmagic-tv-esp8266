#include "ap.h"
#include "config.h"
#include "display.h"

void themeRenderAPMode(const bool forceClear) {
    if (!forceClear) return;

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TC_DATUM);

    const int currentX = tft.width() / 2;
    int currentY = 10; // Start from top with small margin

    // Display IP Info at the top (small font)
    if (displayState.ipInfo[0] != '\0') {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        constexpr int ipFont = FONT_MICRO;
        tft.setTextFont(ipFont);
        const int ipLineHeight = tft.fontHeight();
        tft.drawString(String(displayState.ipInfo), currentX, currentY, ipFont);
        currentY += ipLineHeight;
    }

    constexpr int headerFont = FONT_DEFAULT;
    constexpr int labelFont = FONT_SMALL;
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
    tft.drawString(WIFI_AP_NAME, currentX, currentY, valueFont);

    // Draw Password label
    currentY += 40;
    tft.setTextFont(labelFont);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Password:", currentX, currentY, labelFont);

    // Draw Password value
    currentY += 25;
    tft.setTextFont(valueFont);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(WIFI_AP_PASSWORD, currentX, currentY, valueFont);
}
