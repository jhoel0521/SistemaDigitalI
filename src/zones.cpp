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
    else
    {
        // FUERA DE HORARIO: Sistema de seguridad - Lógica principal de control
        bool hayMovimientoGlobal = false;

        // Verificar si hay movimiento reciente en cualquier zona
        for (int i = 0; i < 2; i++)
        {
            if ((tiempoActual - zonas[i].ultimoMovimiento) <= TIEMPO_MAXIMO_ENCENDIDO)
            {
                hayMovimientoGlobal = true;
                break;
            }
        }

        // Si NO hay movimiento global, apagar TODAS las zonas
        if (!hayMovimientoGlobal)
        {
            // se apagan todas las zonas
            for (int i = 0; i < 2; i++)
            {
                if (zonas[i].estaActivo)
                {
                    configurarEstadoZona(i, false);
                    Serial.printf("Zona %d apagada por falta de movimiento (fuera de horario)\n", i + 1);
                }
            }
        }
        else
        {
            // HAY movimiento: mantener countdown individual
            for (int i = 0; i < 2; i++)
            {
                if (zonas[i].estaActivo && zonas[i].tiempoEncendido > 0)
                {
                    unsigned long tiempoEncendido = tiempoActual - zonas[i].tiempoEncendido;

                    // Apagar si excede el tiempo máximo (5 minutos)
                    if (tiempoEncendido > TIEMPO_MAXIMO_ENCENDIDO)
                    {
                        configurarEstadoZona(i, false);
                        Serial.printf("Zona %d apagada por timeout máximo (5 min) - fuera de horario\n", i + 1);
                    }
                }
            }
        }
    }
}