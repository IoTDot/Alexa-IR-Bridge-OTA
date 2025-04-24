#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include "DeviceConfig.h"
#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer WebServerType;
#else
  #include <WebServer.h>
  typedef WebServer WebServerType;
#endif

// Initialize and tear down the web interface
void setupWebInterface(WebServerType &server);
void stopWebInterface(WebServerType &server);

// Handler to reboot the ESP via the web interface
void handleRestart();
void handleTest();

#endif // WEBINTERFACE_H