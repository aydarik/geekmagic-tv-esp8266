#include "notification.h"

#include "config.h"
#include "display.h"

NotificationState notificationState;

void themeRenderNotification() {
    if (notificationState.message[0] == '\0') {
        displayShowMessage(F("No messages"));
        return;
    }
    tft.fillScreen(TFT_BLACK);

    const int centerY = tft.height() / 2;
    const int centerX = tft.width() / 2;
    int currentY = 0;

    // Draw subject
    if (notificationState.subject[0] != '\0') {
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString(String(notificationState.subject), centerX, currentY, FONT_DEFAULT);
        currentY = 40;
    }

    // Draw message
    const auto style = String(notificationState.style);
    const auto msg = String(notificationState.message);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (style == "big_num") {
        tft.setTextDatum(MC_DATUM);
        tft.drawString(notificationState.message, centerX, centerY + currentY / 2, FONT_HUGE_NUM);
    } else {
        int currentX = 0;
        constexpr int linesOffset = 35;

        // Split message by newline characters
        std::vector<String> linesToProcess;
        unsigned int prev = 0;
        for (unsigned int i = 0; i < msg.length(); i++) {
            if (msg.charAt(i) == '\n') {
                linesToProcess.push_back(msg.substring(prev, i));
                prev = i + 1;
            }
        }
        linesToProcess.push_back(msg.substring(prev)); // Add the last part

        // Draw each wrapped line
        const unsigned int lines = linesToProcess.size();

        if (style == "center") {
            tft.setTextDatum(TC_DATUM);
            currentX = centerX;
            currentY = centerY + currentY / 2 - linesOffset * lines / 2;
        } else {
            tft.setTextDatum(TL_DATUM);
        }

        tft.startWrite();
        for (unsigned int i = 0; i < lines; i++) {
            tft.drawString(linesToProcess[i], currentX, currentY + i * linesOffset, FONT_DEFAULT);
        }
        tft.endWrite();
    }
}
