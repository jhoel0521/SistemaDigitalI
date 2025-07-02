#include "interrupts.h"
#include "config.h"
#include "zones.h"
#include "time_utils.h"
#include <Arduino.h>

// Estados anteriores para detectar cambios (HIGH -> LOW o LOW -> HIGH)
bool estadosAnterioresPIR[CANTIDAD_ZONAS] = {false, false};
unsigned long ultimaLecturaPIR = 0;

// Función mejorada que lee todos los PIR en el loop
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
            zonas[i].ultimoMovimiento = tiempoActual;
            Serial.printf("Zona %d: Movimiento detectado (PIR pin %d) en tiempo %lu ms\n",
                          i + 1, zonas[i].pinPir, tiempoActual);

            // Si la zona no está activa, encenderla automáticamente
            if (!zonas[i].estaActivo)
            {
                zonas[i].ultimoMovimiento = tiempoActual;
                Serial.printf("Zona %d: Se detectó movimiento, Extendiendo tiempo de actividad\n", i + 1);
            }
        }

        // Actualizar estado anterior
        estadosAnterioresPIR[i] = estadoActual;
    }
}