#include "time_utils.h"
#include "config.h"
#include <Arduino.h>

String horariosLaborales[CANTIDAD_HORARIOS][2] = {
    {"08:00", "12:00"},
    {"14:00", "18:10"}};

int horaActual = 19;
int minutoActual = 0;
int segundoActual = 0;
unsigned long referenciaDelTiempo = 0;
bool estaEnHorarioLaboral = true;

void actualizarRelojInterno()
{

    unsigned long tiempoActual = millis();
    unsigned long segundosTranscurridos = (tiempoActual - referenciaDelTiempo) / 1000;

    if (segundosTranscurridos > 0)
    {
        segundoActual += segundosTranscurridos;

        // Mostrar hora solo cada minuto para no saturar Serial
        static int ultimoMinutoMostrado = -1;

        if (segundoActual >= 60)
        {
            minutoActual += segundoActual / 60;
            segundoActual %= 60;

            if (minutoActual >= 60)
            {
                horaActual += minutoActual / 60;
                minutoActual %= 60;

                if (horaActual >= 24)
                {
                    horaActual %= 24;
                }
            }
        }

        // Solo mostrar cuando cambie el minuto
        if (minutoActual != ultimoMinutoMostrado)
        {
            Serial.printf("Hora actual: %02d:%02d:%02d\n", horaActual, minutoActual, segundoActual);
            ultimoMinutoMostrado = minutoActual;
        }

        referenciaDelTiempo = tiempoActual;
    }
}

bool verificarSiEsHorarioLaboral()
{
    for (int i = 0; i < CANTIDAD_HORARIOS; ++i)
    {
        int horaInicio, minutoInicio, horaFin, minutoFin;
        sscanf(horariosLaborales[i][0].c_str(), "%d:%d", &horaInicio, &minutoInicio);
        sscanf(horariosLaborales[i][1].c_str(), "%d:%d", &horaFin, &minutoFin);

        int minutosActuales = horaActual * 60 + minutoActual;
        int minutosInicio = horaInicio * 60 + minutoInicio;
        int minutosFin = horaFin * 60 + minutoFin;

        if (minutosActuales >= minutosInicio && minutosActuales < minutosFin)
        {
            return true;
        }
    }
    return false;
}