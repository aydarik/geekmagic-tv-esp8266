#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>

#define CONTENT_TYPE_TEXT "text/plain"
#define CONTENT_TYPE_JSON "application/json"
#define CONTENT_TYPE_HTML "text/html"

void webserverInit();

void webserverHandle();

extern ESP8266WebServer server;

#endif
