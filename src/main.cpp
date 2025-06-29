#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// Configuración WiFi
const char *ssid = "SistemaDigitales";
const char *password = "12345678";

// Configuración de pines
#define PIR1_PIN 13
#define PIR2_PIN 15

#define RELAY1_PIN 32
#define RELAY2_PIN 25
#define RELAY3_PIN 26
#define RELAY4_PIN 21

// Variables para manejo de interrupciones
volatile bool pir1Triggered = false;
volatile bool pir2Triggered = false;
volatile unsigned long pir1Time = 0;
volatile unsigned long pir2Time = 0;

// Estructura de zonas
struct Zona
{
  int pirPin;
  int relayPines[2];
  unsigned long lastMotion;
  unsigned long encendidoTime; // Tiempo cuando se encendió
  bool activo;
  bool manual;
  String nombre;
  int actividad[60]; // Historial de últimos 60 segundos
  int actividadPtr;
  // constructor para inicializar la zona
  Zona(int pir, int relay1, int relay2, String name)
  {
    pirPin = pir;
    relayPines[0] = relay1;
    relayPines[1] = relay2;
    lastMotion = 0;
    encendidoTime = 0;
    activo = false;
    manual = false;
    nombre = name;
    actividadPtr = 0;
    for (int i = 0; i < 60; i++)
    {
      actividad[i] = 0;
    }
  }
};

Zona zonas[] = {
    Zona(PIR1_PIN, RELAY1_PIN, RELAY2_PIN, String("Zona 1")),
    Zona(PIR2_PIN, RELAY3_PIN, RELAY4_PIN, String("Zona 2"))};

// Horarios laborales
String horarios[][2] = {
    {"08:00", "12:00"},
    {"14:00", "18:00"}};
const int horarioCount = 2;

// Constantes
const unsigned long TIEMPO_APAGADO = 300000; // 5 minutos (300 segundos)
const int VALOR_ENCENDIDO_RELAY = LOW;
const int VALOR_APAGADO_RELAY = HIGH;

// Variables globales
bool modoHorario = true;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Variables para mejorar sincronización WebSocket
unsigned long lastSensorUpdate = 0;
bool estadoSensoresAnterior[2] = {false, false};

// Variables para el reloj manual
int horaManual = 8;                        // Hora inicial: 8 AM
int minutoManual = 0;                      // Minutos iniciales
int segundoManual = 0;                     // Segundos iniciales
unsigned long ultimoTiempoActualizado = 0; // Para llevar el tiempo transcurrido

// Prototipos de funciones
void IRAM_ATTR handlePIR1();
void IRAM_ATTR handlePIR2();
void procesarInterrupciones();
void setZonaActiva(int zonaIndex, bool activa);
void manejarApagadoAutomatico();
bool enHorarioLaboral();
void actualizarRelojInterno();
void handleRoot();
void handleManualControl();
void handleUpdateHorarios();
void handleSetTime();
void handleNotFound();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
void enviarEstadoWebSocket();
void actualizarHistorialActividad();

