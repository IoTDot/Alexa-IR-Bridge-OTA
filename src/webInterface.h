//this is webinteface.h
#ifndef WEBINTERFACE_H
#define WEBINTERFACE_H

#include "DeviceConfig.h"

#if defined(ESP8266)
  #include <ESP8266WebServer.h>
  typedef ESP8266WebServer WebServerType;
#else
  #include <WebServer.h>
  typedef WebServer WebServerType;
#endif

// Deklaracja funkcji interfejsu WWW
void setupWebInterface(WebServerType &server);
void stopWebInterface(WebServerType &server);

#endif // WEBINTERFACE_H
