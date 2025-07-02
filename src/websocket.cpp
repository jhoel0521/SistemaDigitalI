#include "websocket.h"
#include "config.h"
#include "zones.h"
#include "time_utils.h"
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

WebSocketsServer socketWeb = WebSocketsServer(81);

void eventoSocketWeb(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
        Serial.printf("[%u] Desconectado!\n", num);
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = socketWeb.remoteIP(num);
        Serial.printf("[%u] Conectado desde %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        enviarEstadoPorSocketWeb();
        break;
    }
    case WStype_TEXT:
        // Manejar comandos del cliente si es necesario
        break;
    }
}

void enviarEstadoPorSocketWeb()
{

    JsonDocument documento;

    // Hora actual
    documento["hora"] = horaActual;
    documento["minuto"] = minutoActual;
    documento["segundo"] = segundoActual;

    // Modo de operación
    documento["modo"] = estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario";
    documento["modoActivo"] = estaEnHorarioLaboral;

    // Estado de zonas
    JsonArray arregloZonas = documento["zonas"].to<JsonArray>();
    for (int i = 0; i < 2; i++)
    {
        JsonObject objetoZona = arregloZonas.add<JsonObject>();
        objetoZona["nombre"] = zonas[i].nombre;
        objetoZona["activo"] = zonas[i].estaActivo;

        // Calcular tiempo desde último movimiento de forma segura
        unsigned long tiempoDesdeMovimiento = 0;
        if (zonas[i].ultimoMovimiento > 0)
        {
            tiempoDesdeMovimiento = (millis() - zonas[i].ultimoMovimiento) / 1000;
        }
        else
        {
            tiempoDesdeMovimiento = 999999; // Nunca hubo movimiento
        }
        objetoZona["movimiento"] = tiempoDesdeMovimiento;

        objetoZona["tiempoEncendido"] = zonas[i].tiempoEncendido > 0 ? (millis() - zonas[i].tiempoEncendido) / 1000 : 0; // Tiempo desde que se encendió
        objetoZona["sensorActual"] = digitalRead(zonas[i].pinPir);                                                       // Estado actual del sensor PIR
        // Calcular countdown si está activo
        if (zonas[i].estaActivo && zonas[i].ultimoMovimiento > 0)
        {
            unsigned long tiempoRestante = 0;

            if (estaEnHorarioLaboral)
            {
                // EN HORARIO LABORAL: SIN COUNTDOWN - Las luces permanecen encendidas para trabajar
                tiempoRestante = 0; // No hay countdown en horario laboral
            }
            else
            {
                // FUERA DE HORARIO: Sistema de seguridad con countdown individual por zona
                unsigned long tiempoSinMovimientoZona = millis() - zonas[i].ultimoMovimiento;
                
                if (tiempoSinMovimientoZona >= TIEMPO_MAXIMO_ENCENDIDO)
                {
                    tiempoRestante = 0; // Se apagará inmediatamente
                }
                else
                {
                    // Calcular tiempo restante hasta apagado (5 minutos desde último movimiento)
                    tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO - tiempoSinMovimientoZona) / 1000;
                }
            }

            objetoZona["countdown"] = tiempoRestante;
        }
        else
        {
            objetoZona["countdown"] = 0;
        }

        // Historial de actividad (opcional, para futuras expansiones)
        JsonArray actividadArray = objetoZona["actividad"].to<JsonArray>();
    }

    String cadenaJson;
    serializeJson(documento, cadenaJson);
    socketWeb.broadcastTXT(cadenaJson);

    // Debug: mostrar datos enviados cada 10 segundos para no saturar
    static unsigned long ultimoDebug = 0;
    if (millis() - ultimoDebug > 10000)
    {
        Serial.printf("WebSocket - Modo: %s (%s), Hora: %02d:%02d:%02d\n",
                      estaEnHorarioLaboral ? "Laboral" : "Fuera",
                      estaEnHorarioLaboral ? "true" : "false",
                      horaActual, minutoActual, segundoActual);
        for (int i = 0; i < 2; i++)
        {
            unsigned long tiempoDesdeMovimiento = 0;
            if (zonas[i].ultimoMovimiento > 0)
            {
                tiempoDesdeMovimiento = (millis() - zonas[i].ultimoMovimiento) / 1000;
            }
            else
            {
                tiempoDesdeMovimiento = 999999;
            }
            Serial.printf("  Zona %d: activo=%s, movimiento=%lus, PIR=%d\n",
                          i + 1, zonas[i].estaActivo ? "SI" : "NO", tiempoDesdeMovimiento, digitalRead(zonas[i].pinPir));
        }
        ultimoDebug = millis();
    }
}