void setup()
{
  delay(2000); // Estabilización inicial
  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // Inicializar pines
  for (int i = 0; i < 2; i++)
  {
    pinMode(zonas[i].pirPin, INPUT);
    for (int j = 0; j < 2; j++)
    {
      pinMode(zonas[i].relayPines[j], OUTPUT);
      digitalWrite(zonas[i].relayPines[j], VALOR_APAGADO_RELAY);
    }
    // Inicializar historial de actividad
    for (int k = 0; k < 60; k++)
    {
      zonas[i].actividad[k] = 0;
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
  ultimoTiempoActualizado = millis();

  // Configurar interrupciones PIR seguras
  attachInterrupt(digitalPinToInterrupt(PIR1_PIN), handlePIR1, RISING);
  attachInterrupt(digitalPinToInterrupt(PIR2_PIN), handlePIR2, RISING);

  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/on", handleManualControl);
  server.on("/off", handleManualControl);
  server.on("/update", HTTP_POST, handleUpdateHorarios);
  server.on("/settime", HTTP_POST, handleSetTime);
  server.onNotFound(handleNotFound);

  // Iniciar servidor WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.begin();
  Serial.println("Servidor HTTP iniciado");
  Serial.println("Servidor WebSocket iniciado en puerto 81");
}

void loop()
{
  server.handleClient();
  webSocket.loop();
  actualizarRelojInterno();
  
  // Actualizar modo horario
  bool nuevoModoHorario = enHorarioLaboral();
  if (modoHorario != nuevoModoHorario) {
    modoHorario = nuevoModoHorario;
    Serial.printf("Modo cambiado a: %s\n", modoHorario ? "Horario Laboral" : "Fuera de Horario");
    enviarEstadoWebSocket(); // Enviar inmediatamente cuando cambie el modo
  }
  
  procesarInterrupciones();
  manejarApagadoAutomatico();
  actualizarHistorialActividad();

  // Verificar cambios en sensores para envío inmediato
  bool sensoresChanged = false;
  for (int i = 0; i < 2; i++) {
    bool estadoActual = digitalRead(zonas[i].pirPin);
    if (estadoActual != estadoSensoresAnterior[i]) {
      estadoSensoresAnterior[i] = estadoActual;
      sensoresChanged = true;
    }
  }
  
  // Enviar estado cada segundo o cuando cambien los sensores
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000 || sensoresChanged)
  {
    enviarEstadoWebSocket();
    lastUpdate = millis();
  }

  delay(10);
}

// WebSocket Event Handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Desconectado!\n", num);
    break;
  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Conectado desde %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
    enviarEstadoWebSocket();
    break;
  }
  case WStype_TEXT:
    // Manejar comandos del cliente si es necesario
    break;
  }
}

