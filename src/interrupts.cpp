#include "interrupts.h"
#include "config.h"
#include "zones.h"
#include "time_utils.h"
#include <Arduino.h>

volatile bool pir1Disparado = false;
volatile bool pir2Disparado = false;
volatile unsigned long tiempoPir1 = 0;
volatile unsigned long tiempoPir2 = 0;

void IRAM_ATTR manejarPIR1()
{
    if (estaEnHorarioLaboral)
        return;
    pir1Disparado = true;
    tiempoPir1 = millis();
}

void IRAM_ATTR manejarPIR2()
{
    if (estaEnHorarioLaboral)
        return;
    pir2Disparado = true;
    tiempoPir2 = millis();
}

void procesarInterrupcionesPIR()
{

    if (estaEnHorarioLaboral)
    {
        pir1Disparado = false;
        pir2Disparado = false;
        return;
    }
    else
    {
        if (pir1Disparado)
        {
            pir1Disparado = false;
            zonas[0].ultimoMovimiento = tiempoPir1;
            Serial.printf("Zona 1: Movimiento detectado en tiempo %lu ms\n", tiempoPir1);
        }
        if (pir2Disparado)
        {
            pir2Disparado = false;
            zonas[1].ultimoMovimiento = tiempoPir2;
            Serial.printf("Zona 2: Movimiento detectado en tiempo %lu ms\n", tiempoPir2);
        }
    }
}