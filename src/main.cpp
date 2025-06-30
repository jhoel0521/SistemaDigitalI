#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
// Seccion de funciones del documento
void manejarPaginaPrincipal();
void manejarPaginaNoEncontrada();
void manejarControlManual();
void manejarActualizacionHorarios();
void manejarConfiguracionHora();
void enviarEstadoPorSocketWeb();
void IRAM_ATTR manejarPIR1();
void IRAM_ATTR manejarPIR2();
void procesarInterrupcionesPIR();
void actualizarRelojInterno();
bool verificarSiEsHorarioLaboral();
void configurarEstadoZona(int indiceZona, bool activar);
void controlarApagadoAutomatico();
// Fin de funciones del documento

// Configuración WiFi
const char *ssid = "SistemaDigitales";
const char *password = "12345678";

// Configuración de pines
#define PIN_PIR1 13
#define PIN_PIR2 15

#define PIN_RELAY1 32
#define PIN_RELAY2 25
#define PIN_RELAY3 26
#define PIN_RELAY4 21

// Variables para manejo de interrupciones
volatile bool pir1Disparado = false;
volatile bool pir2Disparado = false;
volatile unsigned long tiempoPir1 = 0;
volatile unsigned long tiempoPir2 = 0;

// Estructura de zonas
struct Zona
{
  int pinPir;
  int pinesRelay[2];
  unsigned long ultimoMovimiento;
  unsigned long tiempoEncendido; // Tiempo cuando se encendió
  bool estaActivo;
  String nombre;
  // constructor para inicializar la zona
  Zona(int pir, int relay1, int relay2, String nombre)
  {
    pinPir = pir;
    pinesRelay[0] = relay1;
    pinesRelay[1] = relay2;
    ultimoMovimiento = 0;
    tiempoEncendido = 0;
    estaActivo = false;
    this->nombre = nombre;
  }
};

Zona zonas[] = {
    Zona(PIN_PIR1, PIN_RELAY1, PIN_RELAY2, String("Zona 1")),
    Zona(PIN_PIR2, PIN_RELAY3, PIN_RELAY4, String("Zona 2"))};

// Horarios laborales
String horariosLaborales[][2] = {
    {"08:00", "12:00"},
    {"14:00", "18:10"}};
const int cantidadHorarios = 2;

// Constantes
const unsigned long TIEMPO_MAXIMO_ENCENDIDO = 300000; // 5 minutos (300 segundos)
const int VALOR_RELAY_ENCENDIDO = LOW;
const int VALOR_RELAY_APAGADO = HIGH;

// Variables globales
bool estaEnHorarioLaboral = true;
WebServer servidor(80);
WebSocketsServer socketWeb = WebSocketsServer(81);

// Variables para mejorar sincronización WebSocket
unsigned long ultimaActualizacionSensor = 0;
bool estadoSensoresAnterior[2] = {false, false};

// Variables para el reloj manual
int horaActual = 8;                    // Hora inicial: 8 AM
int minutoActual = 0;                  // Minutos iniciales
int segundoActual = 0;                 // Segundos iniciales
unsigned long referenciaDelTiempo = 0; // Para llevar el tiempo transcurrido

void eventoSocketWeb(uint8_t num, WStype_t type, uint8_t *payload, size_t length);

