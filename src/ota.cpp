#include "ota.h"
#include "config.h"
#include "display.h"
#include "utils.h"
#include <ArduinoOTA.h>
#include <Updater.h>

static int lastPercent = -1;

static bool otaInProgress       = false;
static bool otaWebStarted       = false;
static bool otaFailed           = false;
static bool otaRebootScheduled  = false;
static bool otaSuccessShown     = false;
static size_t otaWebProgress    = 0;
static size_t otaWebTotal       = 0;
static unsigned long otaRebootAt   = 0;
static unsigned long otaLastDataAt = 0;

static void otaUiStart(const String &type) {
    Serial.println("OTA Start: " + type);
    showMessage(F("OTA Update..."), 0, -15);
    tft.drawRect(20, 120, 200, 20, TFT_WHITE);
    tft.fillRect(22, 122, 196, 16, TFT_BLACK);
    lastPercent = -1;
}

static void otaUiProgress(unsigned int progress, unsigned int total) {
    if (total == 0) return;
    const int percent = progress * 100 / total;
    if (percent != lastPercent) {
        tft.fillRect(22, 122, percent * 196 / 100, 16, TFT_BLUE);
        lastPercent = percent;
    }
}

static void otaUiEnd() {
    Serial.println(F("OTA Complete"));
    showMessage(F("Success!\nRebooting..."));
}

static void otaUiError(const String &errorMsg) {
    Serial.println("OTA Error: " + errorMsg);
    showMessage(F("OTA Failed!"));
}

bool otaIsInProgress() {
    return otaInProgress;
}

void otaInit() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([] {
        otaInProgress = true;
        const String type = ArduinoOTA.getCommand() == U_FLASH ? F("firmware") : F("filesystem");
        otaUiStart(type);
    });

    ArduinoOTA.onEnd([] {
        otaUiEnd();
        delay(2000);
    });

    ArduinoOTA.onProgress([](const unsigned int progress, const unsigned int total) {
        otaUiProgress(progress, total);
    });

    ArduinoOTA.onError([](const ota_error_t error) {
        otaInProgress = false;
        otaUiError(String(error));
    });

    ArduinoOTA.begin();
    Serial.println(F("ArduinoOTA ready"));
}

void otaHandleWebUpload(AsyncWebServerRequest *request, const String &filename,
                        size_t index, uint8_t *data, size_t len, bool final) {
    (void)filename;
    otaLastDataAt = millis();

    if (index == 0) {
        otaInProgress      = true;
        otaWebStarted      = true;
        otaFailed          = false;
        otaRebootScheduled = false;
        otaSuccessShown    = false;
        otaWebProgress     = 0;
        otaWebTotal        = request->contentLength();
        Update.runAsync(true);
        const uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace, U_FLASH)) {
            Update.printError(Serial);
            otaFailed = true;
        }
    }

    if (!Update.hasError() && !otaFailed) {
        if (len > 0) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
                otaFailed = true;
            } else {
                otaWebProgress = index + len;
            }
        }
    }

    if (final) {
        if (!Update.end(true)) {
            Update.printError(Serial);
            otaFailed = true;
        }
    }
}

void otaHandleWebRequest(AsyncWebServerRequest *request) {
    const bool ok = !Update.hasError() && !otaFailed;
    AsyncWebServerResponse *response = request->beginResponse(
        200, "text/plain", ok ? F("OK - Rebooting...") : F("FAIL")
    );
    response->addHeader("Connection", "close");
    request->send(response);
    if (ok) {
        otaRebootScheduled = true;
        otaRebootAt = millis() + 1000;
    } else {
        otaFailed = true;
    }
}

void otaHandle() {
    ArduinoOTA.handle();

    if (otaWebStarted) {
        otaWebStarted = false;
        otaUiStart(F("web"));
    }

    if (otaInProgress && otaWebTotal > 0) {
        otaUiProgress(otaWebProgress, otaWebTotal);
    }

    if (otaFailed) {
        otaFailed = false;
        otaInProgress = false;
        otaUiError(F("Web upload failed"));
    }

    if (otaRebootScheduled) {
        if (!otaSuccessShown) {
            otaSuccessShown = true;
            otaUiEnd();
        }
        if (millis() >= otaRebootAt) {
            ESP.restart();
        }
        return;
    }

    // Abort if Web OTA stalled for more than 30 seconds
    if (otaInProgress && !otaRebootScheduled && (millis() - otaLastDataAt > 30000)) {
        Update.end(false);
        otaInProgress = false;
        otaUiError(F("Upload timeout"));
    }
}
