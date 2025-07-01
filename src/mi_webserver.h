#pragma once
#include <WebServer.h>

extern WebServer servidor;

void manejarPaginaPrincipal();
void enviarJavaScript();
void manejarPaginaNoEncontrada();
void manejarControlManual();
void manejarActualizacionHorarios();
void manejarConfiguracionHora();