// Enviar estado actual a todos los clientes WebSocket
void enviarEstadoWebSocket()
{
  DynamicJsonDocument doc(1024);

  // Hora actual
  doc["hora"] = horaManual;
  doc["minuto"] = minutoManual;
  doc["segundo"] = segundoManual;

  // Modo de operación
  doc["modo"] = modoHorario ? "Horario Laboral" : "Fuera de Horario";

  // Estado de zonas
  JsonArray zonasArray = doc.createNestedArray("zonas");
  for (int i = 0; i < 2; i++)
  {
    JsonObject zonaObj = zonasArray.createNestedObject();
    zonaObj["nombre"] = zonas[i].nombre;
    zonaObj["activo"] = zonas[i].activo;
    zonaObj["manual"] = zonas[i].manual;
    zonaObj["movimiento"] = digitalRead(zonas[i].pirPin); // Estado actual del sensor

    // Calcular countdown si está activo
    if (zonas[i].activo && zonas[i].encendidoTime > 0) {
      unsigned long tiempoTranscurrido = millis() - zonas[i].encendidoTime;
      unsigned long tiempoRestante = 0;
      
      if (!modoHorario) {
        // Fuera de horario: countdown desde activación o último movimiento
        if (zonas[i].manual) {
          // Manual: considerar último movimiento
          unsigned long tiempoSinMovimiento = millis() - zonas[i].lastMotion;
          if (tiempoSinMovimiento < 30000) { // Si hay movimiento reciente
            tiempoRestante = (TIEMPO_APAGADO > tiempoTranscurrido) ? (TIEMPO_APAGADO - tiempoTranscurrido) / 1000 : 0;
          } else {
            tiempoRestante = 0; // Se apagará pronto por falta de movimiento
          }
        } else {
          // Automático: desde último movimiento
          unsigned long tiempoSinMovimiento = millis() - zonas[i].lastMotion;
          tiempoRestante = (TIEMPO_APAGADO > tiempoSinMovimiento) ? (TIEMPO_APAGADO - tiempoSinMovimiento) / 1000 : 0;
        }
      } else {
        // En horario laboral: countdown normal
        tiempoRestante = (TIEMPO_APAGADO > tiempoTranscurrido) ? (TIEMPO_APAGADO - tiempoTranscurrido) / 1000 : 0;
      }
      
      zonaObj["countdown"] = tiempoRestante;
    } else {
      zonaObj["countdown"] = 0;
    }

    // Historial de actividad
    JsonArray actividad = zonaObj.createNestedArray("actividad");
    for (int j = 0; j < 60; j++)
    {
      actividad.add(zonas[i].actividad[j]);
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  webSocket.broadcastTXT(jsonString);
}

// Actualizar historial de actividad
void actualizarHistorialActividad()
{
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000)
  {
    for (int i = 0; i < 2; i++)
    {
      zonas[i].actividadPtr = (zonas[i].actividadPtr + 1) % 60;
      zonas[i].actividad[zonas[i].actividadPtr] = digitalRead(zonas[i].pirPin);
    }
    lastUpdate = millis();
  }
}

// Interrupciones seguras
void IRAM_ATTR handlePIR1()
{
  if (modoHorario) return; // Solo activar fuera de horario laboral
  
  pir1Triggered = true;
  pir1Time = millis();
  Serial.println("PIR1 activado (fuera de horario)");
}

void IRAM_ATTR handlePIR2()
{
  if (modoHorario) return; // Solo activar fuera de horario laboral
  
  pir2Triggered = true;
  pir2Time = millis();
  Serial.println("PIR2 activado (fuera de horario)");
}

// Procesar interrupciones en el loop principal
void procesarInterrupciones()
{
  unsigned long tiempoActual = millis();

  if (pir1Triggered)
  {
    pir1Triggered = false;
    if (!modoHorario)
    {
      zonas[0].lastMotion = pir1Time;
      if (!zonas[0].manual)
      {
        setZonaActiva(0, true);
        Serial.println("Zona 1 activada (movimiento fuera de horario)");
      }
      else
      {
        // Si está en manual y hay movimiento, actualizar tiempo de movimiento
        Serial.println("Zona 1: Movimiento detectado (manual activo)");
      }
    }
  }

  if (pir2Triggered)
  {
    pir2Triggered = false;
    if (!modoHorario)
    {
      zonas[1].lastMotion = pir2Time;
      if (!zonas[1].manual)
      {
        setZonaActiva(1, true);
        Serial.println("Zona 2 activada (movimiento fuera de horario)");
      }
      else
      {
        // Si está en manual y hay movimiento, actualizar tiempo de movimiento
        Serial.println("Zona 2: Movimiento detectado (manual activo)");
      }
    }
  }
}

// Actualizar reloj interno
void actualizarRelojInterno()
{
  unsigned long tiempoActual = millis();
  unsigned long segundosTranscurridos = (tiempoActual - ultimoTiempoActualizado) / 1000;

  if (segundosTranscurridos > 0)
  {
    segundoManual += segundosTranscurridos;

    if (segundoManual >= 60)
    {
      minutoManual += segundoManual / 60;
      segundoManual %= 60;

      if (minutoManual >= 60)
      {
        horaManual += minutoManual / 60;
        minutoManual %= 60;

        if (horaManual >= 24)
        {
          horaManual %= 24;
        }
      }
    }
    ultimoTiempoActualizado = tiempoActual;
  }
}

// Establecer estado de una zona
void setZonaActiva(int zonaIndex, bool activa)
{
  zonas[zonaIndex].activo = activa;
  if (activa) {
    zonas[zonaIndex].encendidoTime = millis();
  } else {
    zonas[zonaIndex].encendidoTime = 0;
  }
  
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(zonas[zonaIndex].relayPines[i], activa ? VALOR_ENCENDIDO_RELAY : VALOR_APAGADO_RELAY);
  }
}