void setup()
{
  delay(2000); // Estabilización inicial
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Inicializar pines
  for (int i = 0; i < 2; i++)
  {
    pinMode(zonas[i].pinPir, INPUT);
    for (int j = 0; j < 2; j++)
    {
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

  // Configurar interrupciones PIR seguras
  attachInterrupt(digitalPinToInterrupt(PIN_PIR1), manejarPIR1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_PIR2), manejarPIR2, RISING);

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
  for (int i = 0; i < 2; i++) {
    Serial.printf("Zona %d: %s, PIR pin %d, Relays %d y %d\n", 
      i+1, zonas[i].nombre.c_str(), zonas[i].pinPir, zonas[i].pinesRelay[0], zonas[i].pinesRelay[1]);
  }
}

void loop()
{
  servidor.handleClient();
  socketWeb.loop();
  actualizarRelojInterno();

  // Actualizar modo horario SIN resetear estados
  bool nuevoModoHorario = verificarSiEsHorarioLaboral();
  if (estaEnHorarioLaboral != nuevoModoHorario)
  {
    estaEnHorarioLaboral = nuevoModoHorario;
    Serial.printf("Modo cambiado a: %s\n", estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario");

    // Al cambiar a fuera de horario, actualizar ultimoMovimiento para todas las zonas activas
    if (!estaEnHorarioLaboral)
    {
      for (int i = 0; i < 2; i++)
      {
        if (zonas[i].estaActivo)
        {
          zonas[i].ultimoMovimiento = millis(); // Dar tiempo de gracia
          Serial.printf("Zona %d: Tiempo de movimiento actualizado por cambio de modo\n", i + 1);
        }
      }
    }

    enviarEstadoPorSocketWeb(); // Enviar inmediatamente cuando cambie el modo
  }

  procesarInterrupcionesPIR();
  controlarApagadoAutomatico();

  // Enviar estado WebSocket más frecuentemente para mejor sincronización
  static unsigned long ultimoEnvioWebSocket = 0;
  if (millis() - ultimoEnvioWebSocket > 500) // Cada 500ms
  {
    enviarEstadoPorSocketWeb();
    ultimoEnvioWebSocket = millis();
  }

  delay(10);
}

// WebSocket Event Handler
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

// Enviar estado actual a todos los clientes WebSocket
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
    if (zonas[i].ultimoMovimiento > 0) {
      tiempoDesdeMovimiento = (millis() - zonas[i].ultimoMovimiento) / 1000;
    } else {
      tiempoDesdeMovimiento = 999999; // Nunca hubo movimiento
    }
    objetoZona["movimiento"] = tiempoDesdeMovimiento;
    
    objetoZona["tiempoEncendido"] = zonas[i].tiempoEncendido > 0 ? (millis() - zonas[i].tiempoEncendido) / 1000 : 0; // Tiempo desde que se encendió
    objetoZona["sensorActual"] = digitalRead(zonas[i].pinPir); // Estado actual del sensor PIR
    // Calcular countdown si está activo
    if (zonas[i].estaActivo && zonas[i].tiempoEncendido > 0)
    {
      unsigned long tiempoRestante = 0;

      if (estaEnHorarioLaboral)
      {
        // En horario laboral: countdown desde activación
        unsigned long tiempoTranscurrido = millis() - zonas[i].tiempoEncendido;
        tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO > tiempoTranscurrido) ? (TIEMPO_MAXIMO_ENCENDIDO - tiempoTranscurrido) / 1000 : 0;
      }
      else
      {
        // Fuera de horario: verificar movimiento global
        bool hayMovimientoGlobal = false;
        for (int j = 0; j < 2; j++)
        {
          if ((millis() - zonas[j].ultimoMovimiento) <= 30000)
          {
            hayMovimientoGlobal = true;
            break;
          }
        }

        if (!hayMovimientoGlobal)
        {
          tiempoRestante = 0; // Se apagará inmediatamente
        }
        else
        {
          // Hay movimiento: countdown desde activación
          unsigned long tiempoTranscurrido = millis() - zonas[i].tiempoEncendido;
          tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO > tiempoTranscurrido) ? (TIEMPO_MAXIMO_ENCENDIDO - tiempoTranscurrido) / 1000 : 0;
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
  
  // Debug: mostrar datos enviados
  Serial.printf("WebSocket enviado - Modo: %s, Hora: %02d:%02d:%02d\n", 
    estaEnHorarioLaboral ? "Laboral" : "Fuera", horaActual, minutoActual, segundoActual);
  for (int i = 0; i < 2; i++) {
    unsigned long tiempoDesdeMovimiento = 0;
    if (zonas[i].ultimoMovimiento > 0) {
      tiempoDesdeMovimiento = (millis() - zonas[i].ultimoMovimiento) / 1000;
    } else {
      tiempoDesdeMovimiento = 999999;
    }
    Serial.printf("  Zona %d: activo=%s, movimiento=%lus, PIR=%d\n", 
      i+1, zonas[i].estaActivo ? "SI" : "NO", tiempoDesdeMovimiento, digitalRead(zonas[i].pinPir));
  }
  
  // Debug: mostrar datos enviados
  Serial.println("Datos WebSocket enviados:");
  Serial.printf("Modo: %s (%s)\n", 
    estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario",
    estaEnHorarioLaboral ? "true" : "false");
  for (int i = 0; i < 2; i++) {
    Serial.printf("Zona %d: Activo=%s, UltimoMov=%lus, SensorPIR=%d\n", 
      i+1, 
      zonas[i].estaActivo ? "SI" : "NO",
      zonas[i].ultimoMovimiento > 0 ? (millis() - zonas[i].ultimoMovimiento) / 1000 : 0,
      digitalRead(zonas[i].pinPir));
  }
}

// Interrupciones seguras
void IRAM_ATTR manejarPIR1()
{
  if (estaEnHorarioLaboral)
    return; // Solo procesar fuera de horario laboral

  pir1Disparado = true;
  tiempoPir1 = millis();
}

void IRAM_ATTR manejarPIR2()
{
  if (estaEnHorarioLaboral)
    return; // Solo procesar fuera de horario laboral

  pir2Disparado = true;
  tiempoPir2 = millis();
}

// Procesar interrupciones en el loop principal
void procesarInterrupcionesPIR()
{
  if (estaEnHorarioLaboral)
  {
    pir1Disparado = false;
    pir2Disparado = false;
    return; // No procesar interrupciones en horario laboral
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

// Actualizar reloj interno
void actualizarRelojInterno()
{
  unsigned long tiempoActual = millis();
  unsigned long segundosTranscurridos = (tiempoActual - referenciaDelTiempo) / 1000;

  if (segundosTranscurridos > 0)
  {
    segundoActual += segundosTranscurridos;
    // mostramo la hora actual
    Serial.printf("Hora actual: %02d:%02d:%02d\n", horaActual, minutoActual, segundoActual);
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
    referenciaDelTiempo = tiempoActual;
  }
}

// Establecer estado de una zona
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

// Manejar apagado automático
void controlarApagadoAutomatico()
{
  unsigned long tiempoActual = millis();

  if (!estaEnHorarioLaboral)
  {
    // FUERA DE HORARIO: Lógica principal de control
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
          Serial.printf("Zona %d apagada por falta de movimiento\n", i + 1);
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
            Serial.printf("Zona %d apagada por timeout máximo (5 min)\n", i + 1);
          }
        }
      }
    }
  }
}

