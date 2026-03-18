#include "weather.h"
#include "display.h"
#include "settings.h"
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <weather_icons.h>
#include "config.h"
#include "TJpg_Decoder.h"
#include "utils.h"
#include "fonts/Roboto_Regular24.h"

extern Settings appSettings;

static int httpCode = 0;
static float currentTemp = 0.0f;
static float feelsLike = 0.0f;
static char iconCode[8] = "";

struct IconMap {
    const char *key;
    const uint8_t *value;
    const unsigned int size;
};

const IconMap icons[] PROGMEM = {
    {"01d", owm_icon_01d, owm_icon_01d_len},
    {"01n", owm_icon_01n, owm_icon_01n_len},
    {"02d", owm_icon_02d, owm_icon_02d_len},
    {"02n", owm_icon_02n, owm_icon_02n_len},
    {"03d", owm_icon_03d, owm_icon_03d_len},
    {"03n", owm_icon_03n, owm_icon_03n_len},
    {"04d", owm_icon_04d, owm_icon_04d_len},
    {"04n", owm_icon_04n, owm_icon_04n_len},
    {"09d", owm_icon_09d, owm_icon_09d_len},
    {"09n", owm_icon_09n, owm_icon_09n_len},
    {"10d", owm_icon_10d, owm_icon_10d_len},
    {"10n", owm_icon_10n, owm_icon_10n_len},
    {"11d", owm_icon_11d, owm_icon_11d_len},
    {"11n", owm_icon_11n, owm_icon_11n_len},
    {"13d", owm_icon_13d, owm_icon_13d_len},
    {"13n", owm_icon_13n, owm_icon_13n_len},
    {"50d", owm_icon_50d, owm_icon_50d_len},
    {"50n", owm_icon_50n, owm_icon_50n_len},
};

const IconMap *getIcon(const char *key) {
    for (const auto &icon: icons) {
        const IconMap *entry = &icon;
        const auto storedKey = static_cast<const char *>(pgm_read_ptr(&entry->key));
        if (strcmp_P(key, storedKey) == 0) {
            return entry; // pointer to PROGMEM
        }
    }
    return nullptr;
}

bool weatherUpdateTask() {
    if (!appSettings.showWeather || appSettings.brightness == 0 || displayState.theme != 1) return false;
    if (appSettings.owmApiKey[0] == '\0' || appSettings.owmLocation[0] == '\0') return false;

    char url[256];
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
             appSettings.owmLocation, appSettings.owmApiKey);

    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);
    httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        if (JsonDocument doc; !deserializeJson(doc, http.getStream())) {
            currentTemp = doc["main"]["temp"] | currentTemp;
            feelsLike = doc["main"]["feels_like"] | feelsLike;
            if (const char *icon = doc["weather"][0]["icon"]; icon) {
                strncpy(iconCode, icon, sizeof(iconCode) - 1);
                iconCode[sizeof(iconCode) - 1] = '\0';
            }
        }
    }
    http.end();

    renderWeather(true);
    return true;
}

void clearWeather() {
    tft.startWrite();
    for (int i = 1; i <= 36 / 2; ++i) {
        tft.drawFastHLine(0, i, 240, TFT_BLACK);
        tft.drawFastHLine(0, 36 - i, 240, TFT_BLACK);
        delay(ANIMATION_STEP_DELAY);
    }
    tft.endWrite();
}

void renderWeather(const bool clear) {
    char tempStr[24];
    if (httpCode == HTTP_CODE_OK) snprintf(tempStr, sizeof(tempStr), "%.0f°, feels like %.0f°", currentTemp, feelsLike);
    else if (httpCode == 0) snprintf(tempStr, sizeof(tempStr), "Loading...");
    else snprintf(tempStr, sizeof(tempStr), "FAILED: %d", httpCode);

    if (clear) clearWeather();

    tft.setTextDatum(TL_DATUM);
    tft.loadFont(Roboto_Regular24);
    tft.drawString(tempStr, 35, 7);
    tft.unloadFont();

    if (httpCode == HTTP_CODE_OK && strlen(iconCode) > 0) {
        const IconMap *icon = getIcon(iconCode);
        TJpgDec.drawJpg(1, 1,
                        static_cast<const uint8_t *>(pgm_read_ptr(&icon->value)),
                        pgm_read_dword(&icon->size));
    }
}
