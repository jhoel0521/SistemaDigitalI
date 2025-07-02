#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "zones.h"
#include "mi_webserver.h"
#include "websocket.h"
#include "time_utils.h"
#include "interrupts.h"

// Variables para mejorar sincronización WebSocket
unsigned long ultimaActualizacionSensor = 0;
bool estadoSensoresAnterior[CANTIDAD_ZONAS] = {false, false};

void setup() {
  delay(2000);
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Inicializar pines
  for (int i = 0; i < CANTIDAD_ZONAS; i++) {
    pinMode(zonas[i].pinPir, INPUT);
    for (int j = 0; j < 2; j++) {
      pinMode(zonas[i].pinesRelay[j], OUTPUT);
      digitalWrite(zonas[i].pinesRelay[j], VALOR_RELAY_APAGADO);
    }
  }

  // Configurar como punto de acceso WiFi
  WiFi.softAP(ssid, password);
  Serial.println("\nPunto de acceso creado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Iniciar el reloj interno
  referenciaDelTiempo = millis();

  // Las interrupciones PIR han sido reemplazadas por lectura en loop()
  // Esto es más estable y eficiente para sensores PIR que tienen retardo interno
  Serial.println("Sensores PIR configurados para lectura por polling (más estable)");

  // Configurar rutas del servidor web
  servidor.on("/", manejarPaginaPrincipal);
  servidor.on("/on", manejarControlManual);
  servidor.on("/off", manejarControlManual);
  servidor.on("/update", HTTP_POST, manejarActualizacionHorarios);
  servidor.on("/settime", HTTP_POST, manejarConfiguracionHora);
  servidor.onNotFound(manejarPaginaNoEncontrada);

  // Iniciar servidor WebSocket
  socketWeb.begin();
  socketWeb.onEvent(eventoSocketWeb);

  servidor.begin();
  Serial.println("Servidor HTTP iniciado");
  Serial.println("Servidor WebSocket iniciado en puerto 81");
  
  // Inicializar estado del sistema
  estaEnHorarioLaboral = verificarSiEsHorarioLaboral();
  Serial.printf("Estado inicial: %s\n", estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario");
  
  // Mostrar estado inicial de las zonas
  for (int i = 0; i < CANTIDAD_ZONAS; i++) {
    Serial.printf("Zona %d: %s, PIR pin %d, Relays %d y %d\n", 
      i+1, zonas[i].nombre.c_str(), zonas[i].pinPir, 
      zonas[i].pinesRelay[0], zonas[i].pinesRelay[1]);
  }
}

void loop() {
  servidor.handleClient();
  socketWeb.loop();
  actualizarRelojInterno();

  // Actualizar modo horario
  bool nuevoModoHorario = verificarSiEsHorarioLaboral();
  if (estaEnHorarioLaboral != nuevoModoHorario) {
    estaEnHorarioLaboral = nuevoModoHorario;
    Serial.printf("*** CAMBIO DE MODO *** De %s a %s\n", 
      !estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario",
      estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario");

    if (!estaEnHorarioLaboral) {
      for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        if (zonas[i].estaActivo) {
          zonas[i].ultimoMovimiento = millis();
          Serial.printf("Zona %d: Tiempo de movimiento actualizado por cambio de modo\n", i + 1);
        }
      }
    }
    enviarEstadoPorSocketWeb();
  }

  procesarInterrupcionesPIR();
  controlarApagadoAutomatico();

  // Enviar estado WebSocket periódicamente
  static unsigned long ultimoEnvioWebSocket = 0;
  if (millis() - ultimoEnvioWebSocket > 500) {
    enviarEstadoPorSocketWeb();
    ultimoEnvioWebSocket = millis();
  }

  delay(10);
}