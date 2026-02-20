#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <WiFiManager.h> // Include for WiFiManager access

#define CONTENT_TYPE_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_HTML "text/html"

void webserverInit();

void webserverHandle();

extern ESP8266WebServer server;
extern WiFiManager wifiManager; // Declare WiFiManager object as extern

#endif
