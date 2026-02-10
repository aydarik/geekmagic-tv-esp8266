#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <NTPClient.h> // Include for NTPClient access
#include <WiFiManager.h> // Include for WiFiManager access
#include "display.h" // Include for display functions

void webserverInit();

void webserverHandle();

void handleRoot();

extern ESP8266WebServer server;
extern NTPClient timeClient; // Declare NTPClient object as extern
extern WiFiManager wifiManager; // Declare WiFiManager object as extern
extern bool wifiFailsafeMode; // WiFi failsafe mode flag

#endif