// Manejar apagado automático
void manejarApagadoAutomatico()
{
  unsigned long tiempoActual = millis();

  if (modoHorario)
  {
    // En horario laboral: apagar por inactividad (control manual normal)
    for (int i = 0; i < 2; i++)
    {
      if (zonas[i].activo && !zonas[i].manual &&
          (tiempoActual - zonas[i].lastMotion > TIEMPO_APAGADO))
      {
        setZonaActiva(i, false);
        Serial.print("Zona ");
        Serial.print(i + 1);
        Serial.println(" apagada por inactividad");
      }
    }
  }
  else
  {
    // Fuera de horario: lógica especial
    for (int i = 0; i < 2; i++)
    {
      if (zonas[i].activo && zonas[i].encendidoTime > 0)
      {
        unsigned long tiempoEncendido = tiempoActual - zonas[i].encendidoTime;
        
        // Si fue encendido manualmente, requiere movimiento para mantenerse
        if (zonas[i].manual)
        {
          bool hayMovimientoReciente = (tiempoActual - zonas[i].lastMotion) <= 30000; // 30 segundos
          
          // Si no hay movimiento reciente O han pasado 5 minutos, apagar
          if (!hayMovimientoReciente || tiempoEncendido > TIEMPO_APAGADO)
          {
            setZonaActiva(i, false);
            zonas[i].manual = false; // Reset manual flag
            Serial.printf("Zona %d apagada (manual sin movimiento o timeout)\n", i + 1);
          }
        }
        // Si fue activado por sensor, apagar después de 5 minutos sin movimiento
        else if (tiempoActual - zonas[i].lastMotion > TIEMPO_APAGADO)
        {
          setZonaActiva(i, false);
          Serial.printf("Zona %d apagada (sensor timeout)\n", i + 1);
        }
      }
    }
  }
}

// Verificar si estamos en horario laboral
bool enHorarioLaboral()
{
  char horaActual[6];
  sprintf(horaActual, "%02d:%02d", horaManual, minutoManual);

  for (int i = 0; i < horarioCount; i++)
  {
    if (String(horaActual) >= horarios[i][0] && String(horaActual) <= horarios[i][1])
    {
      return true;
    }
  }
  return false;
}

// Handler para página principal - VERSIÓN OPTIMIZADA PARA MÓVILES
void handleRoot()
{
  char horaActual[9];
  sprintf(horaActual, "%02d:%02d:%02d", horaManual, minutoManual, segundoManual);

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

  server.send(200, "text/html", html);
}

// Configurar hora manual
void handleSetTime()
{
  if (server.hasArg("time"))
  {
    String timeStr = server.arg("time");
    int hora, minuto;
    if (sscanf(timeStr.c_str(), "%d:%d", &hora, &minuto) == 2)
    {
      horaManual = hora;
      minutoManual = minuto;
      segundoManual = 0;
      ultimoTiempoActualizado = millis();
      Serial.printf("Hora actualizada: %02d:%02d\n", hora, minuto);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Control manual de zonas
void handleManualControl()
{
  if (server.hasArg("zona"))
  {
    int zonaIndex = server.arg("zona").toInt();
    if (zonaIndex >= 0 && zonaIndex < 2)
    {
      bool encender = (server.uri() == "/on");
      
      // Permitir control manual siempre, pero con lógica especial fuera de horario
      if (encender) {
        zonas[zonaIndex].manual = true;
        setZonaActiva(zonaIndex, true);
        
        if (!modoHorario) {
          // Fuera de horario: actualizar tiempo de movimiento para el countdown
          zonas[zonaIndex].lastMotion = millis();
          Serial.printf("Zona %d encendida manualmente (fuera de horario)\n", zonaIndex + 1);
        } else {
          Serial.printf("Zona %d encendida manualmente\n", zonaIndex + 1);
        }
      } else {
        zonas[zonaIndex].manual = false;
        setZonaActiva(zonaIndex, false);
        Serial.printf("Zona %d apagada manualmente\n", zonaIndex + 1);
      }
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Actualizar horarios
void handleUpdateHorarios()
{
  for (int i = 0; i < horarioCount; i++)
  {
    String inicioKey = "inicio" + String(i);
    String finKey = "fin" + String(i);
    if (server.hasArg(inicioKey) && server.hasArg(finKey))
    {
      horarios[i][0] = server.arg(inicioKey);
      horarios[i][1] = server.arg(finKey);
      Serial.printf("Horario actualizado: %s - %s\n",
                    horarios[i][0].c_str(), horarios[i][1].c_str());
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Handler para rutas no encontradas
void handleNotFound()
{
  server.send(404, "text/plain", "Pagina no encontrada");
}