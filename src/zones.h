#pragma once
#include <Arduino.h>
#include "config.h"

struct Zona
{
    int pinPir;
    int pinesRelay[2];
    unsigned long ultimoMovimiento;
    unsigned long tiempoEncendido;
    bool estaActivo;
    String nombre;

    Zona(int pir, int relay1, int relay2, String nombre);
};

extern Zona zonas[];

void configurarEstadoZona(int indiceZona, bool activar);
void controlarApagadoAutomatico();