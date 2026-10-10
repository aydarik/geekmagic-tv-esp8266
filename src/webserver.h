#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ArduinoJson.h>

void webserverInit();
void webserverHandle();
void wsBroadcast(const JsonDocument &doc);
void wsBroadcastState();

// Sends the configured webhook (GET or POST). Blocking. For POST, `body` is sent as JSON.
bool sendWebhook();

// Deferred sending: request from any context, executed from loop() via webhookProcess()
void webhookRequest();
void webhookProcess();

#endif
