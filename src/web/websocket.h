#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

void webSocketInit(AsyncWebServer *server);
void broadcastTelemetry();
bool isWebSocketClientConnected();

#endif // WEBSOCKET_H
