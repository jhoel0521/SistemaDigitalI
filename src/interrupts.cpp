#include "interrupts.h"
#include "config.h"
#include "zones.h"
#include "time_utils.h"
#include <Arduino.h>

// Estados anteriores para detectar cambios (HIGH -> LOW o LOW -> HIGH)
bool estadosAnterioresPIR[CANTIDAD_ZONAS] = {false, false};
unsigned long ultimaLecturaPIR = 0;

// Función mejorada que lee todos los PIR en el loop
// COMPORTAMIENTO DE AHORRO ENERGÉTICO:
// - Durante horario laboral: PIR inactivos (control manual únicamente)
// - Fuera de horario: PIR SOLO extienden tiempo de zonas YA ENCENDIDAS
//   NUNCA encienden zonas apagadas para ahorrar energía
void procesarInterrupcionesPIR()
{
    // Leer PIR cada 100ms para no saturar (PIR responde lento anyway)
    unsigned long tiempoActual = millis();
    if (tiempoActual - ultimaLecturaPIR < 100)
    {
        return;
    }
    ultimaLecturaPIR = tiempoActual;

    // Si estamos en horario laboral, no procesar PIR
    if (estaEnHorarioLaboral)
    {
        // Resetear estados para evitar falsas alarmas al salir de horario
        for (int i = 0; i < CANTIDAD_ZONAS; i++)
        {
            estadosAnterioresPIR[i] = false;
        }
        return;
    }

    // Leer todos los sensores PIR
    for (int i = 0; i < CANTIDAD_ZONAS; i++)
    {
        bool estadoActual = digitalRead(zonas[i].pinPir);

        // Detectar transición de LOW a HIGH (nuevo movimiento)
        if (estadoActual && !estadosAnterioresPIR[i])
        {
            // AHORRO ENERGÉTICO: Fuera de horario, PIR SOLO extiende tiempo de zonas YA ENCENDIDAS
            // NUNCA enciende zonas apagadas para ahorrar energía
            if (zonas[i].estaActivo)
            {
                // Solo si la zona YA está encendida, extender su tiempo de actividad
                zonas[i].ultimoMovimiento = tiempoActual;
                Serial.printf("Zona %d: Movimiento detectado (PIR pin %d) - EXTENDIENDO tiempo de zona encendida\n",
                              i + 1, zonas[i].pinPir);
            }
            else
            {
                // Zona apagada: PIR NO la enciende para ahorrar energía
                Serial.printf("Zona %d: Movimiento detectado (PIR pin %d) - pero zona APAGADA, NO se enciende (ahorro energético)\n",
                              i + 1, zonas[i].pinPir);
            }
        }

        // Actualizar estado anterior
        estadosAnterioresPIR[i] = estadoActual;
    }
}