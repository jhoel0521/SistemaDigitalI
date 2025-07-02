#pragma once

#include <Arduino.h>

// Estados de PIR para lectura por polling (más estable que interrupciones)
extern bool estadosAnterioresPIR[];
extern unsigned long ultimaLecturaPIR;

// Función principal para procesar PIR por polling
void procesarInterrupcionesPIR();