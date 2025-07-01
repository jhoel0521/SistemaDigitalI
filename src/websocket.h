#pragma once
#include <WebSocketsServer.h>

extern WebSocketsServer socketWeb;

void eventoSocketWeb(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void enviarEstadoPorSocketWeb();