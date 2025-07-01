#include <Arduino.h>
#pragma once

// Configuración WiFi - Declaraciones extern
extern const char *ssid;
extern const char *password;

// Constantes
const unsigned long TIEMPO_MAXIMO_ENCENDIDO = 300000; // 5 minutos
const int VALOR_RELAY_ENCENDIDO = LOW;
const int VALOR_RELAY_APAGADO = HIGH;
const int CANTIDAD_ZONAS = 2;
const int CANTIDAD_HORARIOS = 2;