// Verificar si estamos en horario laboral
bool verificarSiEsHorarioLaboral()
{
  char cadenaHoraActual[6];
  sprintf(cadenaHoraActual, "%02d:%02d", horaActual, minutoActual);

  for (int i = 0; i < cantidadHorarios; i++)
  {
    if (String(cadenaHoraActual) >= horariosLaborales[i][0] && String(cadenaHoraActual) <= horariosLaborales[i][1])
    {
      return true;
    }
  }
  return false;
}

// Handler para página principal - VERSIÓN OPTIMIZADA PARA MÓVILES
void manejarPaginaPrincipal()
{
  char cadenaHoraActual[9];
  sprintf(cadenaHoraActual, "%02d:%02d:%02d", horaActual, minutoActual, segundoActual);

  // Obtener valores actuales de horariosLaborales
  String inicio0 = horariosLaborales[0][0];
  String fin0 = horariosLaborales[0][1];
  String inicio1 = horariosLaborales[1][0];
  String fin1 = horariosLaborales[1][1];

  String html = "<!DOCTYPE html>\
<html lang=\"es\">\
<head>\
    <meta charset=\"UTF-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no\">\
    <title>Control de Luces Inteligente</title>\
    <style>\
        :root {\
            --primary: #4361ee;\
            --secondary: #3f37c9;\
            --success: #4cc9f0;\
            --danger: #f72585;\
            --warning: #f8961e;\
            --dark: #212529;\
            --light: #f8f9fa;\
        }\
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; touch-action: manipulation; }\
        body { background-color: #f5f7fa; color: #333; line-height: 1.6; padding: 10px; font-size: 14px; }\
        .container { max-width: 100%; margin: 0 auto; display: grid; grid-template-columns: 1fr; gap: 15px; }\
        .card { background: white; border-radius: 10px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); padding: 15px; transition: transform 0.3s ease; }\
        .card-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; padding-bottom: 8px; border-bottom: 1px solid #eee; }\
        .card-title { font-size: 1.1rem; font-weight: 600; color: var(--primary); }\
        .clock-container { text-align: center; padding: 12px; background: linear-gradient(135deg, #4361ee, #3a0ca3); color: white; border-radius: 8px; margin-bottom: 15px; }\
        .mode-banner { text-align: center; padding: 15px; border-radius: 8px; margin-bottom: 15px; font-size: 1.3rem; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; transition: all 0.3s ease; }\
        .mode-horario { background: linear-gradient(135deg, #28a745, #20c997); color: white; border: 2px solid #1e7e34; }\
        .mode-fuera-horario { background: linear-gradient(135deg, #dc3545, #fd7e14); color: white; border: 2px solid #721c24; animation: pulse 2s infinite; }\
        @keyframes pulse { 0% { transform: scale(1); } 50% { transform: scale(1.02); } 100% { transform: scale(1); } }\
        #real-time-clock { font-size: 2.2rem; font-weight: 700; letter-spacing: 1px; font-family: 'Courier New', monospace; }\
        .mode-indicator { font-size: 1rem; margin-top: 5px; }\
        .grid-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }\
        .zone-card { text-align: center; padding: 12px; border-radius: 8px; background-color: #f8f9fa; position: relative; overflow: hidden; }\
        .zone-title { font-weight: 600; margin-bottom: 8px; font-size: 1rem; }\
        .zone-status { font-size: 1rem; font-weight: 700; margin-bottom: 8px; }\
        .sensor-indicator { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 4px; }\
        .sensor-active { background-color: #4cc9f0; box-shadow: 0 0 6px #4cc9f0; }\
        .sensor-inactive { background-color: #adb5bd; }\
        .btn { display: inline-block; padding: 8px 12px; border: none; border-radius: 5px; cursor: pointer; font-weight: 500; transition: all 0.3s; text-decoration: none; font-size: 0.9rem; }\
        .btn-primary { background-color: var(--primary); color: white; }\
        .btn-danger { background-color: var(--danger); color: white; }\
        .form-group { margin-bottom: 12px; }\
        .form-row { display: flex; gap: 8px; }\
        input[type='time'], input[type='text'] { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 5px; font-size: 0.9rem; }\
        .connection-status { display: flex; align-items: center; justify-content: center; margin-top: 5px; font-size: 0.8rem; }\
        .connection-dot { width: 10px; height: 10px; border-radius: 50%; margin-right: 5px; }\
        .connected { background-color: #4cc9f0; }\
        .disconnected { background-color: #f72585; }\
        .switch { position: relative; display: inline-block; width: 60px; height: 34px; margin-top: 8px; }\
        .switch input { opacity: 0; width: 0; height: 0; }\
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }\
        .slider:before { position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }\
        input:checked + .slider { background-color: var(--success); }\
        input:checked + .slider:before { transform: translateX(26px); }\
        .countdown-display { font-size: 0.8rem; color: var(--warning); margin-top: 5px; font-weight: 600; }\
    </style>\
</head>\
<body>\
    <div class=\"container\">\
        <div class=\"card\">\
            <div class=\"clock-container\">\
                <div id=\"real-time-clock\">--:--:--</div>\
                <div class=\"mode-indicator\" id=\"mode-indicator\">Modo: Cargando...</div>\
                <div class=\"connection-status\">\
                    <div class=\"connection-dot disconnected\" id=\"connection-dot\"></div>\
                    <span id=\"connection-status\">Desconectado</span>\
                </div>\
            </div>\
            <div class=\"mode-banner mode-horario\" id=\"mode-banner\">\
                🕐 HORARIO LABORAL ACTIVO\
            </div>\
            <div class=\"grid-2\">\
                <div class=\"zone-card\" id=\"zone-1-card\">\
                    <div class=\"zone-title\">Zona 1</div>\
                    <div class=\"zone-status\" id=\"zone-1-status\">APAGADO</div>\
                    <div><span class=\"sensor-indicator\" id=\"zone-1-sensor\"></span> Sensor de movimiento</div>\
                    <div class=\"actions\">\
                        <label class=\"switch\">\
                            <input type=\"checkbox\" id=\"zone-1-switch\" onchange=\"toggleZone(0, this.checked)\">\
                            <span class=\"slider\"></span>\
                        </label>\
                        <div class=\"countdown-display\" id=\"zone-1-countdown\" style=\"display: none;\"></div>\
                    </div>\
                </div>\
                <div class=\"zone-card\" id=\"zone-2-card\">\
                    <div class=\"zone-title\">Zona 2</div>\
                    <div class=\"zone-status\" id=\"zone-2-status\">APAGADO</div>\
                    <div><span class=\"sensor-indicator\" id=\"zone-2-sensor\"></span> Sensor de movimiento</div>\
                    <div class=\"actions\">\
                        <label class=\"switch\">\
                            <input type=\"checkbox\" id=\"zone-2-switch\" onchange=\"toggleZone(1, this.checked)\">\
                            <span class=\"slider\"></span>\
                        </label>\
                        <div class=\"countdown-display\" id=\"zone-2-countdown\" style=\"display: none;\"></div>\
                    </div>\
                </div>\
            </div>\
        </div>\
        <div class=\"card\">\
            <div class=\"card-header\">\
                <div class=\"card-title\">Estado de Sensores de Movimiento</div>\
            </div>\
            <div style=\"padding: 10px;\">\
                <p id=\"movimiento-zona-1\" style=\"margin: 8px 0; padding: 8px; border-radius: 5px; background-color: #f8f9fa;\">Zona 1: Sin movimiento</p>\
                <p id=\"movimiento-zona-2\" style=\"margin: 8px 0; padding: 8px; border-radius: 5px; background-color: #f8f9fa;\">Zona 2: Sin movimiento</p>\
            </div>\
        </div>\
        <div class=\"card\">\
            <div class=\"card-header\">\
                <div class=\"card-title\">Configuración</div>\
            </div>\
            <form action=\"/update\" method=\"post\" id=\"schedule-form\">\
                <div class=\"form-row\">\
                    <div class=\"form-group\">\
                        <label>Horario 1:</label>\
                        <input type=\"time\" name=\"inicio0\" value=\"" +
                inicio0 + "\">\
                        <input type=\"time\" name=\"fin0\" value=\"" +
                fin0 + "\" style=\"margin-top: 5px;\">\
                    </div>\
                    <div class=\"form-group\">\
                        <label>Horario 2:</label>\
                        <input type=\"time\" name=\"inicio1\" value=\"" +
                inicio1 + "\">\
                        <input type=\"time\" name=\"fin1\" value=\"" +
                fin1 + "\" style=\"margin-top: 5px;\">\
                    </div>\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\" style=\"width: 100%; margin-top: 10px;\">Actualizar Horarios</button>\
            </form>\
            <form action=\"/settime\" method=\"post\" style=\"margin-top: 15px;\">\
                <div class=\"form-group\">\
                    <label>Hora actual (HH:MM):</label>\
                    <input type=\"text\" name=\"time\" id=\"manual-time\" placeholder=\"HH:MM\">\
                </div>\
                <button type=\"submit\" class=\"btn btn-primary\" style=\"width: 100%;\">Establecer Hora</button>\
            </form>\
        </div>\
    </div>\
    <script>\
        const connectionDot = document.getElementById('connection-dot');\
        const connectionStatus = document.getElementById('connection-status');\
        let socket;\
        const host = window.location.hostname;\
        function initWebSocket() {\
            socket = new WebSocket(`ws://${host}:81/`);\
            socket.addEventListener('open', () => {\
                connectionDot.classList.remove('disconnected');\
                connectionDot.classList.add('connected');\
                connectionStatus.textContent = 'Conectado';\
                syncTimeAutomatically();\
            });\
            socket.addEventListener('message', (event) => {\
                const data = JSON.parse(event.data);\
                \
                // Actualizar reloj con hora del ESP32\
                document.getElementById('real-time-clock').textContent = `${String(data.hora).padStart(2, '0')}:${String(data.minuto).padStart(2, '0')}:${String(data.segundo).padStart(2, '0')}`;\
                document.getElementById('mode-indicator').textContent = `Modo: ${data.modo}`;\
                \
                // Actualizar banner de modo\
                const modeBanner = document.getElementById('mode-banner');\
                const isHorarioLaboral = data.modoActivo === true;\
                \
                modeBanner.className = `mode-banner ${isHorarioLaboral ? 'mode-horario' : 'mode-fuera-horario'}`;\
                modeBanner.innerHTML = isHorarioLaboral ? \
                    '🕐 HORARIO LABORAL ACTIVO<br><small>Sensores desactivados</small>' : \
                    '🚨 FUERA DE HORARIO<br><small>Sensores de seguridad activos</small>';\
                \
                // Actualizar estado de cada zona\
                data.zonas.forEach((zona, index) => {\
                    const idx = index + 1;\
                    \
                    // Estado encendido/apagado\
                    document.getElementById(`zone-${idx}-status`).textContent = zona.activo ? 'ENCENDIDO' : 'APAGADO';\
                    document.getElementById(`zone-${idx}-status`).style.color = zona.activo ? '#4cc9f0' : '#f72585';\
                    \
                    // Actualizar switch sin disparar evento\
                    const switchElement = document.getElementById(`zone-${idx}-switch`);\
                    switchElement.onchange = null;\
                    switchElement.checked = zona.activo;\
                    switchElement.onchange = function() { toggleZone(index, this.checked); };\
                    \
                    // Actualizar countdown\
                    if (zona.countdown && zona.countdown > 0) {\
                        const countdownElement = document.getElementById(`zone-${idx}-countdown`);\
                        countdownElement.style.display = 'block';\
                        const minutes = Math.floor(zona.countdown / 60);\
                        const seconds = zona.countdown % 60;\
                        countdownElement.textContent = `Apagado en: ${minutes}:${String(seconds).padStart(2, '0')}`;\
                    } else {\
                        document.getElementById(`zone-${idx}-countdown`).style.display = 'none';\
                    }\
                    \
                    // Indicador visual del sensor (basado en tiempo desde último movimiento)\
                    const tiempoSinMovimiento = zona.movimiento; // segundos desde último movimiento\
                    const sensorActivo = tiempoSinMovimiento < 10; // Activo si hubo movimiento en últimos 10 segundos\
                    const sensor = document.getElementById(`zone-${idx}-sensor`);\
                    sensor.classList.toggle('sensor-active', sensorActivo);\
                    sensor.classList.toggle('sensor-inactive', !sensorActivo);\
                    \
                    // Actualizar estado de movimiento\
                    const movimientoElement = document.getElementById(`movimiento-zona-${idx}`);\
                    if (isHorarioLaboral) {\
                        movimientoElement.textContent = `Zona ${idx}: Sensor desactivado (horario laboral)`;\
                        movimientoElement.style.color = '#6c757d';\
                        movimientoElement.style.fontWeight = 'normal';\
                    } else {\
                        // Mostrar tiempo desde último movimiento\
                        let textoMovimiento;\
                        if (tiempoSinMovimiento >= 999999) {\
                            textoMovimiento = 'Sin movimiento registrado';\
                        } else if (tiempoSinMovimiento < 60) {\
                            textoMovimiento = `Último movimiento: hace ${tiempoSinMovimiento}s`;\
                        } else {\
                            const minutos = Math.floor(tiempoSinMovimiento / 60);\
                            const segundos = tiempoSinMovimiento % 60;\
                            textoMovimiento = `Último movimiento: hace ${minutos}m ${segundos}s`;\
                        }\
                        \
                        movimientoElement.textContent = `Zona ${idx}: ${textoMovimiento}`;\
                        \
                        // Color según qué tan reciente fue el movimiento\
                        if (tiempoSinMovimiento < 10) {\
                            movimientoElement.style.color = '#dc3545'; // Rojo: movimiento muy reciente\
                            movimientoElement.style.fontWeight = 'bold';\
                        } else if (tiempoSinMovimiento < 30) {\
                            movimientoElement.style.color = '#f8961e'; // Naranja: movimiento reciente\
                            movimientoElement.style.fontWeight = '600';\
                        } else {\
                            movimientoElement.style.color = '#28a745'; // Verde: sin movimiento\
                            movimientoElement.style.fontWeight = 'normal';\
                        }\
                    }\
                });\
            });\
            socket.addEventListener('close', () => {\
                connectionDot.classList.remove('connected');\
                connectionDot.classList.add('disconnected');\
                connectionStatus.textContent = 'Desconectado, reconectando...';\
                setTimeout(initWebSocket, 3000);\
            });\
            socket.addEventListener('error', () => socket.close());\
        }\
        document.addEventListener('DOMContentLoaded', () => {\
            const now = new Date();\
            document.getElementById('manual-time').value = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;\
            initWebSocket();\
        });\
        function toggleZone(zona, estado) {\
            const url = estado ? `/on?zona=${zona}` : `/off?zona=${zona}`;\
            fetch(url)\
                .then(response => {\
                    if (!response.ok) {\
                        console.error('Error al cambiar estado de zona');\
                    }\
                })\
                .catch(err => {\
                    console.error('Error:', err);\
                });\
        }\
        function syncTimeAutomatically() {\
            const now = new Date();\
            const timeString = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;\
            fetch('/settime', {\
                method: 'POST',\
                headers: {\
                    'Content-Type': 'application/x-www-form-urlencoded',\
                },\
                body: `time=${timeString}`\
            }).then(() => {\
                console.log('Hora sincronizada automáticamente:', timeString);\
            }).catch(err => {\
                console.error('Error sincronizando hora:', err);\
            });\
        }\
    </script>\
</body>\
</html>";

  servidor.send(200, "text/html", html);
}

