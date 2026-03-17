#include "weather.h"
#include "display.h"
#include "settings.h"
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <weather_icons.h>
#include "TJpg_Decoder.h"
#include "fonts/Roboto_Regular24.h"

extern Settings appSettings;

static unsigned long lastWeatherUpdate = 0;
static int httpCode = 0;
static float currentTemp = 0.0f;
static float feelsLike = 0.0f;
static char iconCode[8] = "";

struct IconMap {
    const char *key;
    const unsigned char *value;
    const unsigned int size;
};

const IconMap icons[] = {
    {"01d", __01d, __01d_len},
    {"01n", __01n, __01n_len},
    {"02d", __02d, __02d_len},
    {"02n", __02n, __02n_len},
    {"03d", __03d, __03d_len},
    {"03n", __03n, __03n_len},
    {"04d", __04d, __04d_len},
    {"04n", __04n, __04n_len},
    {"09d", __09d, __09d_len},
    {"09n", __09n, __09n_len},
    {"10d", __10d, __10d_len},
    {"10n", __10n, __10n_len},
    {"11d", __11d, __11d_len},
    {"11n", __11n, __11n_len},
    {"13d", __13d, __13d_len},
    {"13n", __13n, __13n_len},
    {"50d", __50d, __50d_len},
    {"50n", __50n, __50n_len},
};

const IconMap *getIcon(const char *key) {
    for (const auto &icon: icons) {
        if (strcmp(icon.key, key) == 0) {
            return &icon;
        }
    }
    return nullptr;
}

void weatherUpdateTask(const unsigned long now) {
    if (!appSettings.showWeather) return;
    if (appSettings.owmApiKey[0] == '\0' || appSettings.owmLocation[0] == '\0') return;

    if (lastWeatherUpdate != 0 && now - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL) return;
    lastWeatherUpdate = now;

    char url[256];
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
             appSettings.owmLocation, appSettings.owmApiKey);

    WiFiClient client;
    HTTPClient http;
    http.begin(client, url);
    httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        if (JsonDocument doc; !deserializeJson(doc, payload)) {
            currentTemp = doc["main"]["temp"] | currentTemp;
            feelsLike = doc["main"]["feels_like"] | feelsLike;
            if (const char *icon = doc["weather"][0]["icon"]; icon) {
                strncpy(iconCode, icon, sizeof(iconCode) - 1);
                iconCode[sizeof(iconCode) - 1] = '\0';
            }
        }
    }
    http.end();

    if (displayState.theme == 1) displayUpdate();
}

void renderWeather(const int32_t y) {
    char tempStr[24];
    if (httpCode == HTTP_CODE_OK) snprintf(tempStr, sizeof(tempStr), "%.0f°, feels like %.0f°", currentTemp, feelsLike);
    else if (httpCode == 0) snprintf(tempStr, sizeof(tempStr), "Loading...");
    else snprintf(tempStr, sizeof(tempStr), "FAILED: %d", httpCode);

    tft.setTextDatum(TL_DATUM);
    tft.loadFont(Roboto_Regular24);
    tft.drawString(tempStr, 50, y);
    tft.unloadFont();

    if (httpCode == HTTP_CODE_OK && strlen(iconCode) > 0) {
        const IconMap *icon = getIcon(iconCode);
        TJpgDec.drawJpg(10, y - 5, icon->value, icon->size);
    }
}
