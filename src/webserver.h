#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ArduinoJson.h>

void webserverInit();
void webserverHandle();
void wsBroadcast(const JsonDocument &doc);
void wsBroadcastState();

#endif
