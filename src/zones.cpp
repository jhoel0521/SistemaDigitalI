#include "zones.h"
#include "config.h"
#include "time_utils.h"
#include <Arduino.h>

Zona zonas[CANTIDAD_ZONAS] = {
    Zona(13, 32, 25, "Zona 1"),
    Zona(15, 26, 21, "Zona 2")};

Zona::Zona(int pir, int relay1, int relay2, String nombre)
{
    pinPir = pir;
    pinesRelay[0] = relay1;
    pinesRelay[1] = relay2;
    ultimoMovimiento = 0;
    tiempoEncendido = 0;
    estaActivo = false;
    this->nombre = nombre;
}

void configurarEstadoZona(int indiceZona, bool activar)
{
    zonas[indiceZona].estaActivo = activar;
    if (activar)
    {
        zonas[indiceZona].tiempoEncendido = millis();
        Serial.printf("Zona %d: ENCENDIDA en tiempo %lu ms\n", indiceZona + 1, zonas[indiceZona].tiempoEncendido);
    }
    else
    {
        zonas[indiceZona].tiempoEncendido = 0;
        Serial.printf("Zona %d: APAGADA\n", indiceZona + 1);
    }

    for (int i = 0; i < 2; i++)
    {
        digitalWrite(zonas[indiceZona].pinesRelay[i], activar ? VALOR_RELAY_ENCENDIDO : VALOR_RELAY_APAGADO);
    }
}

void controlarApagadoAutomatico()
{
    unsigned long tiempoActual = millis();

    if (estaEnHorarioLaboral)
    {
        // EN HORARIO LABORAL: NO se apagan automáticamente las luces
        // Las luces permanecen encendidas para permitir el trabajo sin interrupciones
        return; // No hacer nada durante horario laboral
    }
    
    // FUERA DE HORARIO: Control independiente por zona
    // Cada zona se controla de forma independiente según su propio movimiento
    for (int i = 0; i < CANTIDAD_ZONAS; i++)
    {
        if (zonas[i].estaActivo)  // Solo verificar zonas que están encendidas
        {
            unsigned long tiempoSinMovimiento = tiempoActual - zonas[i].ultimoMovimiento;
            
            // Si esta zona específica excede 5 minutos sin movimiento, apagarla
            if (tiempoSinMovimiento > TIEMPO_MAXIMO_ENCENDIDO)
            {
                configurarEstadoZona(i, false);
                Serial.printf("Zona %d apagada por timeout (5 min sin movimiento) - fuera de horario\n", i + 1);
            }
        }
    }
}