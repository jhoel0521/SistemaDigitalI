#pragma once

#include <Arduino.h>

extern String horariosLaborales[][2];
extern int horaActual;
extern int minutoActual;
extern int segundoActual;
extern unsigned long referenciaDelTiempo;
extern bool estaEnHorarioLaboral;

void actualizarRelojInterno();
bool verificarSiEsHorarioLaboral();