#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

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
  bool controlManual;
  String nombre;
  int historialActividad[60]; // Historial de últimos 60 segundos
  int punteroActividad;
  // constructor para inicializar la zona
  Zona(int pir, int relay1, int relay2, String nombre)
  {
    pinPir = pir;
    pinesRelay[0] = relay1;
    pinesRelay[1] = relay2;
    ultimoMovimiento = 0;
    tiempoEncendido = 0;
    estaActivo = false;
    controlManual = false;
    this->nombre = nombre;
    punteroActividad = 0;
    for (int i = 0; i < 60; i++)
    {
      historialActividad[i] = 0;
    }
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
int horaActual = 8;                        // Hora inicial: 8 AM
int minutoActual = 0;                      // Minutos iniciales
int segundoActual = 0;                     // Segundos iniciales
unsigned long ultimaActualizacionTiempo = 0; // Para llevar el tiempo transcurrido

// Prototipos de funciones
void IRAM_ATTR manejarPIR1();
void IRAM_ATTR manejarPIR2();
void procesarInterrupcionesPIR();
void configurarEstadoZona(int indiceZona, bool activar);
void controlarApagadoAutomatico();
bool verificarSiEsHorarioLaboral();
void actualizarRelojInterno();
void manejarPaginaPrincipal();
void manejarControlManual();
void manejarActualizacionHorarios();
void manejarConfiguracionHora();
void manejarPaginaNoEncontrada();
void eventoSocketWeb(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void enviarEstadoPorSocketWeb();
void actualizarHistorialActividad();

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
    // Inicializar historial de actividad
    for (int k = 0; k < 60; k++)
    {
      zonas[i].historialActividad[k] = 0;
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
  ultimaActualizacionTiempo = millis();

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
}

void loop()
{
  servidor.handleClient();
  socketWeb.loop();
  actualizarRelojInterno();
  
  // Actualizar modo horario SIN resetear estados
  bool nuevoModoHorario = verificarSiEsHorarioLaboral();
  if (estaEnHorarioLaboral != nuevoModoHorario) {
    estaEnHorarioLaboral = nuevoModoHorario;
    Serial.printf("Modo cambiado a: %s\n", estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario");
    
    // Al cambiar a fuera de horario, actualizar ultimoMovimiento para todas las zonas activas
    if (!estaEnHorarioLaboral) {
      for (int i = 0; i < 2; i++) {
        if (zonas[i].estaActivo) {
          zonas[i].ultimoMovimiento = millis(); // Dar tiempo de gracia
          Serial.printf("Zona %d: Tiempo de movimiento actualizado por cambio de modo\n", i + 1);
        }
      }
    }
    
    enviarEstadoPorSocketWeb(); // Enviar inmediatamente cuando cambie el modo
  }
  
  procesarInterrupcionesPIR();
  controlarApagadoAutomatico();
  actualizarHistorialActividad();

  // Verificar cambios en sensores para envío inmediato
  bool sensoresCambiaron = false;
  for (int i = 0; i < 2; i++) {
    bool estadoActualSensor = digitalRead(zonas[i].pinPir);
    if (estadoActualSensor != estadoSensoresAnterior[i]) {
      estadoSensoresAnterior[i] = estadoActualSensor;
      sensoresCambiaron = true;
    }
  }
  
  // Enviar estado cada segundo o cuando cambien los sensores
  static unsigned long ultimaActualizacion = 0;
  if (millis() - ultimaActualizacion > 1000 || sensoresCambiaron)
  {
    enviarEstadoPorSocketWeb();
    ultimaActualizacion = millis();
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
  DynamicJsonDocument documento(1024);

  // Hora actual
  documento["hora"] = horaActual;
  documento["minuto"] = minutoActual;
  documento["segundo"] = segundoActual;

  // Modo de operación
  documento["modo"] = estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario";

  // Estado de zonas
  JsonArray arregloZonas = documento.createNestedArray("zonas");
  for (int i = 0; i < 2; i++)
  {
    JsonObject objetoZona = arregloZonas.createNestedObject();
    objetoZona["nombre"] = zonas[i].nombre;
    objetoZona["activo"] = zonas[i].estaActivo;
    objetoZona["manual"] = zonas[i].controlManual;
    objetoZona["movimiento"] = digitalRead(zonas[i].pinPir); // Estado actual del sensor

    // Calcular countdown si está activo
    if (zonas[i].estaActivo && zonas[i].tiempoEncendido > 0) {
      unsigned long tiempoRestante = 0;
      
      if (estaEnHorarioLaboral) {
        // En horario laboral: countdown desde activación
        unsigned long tiempoTranscurrido = millis() - zonas[i].tiempoEncendido;
        tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO > tiempoTranscurrido) ? (TIEMPO_MAXIMO_ENCENDIDO - tiempoTranscurrido) / 1000 : 0;
      } else {
        // Fuera de horario: verificar movimiento global
        bool hayMovimientoGlobal = false;
        for (int j = 0; j < 2; j++) {
          if ((millis() - zonas[j].ultimoMovimiento) <= 30000) {
            hayMovimientoGlobal = true;
            break;
          }
        }
        
        if (!hayMovimientoGlobal) {
          tiempoRestante = 0; // Se apagará inmediatamente
        } else {
          // Hay movimiento: countdown desde activación
          unsigned long tiempoTranscurrido = millis() - zonas[i].tiempoEncendido;
          tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO > tiempoTranscurrido) ? (TIEMPO_MAXIMO_ENCENDIDO - tiempoTranscurrido) / 1000 : 0;
        }
      }
      
      objetoZona["countdown"] = tiempoRestante;
    } else {
      objetoZona["countdown"] = 0;
    }

    // Historial de actividad
    JsonArray actividadArray = objetoZona.createNestedArray("actividad");
    for (int j = 0; j < 60; j++)
    {
      actividadArray.add(zonas[i].historialActividad[j]);
    }
  }

  String cadenaJson;
  serializeJson(documento, cadenaJson);
  socketWeb.broadcastTXT(cadenaJson);
}

// Actualizar historial de actividad
void actualizarHistorialActividad()
{
  static unsigned long ultimaActualizacion = 0;
  if (millis() - ultimaActualizacion > 1000)
  {
    for (int i = 0; i < 2; i++)
    {
      zonas[i].punteroActividad = (zonas[i].punteroActividad + 1) % 60;
      zonas[i].historialActividad[zonas[i].punteroActividad] = digitalRead(zonas[i].pinPir);
    }
    ultimaActualizacion = millis();
  }
}

// Interrupciones seguras
void IRAM_ATTR manejarPIR1()
{
  if (estaEnHorarioLaboral) return; // Solo procesar fuera de horario laboral
  
  pir1Disparado = true;
  tiempoPir1 = millis();
  Serial.println("PIR1 detectado (fuera de horario)");
}

void IRAM_ATTR manejarPIR2()
{
  if (estaEnHorarioLaboral) return; // Solo procesar fuera de horario laboral
  
  pir2Disparado = true;
  tiempoPir2 = millis();
  Serial.println("PIR2 detectado (fuera de horario)");
}

// Procesar interrupciones en el loop principal
void procesarInterrupcionesPIR()
{
  if (pir1Disparado)
  {
    pir1Disparado = false;
    if (!estaEnHorarioLaboral) // Solo procesar fuera de horario
    {
      zonas[0].ultimoMovimiento = tiempoPir1;
      Serial.println("Zona 1: Movimiento detectado - actualizando temporizador");
    }
  }

  if (pir2Disparado)
  {
    pir2Disparado = false;
    if (!estaEnHorarioLaboral) // Solo procesar fuera de horario
    {
      zonas[1].ultimoMovimiento = tiempoPir2;
      Serial.println("Zona 2: Movimiento detectado - actualizando temporizador");
    }
  }
}

// Actualizar reloj interno
void actualizarRelojInterno()
{
  unsigned long tiempoActual = millis();
  unsigned long segundosTranscurridos = (tiempoActual - ultimaActualizacionTiempo) / 1000;

  if (segundosTranscurridos > 0)
  {
    segundoActual += segundosTranscurridos;

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
    ultimaActualizacionTiempo = tiempoActual;
  }
}

// Establecer estado de una zona
void configurarEstadoZona(int indiceZona, bool activar)
{
  zonas[indiceZona].estaActivo = activar;
  if (activar) {
    zonas[indiceZona].tiempoEncendido = millis();
  } else {
    zonas[indiceZona].tiempoEncendido = 0;
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

  if (estaEnHorarioLaboral)
  {
    // En horario laboral: control manual normal con temporizador
    for (int i = 0; i < 2; i++)
    {
      if (zonas[i].estaActivo && zonas[i].tiempoEncendido > 0)
      {
        unsigned long tiempoEncendido = tiempoActual - zonas[i].tiempoEncendido;
        if (tiempoEncendido > TIEMPO_MAXIMO_ENCENDIDO)
        {
          configurarEstadoZona(i, false);
          zonas[i].controlManual = false;
          Serial.printf("Zona %d apagada por timeout (horario laboral)\n", i + 1);
        }
      }
    }
  }
  else
  {
    // FUERA DE HORARIO: Lógica principal de control
    bool hayMovimientoGlobal = false;
    
    // Verificar si hay movimiento reciente en cualquier zona
    for (int i = 0; i < 2; i++) {
      if ((tiempoActual - zonas[i].ultimoMovimiento) <= 30000) { // 30 segundos de gracia
        hayMovimientoGlobal = true;
        break;
      }
    }
    
    // Si NO hay movimiento global, apagar TODAS las zonas
    if (!hayMovimientoGlobal) {
      bool algunaZonaApagada = false;
      for (int i = 0; i < 2; i++) {
        if (zonas[i].estaActivo) {
          configurarEstadoZona(i, false);
          zonas[i].controlManual = false;
          algunaZonaApagada = true;
        }
      }
      if (algunaZonaApagada) {
        Serial.println("TODAS las zonas apagadas - Sin movimiento fuera de horario");
      }
    }
    else {
      // HAY movimiento: mantener countdown individual
      for (int i = 0; i < 2; i++) {
        if (zonas[i].estaActivo && zonas[i].tiempoEncendido > 0) {
          unsigned long tiempoEncendido = tiempoActual - zonas[i].tiempoEncendido;
          
          // Apagar si excede el tiempo máximo (5 minutos)
          if (tiempoEncendido > TIEMPO_MAXIMO_ENCENDIDO) {
            configurarEstadoZona(i, false);
            zonas[i].controlManual = false;
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

  String html = R"=====(<!DOCTYPE html>
<html lang="es">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Control de Luces Inteligente</title>
    <style>
        :root {
            --primary: #4361ee;
            --secondary: #3f37c9;
            --success: #4cc9f0;
            --danger: #f72585;
            --warning: #f8961e;
            --dark: #212529;
            --light: #f8f9fa;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            touch-action: manipulation;
        }

        body {
            background-color: #f5f7fa;
            color: #333;
            line-height: 1.6;
            padding: 10px;
            font-size: 14px;
        }

        .container {
            max-width: 100%;
            margin: 0 auto;
            display: grid;
            grid-template-columns: 1fr;
            gap: 15px;
        }

        .card {
            background: white;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
            padding: 15px;
            transition: transform 0.3s ease;
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
            padding-bottom: 8px;
            border-bottom: 1px solid #eee;
        }

        .card-title {
            font-size: 1.1rem;
            font-weight: 600;
            color: var(--primary);
        }

        .clock-container {
            text-align: center;
            padding: 12px;
            background: linear-gradient(135deg, #4361ee, #3a0ca3);
            color: white;
            border-radius: 8px;
            margin-bottom: 15px;
        }

        .mode-banner {
            text-align: center;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 15px;
            font-size: 1.3rem;
            font-weight: 700;
            text-transform: uppercase;
            letter-spacing: 1px;
            transition: all 0.3s ease;
        }

        .mode-horario {
            background: linear-gradient(135deg, #28a745, #20c997);
            color: white;
            border: 2px solid #1e7e34;
        }

        .mode-fuera-horario {
            background: linear-gradient(135deg, #dc3545, #fd7e14);
            color: white;
            border: 2px solid #721c24;
            animation: pulse 2s infinite;
        }

        @keyframes pulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.02); }
            100% { transform: scale(1); }
        }

        #real-time-clock {
            font-size: 2.2rem;
            font-weight: 700;
            letter-spacing: 1px;
            font-family: 'Courier New', monospace;
        }

        .mode-indicator {
            font-size: 1rem;
            margin-top: 5px;
        }

        .grid-2 {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
        }

        .zone-card {
            text-align: center;
            padding: 12px;
            border-radius: 8px;
            background-color: #f8f9fa;
            position: relative;
            overflow: hidden;
        }

        .zone-title {
            font-weight: 600;
            margin-bottom: 8px;
            font-size: 1rem;
        }

        .zone-status {
            font-size: 1rem;
            font-weight: 700;
            margin-bottom: 8px;
        }

        .sensor-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 4px;
        }

        .sensor-active {
            background-color: #4cc9f0;
            box-shadow: 0 0 6px #4cc9f0;
        }

        .sensor-inactive {
            background-color: #adb5bd;
        }

        .btn {
            display: inline-block;
            padding: 8px 12px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.3s;
            text-decoration: none;
            font-size: 0.9rem;
        }

        .btn-primary {
            background-color: var(--primary);
            color: white;
        }

        .btn-danger {
            background-color: var(--danger);
            color: white;
        }

        .form-group {
            margin-bottom: 12px;
        }

        .form-row {
            display: flex;
            gap: 8px;
        }

        input[type='time'],
        input[type='text'] {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 5px;
            font-size: 0.9rem;
        }

        .connection-status {
            display: flex;
            align-items: center;
            justify-content: center;
            margin-top: 5px;
            font-size: 0.8rem;
        }

        .connection-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 5px;
        }

        .connected {
            background-color: #4cc9f0;
        }

        .disconnected {
            background-color: #f72585;
        }

        /* Estilos para el switch */
        .switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
            margin-top: 8px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: var(--success);
        }

        input:checked + .slider:before {
            transform: translateX(26px);
        }

        .countdown-display {
            font-size: 0.8rem;
            color: var(--warning);
            margin-top: 5px;
            font-weight: 600;
        }
    </style>
</head>

<body>
    <div class="container">
        <div class="card">
            <div class="clock-container">
                <div id="real-time-clock">--:--:--</div>
                <div class="mode-indicator" id="mode-indicator">Modo: Cargando...</div>
                <div class="connection-status">
                    <div class="connection-dot disconnected" id="connection-dot"></div>
                    <span id="connection-status">Desconectado</span>
                </div>
            </div>

            <div class="mode-banner mode-horario" id="mode-banner">
                🕐 HORARIO LABORAL ACTIVO
            </div>

            <div class="grid-2">
                <div class="zone-card" id="zone-1-card">
                    <div class="zone-title">Zona 1</div>
                    <div class="zone-status" id="zone-1-status">APAGADO</div>
                    <div><span class="sensor-indicator" id="zone-1-sensor"></span> Sensor de movimiento</div>
                    <div class="actions">
                        <label class="switch">
                            <input type="checkbox" id="zone-1-switch" onchange="toggleZone(0, this.checked)">
                            <span class="slider"></span>
                        </label>
                        <div class="countdown-display" id="zone-1-countdown" style="display: none;"></div>
                    </div>
                </div>

                <div class="zone-card" id="zone-2-card">
                    <div class="zone-title">Zona 2</div>
                    <div class="zone-status" id="zone-2-status">APAGADO</div>
                    <div><span class="sensor-indicator" id="zone-2-sensor"></span> Sensor de movimiento</div>
                    <div class="actions">
                        <label class="switch">
                            <input type="checkbox" id="zone-2-switch" onchange="toggleZone(1, this.checked)">
                            <span class="slider"></span>
                        </label>
                        <div class="countdown-display" id="zone-2-countdown" style="display: none;"></div>
                    </div>
                </div>
            </div>
        </div>

        <div class="card">
            <div class="card-header">
                <div class="card-title">Estado de Sensores de Movimiento</div>
            </div>
            <div style="padding: 10px;">
                <p id="movimiento-zona-1" style="margin: 8px 0; padding: 8px; border-radius: 5px; background-color: #f8f9fa;">Zona 1: Sin movimiento</p>
                <p id="movimiento-zona-2" style="margin: 8px 0; padding: 8px; border-radius: 5px; background-color: #f8f9fa;">Zona 2: Sin movimiento</p>
            </div>
        </div>

        <div class="card">
            <div class="card-header">
                <div class="card-title">Configuración</div>
            </div>
            <form action="/update" method="post">
                <div class="form-row">
                    <div class="form-group">
                        <label>Horario 1:</label>
                        <input type="time" name="inicio0" value="08:00">
                        <input type="time" name="fin0" value="12:00" style="margin-top: 5px;">
                    </div>
                    <div class="form-group">
                        <label>Horario 2:</label>
                        <input type="time" name="inicio1" value="14:00">
                        <input type="time" name="fin1" value="18:00" style="margin-top: 5px;">
                    </div>
                </div>
                <button type="submit" class="btn btn-primary" style="width: 100%; margin-top: 10px;">Actualizar
                    Horarios</button>
            </form>
            <form action="/settime" method="post" style="margin-top: 15px;">
                <div class="form-group">
                    <label>Hora actual (HH:MM):</label>
                    <input type="text" name="time" id="manual-time" placeholder="HH:MM">
                </div>
                <button type="submit" class="btn btn-primary" style="width: 100%;">Establecer Hora</button>
            </form>
        </div>
    </div>

    <script>
        const connectionDot = document.getElementById('connection-dot');
        const connectionStatus = document.getElementById('connection-status');

        let socket;
        const host = window.location.hostname;

        function initWebSocket() {
            socket = new WebSocket(`ws://${host}:81/`);

            socket.addEventListener('open', () => {
                connectionDot.classList.remove('disconnected');
                connectionDot.classList.add('connected');
                connectionStatus.textContent = 'Conectado';
                
                // Sincronizar hora solo una vez al conectar
                syncTimeAutomatically();
            });

            socket.addEventListener('message', (event) => {
                const data = JSON.parse(event.data);
                
                // Usar solo la hora del ESP32
                document.getElementById('real-time-clock').textContent = `${String(data.hora).padStart(2, '0')}:${String(data.minuto).padStart(2, '0')}:${String(data.segundo).padStart(2, '0')}`;
                document.getElementById('mode-indicator').textContent = `Modo: ${data.modo}`;

                // Actualizar banner de modo
                const modeBanner = document.getElementById('mode-banner');
                const isHorarioLaboral = data.modo === 'Horario Laboral';
                
                modeBanner.className = `mode-banner ${isHorarioLaboral ? 'mode-horario' : 'mode-fuera-horario'}`;
                modeBanner.innerHTML = isHorarioLaboral ? 
                    '🕐 HORARIO LABORAL ACTIVO<br><small>Sensores desactivados</small>' : 
                    '🚨 FUERA DE HORARIO<br><small>Sensores de seguridad activos</small>';

                data.zonas.forEach((zona, index) => {
                    const idx = index + 1;
                    document.getElementById(`zone-${idx}-status`).textContent = zona.activo ? 'ENCENDIDO' : 'APAGADO';
                    document.getElementById(`zone-${idx}-status`).style.color = zona.activo ? '#4cc9f0' : '#f72585';

                    // Actualizar switch sin disparar evento
                    const switchElement = document.getElementById(`zone-${idx}-switch`);
                    switchElement.onchange = null; // Temporalmente desactivar evento
                    switchElement.checked = zona.activo;
                    switchElement.onchange = function() { toggleZone(index, this.checked); }; // Reactivar evento

                    // Actualizar countdown si existe
                    if (zona.countdown && zona.countdown > 0) {
                        const countdownElement = document.getElementById(`zone-${idx}-countdown`);
                        countdownElement.style.display = 'block';
                        const minutes = Math.floor(zona.countdown / 60);
                        const seconds = zona.countdown % 60;
                        countdownElement.textContent = `Apagado en: ${minutes}:${String(seconds).padStart(2, '0')}`;
                    } else {
                        document.getElementById(`zone-${idx}-countdown`).style.display = 'none';
                    }

                    const sensorActivo = zona.movimiento === 1;
                    const sensor = document.getElementById(`zone-${idx}-sensor`);
                    sensor.classList.toggle('sensor-active', sensorActivo);
                    sensor.classList.toggle('sensor-inactive', !sensorActivo);

                    // Actualizar estado de movimiento con información más detallada
                    const movimientoElement = document.getElementById(`movimiento-zona-${idx}`);
                    if (isHorarioLaboral) {
                        movimientoElement.textContent = `Zona ${idx}: Sensor desactivado (horario laboral)`;
                        movimientoElement.style.color = '#6c757d';
                    } else {
                        movimientoElement.textContent = `Zona ${idx}: ` + (sensorActivo ? '🔴 MOVIMIENTO DETECTADO' : '🟢 Sin movimiento');
                        movimientoElement.style.color = sensorActivo ? '#dc3545' : '#28a745';
                        movimientoElement.style.fontWeight = sensorActivo ? 'bold' : 'normal';
                    }
                });
            });

            socket.addEventListener('close', () => {
                connectionDot.classList.remove('connected');
                connectionDot.classList.add('disconnected');
                connectionStatus.textContent = 'Desconectado, reconectando...';
                setTimeout(initWebSocket, 3000);
            });

            socket.addEventListener('error', () => socket.close());
        }

        document.addEventListener('DOMContentLoaded', () => {
            const now = new Date();
            document.getElementById('manual-time').value = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
            
            // Inicializar WebSocket y sincronizar una vez
            initWebSocket();
        });

        function toggleZone(zona, estado) {
            const url = estado ? `/on?zona=${zona}` : `/off?zona=${zona}`;
            fetch(url)
                .then(response => {
                    if (!response.ok) {
                        console.error('Error al cambiar estado de zona');
                    }
                })
                .catch(err => {
                    console.error('Error:', err);
                });
        }

        function syncTimeAutomatically() {
            const now = new Date();
            const timeString = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
            
            fetch('/settime', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `time=${timeString}`
            }).then(() => {
                console.log('Hora sincronizada automáticamente:', timeString);
            }).catch(err => {
                console.error('Error sincronizando hora:', err);
            });
        }
    </script>
</body>

</html>
)=====";

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
      ultimaActualizacionTiempo = millis();
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
      
      if (encender) {
        zonas[indiceZona].controlManual = true;
        configurarEstadoZona(indiceZona, true);
        
        // Establecer tiempo de movimiento para mantener la luz
        zonas[indiceZona].ultimoMovimiento = millis();
        
        if (!estaEnHorarioLaboral) {
          Serial.printf("Zona %d encendida manualmente (fuera de horario)\n", indiceZona + 1);
        } else {
          Serial.printf("Zona %d encendida manualmente (horario laboral)\n", indiceZona + 1);
        }
      } else {
        zonas[indiceZona].controlManual = false;
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