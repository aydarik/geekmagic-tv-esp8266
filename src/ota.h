#ifndef OTA_H
#define OTA_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void otaInit();
void otaHandle();
bool otaIsInProgress();

// Web upload endpoints for ESPAsyncWebServer
void otaHandleWebRequest(AsyncWebServerRequest *request);
void otaHandleWebUpload(AsyncWebServerRequest *request, const String &filename,
                        size_t index, uint8_t *data, size_t len, bool final);

#endif