// Configurar hora manual
void manejarConfiguracionHora()
{
  if (servidor.hasArg("time"))
  {
    String cadenaHora = servidor.arg("time");
    int hora, minuto;
    if (sscanf(cadenaHora.c_str(), "%d:%d", &hora, &minuto) == 2)
    {
      horaActual = hora;
      minutoActual = minuto;
      segundoActual = 0;
      referenciaDelTiempo = millis();
      Serial.printf("Hora actualizada: %02d:%02d\n", hora, minuto);
    }
  }
  servidor.sendHeader("Location", "/");
  servidor.send(303);
}

// Control manual de zonas
void manejarControlManual()
{
  if (servidor.hasArg("zona"))
  {
    int indiceZona = servidor.arg("zona").toInt();
    if (indiceZona >= 0 && indiceZona < 2)
    {
      bool encender = (servidor.uri() == "/on");

      if (encender)
      {
        configurarEstadoZona(indiceZona, true);

        // Establecer tiempo de movimiento para mantener la luz
        zonas[indiceZona].ultimoMovimiento = millis();

        if (!estaEnHorarioLaboral)
        {
          Serial.printf("Zona %d encendida manualmente (fuera de horario)\n", indiceZona + 1);
        }
        else
        {
          Serial.printf("Zona %d encendida manualmente (horario laboral)\n", indiceZona + 1);
        }
      }
      else
      {
        configurarEstadoZona(indiceZona, false);
        Serial.printf("Zona %d apagada manualmente\n", indiceZona + 1);
      }
    }
  }
  servidor.sendHeader("Location", "/");
  servidor.send(303);
}

// Actualizar horarios
void manejarActualizacionHorarios()
{
  for (int i = 0; i < cantidadHorarios; i++)
  {
    String claveInicio = "inicio" + String(i);
    String claveFin = "fin" + String(i);
    if (servidor.hasArg(claveInicio) && servidor.hasArg(claveFin))
    {
      horariosLaborales[i][0] = servidor.arg(claveInicio);
      horariosLaborales[i][1] = servidor.arg(claveFin);
      Serial.printf("Horario actualizado: %s - %s\n",
                    horariosLaborales[i][0].c_str(), horariosLaborales[i][1].c_str());
    }
  }
  servidor.sendHeader("Location", "/");
  servidor.send(303);
}

// Handler para rutas no encontradas
void manejarPaginaNoEncontrada()
{
  servidor.send(404, "text/plain", "Pagina no encontrada");
}