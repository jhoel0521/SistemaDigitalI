#pragma once

#include <Arduino.h>

extern volatile bool pir1Disparado;
extern volatile bool pir2Disparado;
extern volatile unsigned long tiempoPir1;
extern volatile unsigned long tiempoPir2;

void IRAM_ATTR manejarPIR1();
void IRAM_ATTR manejarPIR2();
void procesarInterrupcionesPIR();