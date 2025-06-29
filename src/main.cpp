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
const unsigned long TIEMPO_APAGADO = 60000; // 60 segundos
const int VALOR_ENCENDIDO_RELAY = LOW;
const int VALOR_APAGADO_RELAY = HIGH;

// Variables globales
bool modoHorario = true;
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

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
  procesarInterrupciones();
  manejarApagadoAutomatico();
  actualizarHistorialActividad();

  // Enviar estado cada segundo
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000)
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
  pir1Triggered = true;
  pir1Time = millis();
}

void IRAM_ATTR handlePIR2()
{
  pir2Triggered = true;
  pir2Time = millis();
}

// Procesar interrupciones en el loop principal
void procesarInterrupciones()
{
  unsigned long tiempoActual = millis();

  if (pir1Triggered)
  {
    pir1Triggered = false;
    if (modoHorario)
    {
      zonas[0].lastMotion = pir1Time;
      if (!zonas[0].manual)
      {
        setZonaActiva(0, true);
        Serial.println("Zona 1 activada (movimiento)");
      }
    }
    else
    {
      for (int i = 0; i < 2; i++)
      {
        zonas[i].lastMotion = tiempoActual;
        if (!zonas[i].manual)
        {
          setZonaActiva(i, true);
        }
      }
      Serial.println("Todas las zonas activadas (fuera de horario)");
    }
  }

  if (pir2Triggered)
  {
    pir2Triggered = false;
    if (modoHorario)
    {
      zonas[1].lastMotion = pir2Time;
      if (!zonas[1].manual)
      {
        setZonaActiva(1, true);
        Serial.println("Zona 2 activada (movimiento)");
      }
    }
    else
    {
      for (int i = 0; i < 2; i++)
      {
        zonas[i].lastMotion = tiempoActual;
        if (!zonas[i].manual)
        {
          setZonaActiva(i, true);
        }
      }
      Serial.println("Todas las zonas activadas (fuera de horario)");
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
    bool movimientoReciente = false;
    for (int i = 0; i < 2; i++)
    {
      if (tiempoActual - zonas[i].lastMotion <= TIEMPO_APAGADO)
      {
        movimientoReciente = true;
        break;
      }
    }

    if (!movimientoReciente)
    {
      for (int i = 0; i < 2; i++)
      {
        if (!zonas[i].manual && zonas[i].activo)
        {
          setZonaActiva(i, false);
        }
      }
      Serial.println("Todas las zonas apagadas (fuera de horario)");
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

  String html = R"=====(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Control de Luces Inteligente</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
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
    
    .status-badge {
      padding: 4px 8px;
      border-radius: 15px;
      font-size: 0.75rem;
      font-weight: 500;
    }
    
    .status-active {
      background-color: rgba(76, 201, 240, 0.2);
      color: #4cc9f0;
    }
    
    .status-inactive {
      background-color: rgba(247, 37, 133, 0.2);
      color: #f72585;
    }
    
    .clock-container {
      text-align: center;
      padding: 12px;
      background: linear-gradient(135deg, #4361ee, #3a0ca3);
      color: white;
      border-radius: 8px;
      margin-bottom: 15px;
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
    
    .zone-card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 4px;
    }
    
    .zone-card.active::before {
      background-color: var(--success);
    }
    
    .zone-card.inactive::before {
      background-color: var(--danger);
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
    
    .chart-container {
      height: 200px;
      position: relative;
    }
    
    .form-group {
      margin-bottom: 12px;
    }
    
    label {
      display: block;
      margin-bottom: 4px;
      font-weight: 500;
      font-size: 0.9rem;
    }
    
    input[type='time'], input[type='text'] {
      width: 100%;
      padding: 8px;
      border: 1px solid #ddd;
      border-radius: 5px;
      font-size: 0.9rem;
    }
    
    .form-row {
      display: flex;
      gap: 8px;
    }
    
    .form-row .form-group {
      flex: 1;
    }
    
    .actions {
      display: flex;
      gap: 8px;
      margin-top: 8px;
    }
    
    .actions .btn {
      flex: 1;
      text-align: center;
      padding: 8px;
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
    
    .test-button {
      background-color: #f8961e;
      color: white;
      border: none;
      border-radius: 5px;
      padding: 8px 12px;
      margin-top: 8px;
      cursor: pointer;
      width: 100%;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <div class="clock-container">
        <div id="real-time-clock">00:00:00</div>
        <div class="mode-indicator" id="mode-indicator">Modo: Cargando...</div>
        <div class="connection-status">
          <div class="connection-dot disconnected" id="connection-dot"></div>
          <span id="connection-status">Desconectado</span>
        </div>
      </div>
      
      <div class="grid-2">
        <div class="zone-card" id="zone-1-card">
          <div class="zone-title">Zona 1</div>
          <div class="zone-status" id="zone-1-status">APAGADO</div>
          <div>
            <span class="sensor-indicator" id="zone-1-sensor"></span>
            Sensor de movimiento
          </div>
          <div class="actions">
            <a href="/on?zona=0" class="btn btn-primary">Encender</a>
            <a href="/off?zona=0" class="btn btn-danger">Apagar</a>
          </div>
          <button class="test-button" onclick="testSensor(0)">Probar Sensor</button>
        </div>
        
        <div class="zone-card" id="zone-2-card">
          <div class="zone-title">Zona 2</div>
          <div class="zone-status" id="zone-2-status">APAGADO</div>
          <div>
            <span class="sensor-indicator" id="zone-2-sensor"></span>
            Sensor de movimiento
          </div>
          <div class="actions">
            <a href="/on?zona=1" class="btn btn-primary">Encender</a>
            <a href="/off?zona=1" class="btn btn-danger">Apagar</a>
          </div>
          <button class="test-button" onclick="testSensor(1)">Probar Sensor</button>
        </div>
      </div>
    </div>
    
    <div class="card">
      <div class="card-header">
        <div class="card-title">Actividad de Sensores (Últimos 60s)</div>
      </div>
      <div class="chart-container">
        <canvas id="activity-chart"></canvas>
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
        
        <button type="submit" class="btn btn-primary" style="width: 100%; margin-top: 10px;">Actualizar Horarios</button>
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
    // Elementos de estado de conexión
    const connectionDot = document.getElementById('connection-dot');
    const connectionStatus = document.getElementById('connection-status');
    
    // Actualizar reloj en tiempo real
    function updateClock() {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      const seconds = String(now.getSeconds()).padStart(2, '0');
      document.getElementById('real-time-clock').textContent = `${hours}:${minutes}:${seconds}`;
    }
    
    // Inicializar reloj y actualizar cada segundo
    updateClock();
    setInterval(updateClock, 1000);
    
    // Configurar WebSocket
    let socket;
    let reconnectInterval;
    const host = window.location.hostname;
    
    function initWebSocket() {
      socket = new WebSocket(`ws://${host}:81/`);
      
      socket.addEventListener('open', (event) => {
        console.log('WebSocket conectado');
        connectionDot.classList.remove('disconnected');
        connectionDot.classList.add('connected');
        connectionStatus.textContent = 'Conectado';
      });
      
      socket.addEventListener('message', (event) => {
        const data = JSON.parse(event.data);
        
        // Actualizar hora
        document.getElementById('real-time-clock').textContent = 
          `${String(data.hora).padStart(2, '0')}:${String(data.minuto).padStart(2, '0')}:${String(data.segundo).padStart(2, '0')}`;
        
        // Actualizar modo
        document.getElementById('mode-indicator').textContent = `Modo: ${data.modo}`;
        
        // Actualizar zonas
        data.zonas.forEach((zona, index) => {
          const zoneCard = document.getElementById(`zone-${index+1}-card`);
          const zoneStatus = document.getElementById(`zone-${index+1}-status`);
          const zoneSensor = document.getElementById(`zone-${index+1}-sensor`);
          
          // Actualizar estado
          zoneStatus.textContent = zona.activo ? 'ENCENDIDO' : 'APAGADO';
          zoneStatus.style.color = zona.activo ? '#4cc9f0' : '#f72585';
          
          // Actualizar tarjeta
          zoneCard.classList.toggle('active', zona.activo);
          zoneCard.classList.toggle('inactive', !zona.activo);
          
          // Actualizar sensor
          const sensorActive = zona.movimiento === 1;
          zoneSensor.classList.toggle('sensor-active', sensorActive);
          zoneSensor.classList.toggle('sensor-inactive', !sensorActive);
          
          // Actualizar gráfico
          if (activityChart) {
            chartData.datasets[index].data = zona.actividad;
            activityChart.update();
          }
        });
      });
      
      socket.addEventListener('close', (event) => {
        console.log('WebSocket desconectado');
        connectionDot.classList.remove('connected');
        connectionDot.classList.add('disconnected');
        connectionStatus.textContent = 'Desconectado, reconectando...';
        
        // Intentar reconectar cada 3 segundos
        clearInterval(reconnectInterval);
        reconnectInterval = setTimeout(() => {
          initWebSocket();
        }, 3000);
      });
      
      socket.addEventListener('error', (error) => {
        console.error('WebSocket error:', error);
        socket.close();
      });
    }
    
    // Variables para gráficos
    let activityChart;
    let chartData = {
      labels: Array.from({length: 60}, (_, i) => 59 - i), // De 59 a 0 (ahora)
      datasets: [
        {
          label: 'Zona 1',
          data: Array(60).fill(0),
          borderColor: '#4361ee',
          backgroundColor: 'rgba(67, 97, 238, 0.1)',
          tension: 0.4,
          fill: true,
          pointRadius: 0 // Sin puntos para mejorar rendimiento
        },
        {
          label: 'Zona 2',
          data: Array(60).fill(0),
          borderColor: '#f72585',
          backgroundColor: 'rgba(247, 37, 133, 0.1)',
          tension: 0.4,
          fill: true,
          pointRadius: 0
        }
      ]
    };
    
    // Inicializar gráfico
    function initChart() {
      const ctx = document.getElementById('activity-chart').getContext('2d');
      activityChart = new Chart(ctx, {
        type: 'line',
        data: chartData,
        options: {
          responsive: true,
          maintainAspectRatio: false,
          scales: {
            y: {
              min: 0,
              max: 1,
              ticks: {
                stepSize: 1,
                callback: value => value === 1 ? 'Activo' : 'Inactivo'
              }
            },
            x: {
              reverse: true,
              ticks: {
                maxTicksLimit: 10,
                callback: (value, index) => {
                  if (index === 0) return 'Ahora';
                  if (index === 59) return '60s';
                  return (59 - index) % 10 === 0 ? `${59 - index}s` : '';
                }
              }
            }
          },
          plugins: {
            legend: {
              position: 'top',
            },
            tooltip: {
              callbacks: {
                label: function(context) {
                  return context.dataset.label + ': ' + 
                         (context.parsed.y === 1 ? 'Activo' : 'Inactivo');
                }
              }
            }
          }
        }
      });
    }
    
    // Función para probar sensores
    function testSensor(zonaIndex) {
      const sensorElement = document.getElementById(`zone-${zonaIndex+1}-sensor`);
      
      // Simular detección de movimiento
      sensorElement.classList.add('sensor-active');
      
      setTimeout(() => {
        sensorElement.classList.remove('sensor-active');
      }, 2000);
      
      // Enviar comando al ESP32 para activar manualmente
      fetch(`/on?zona=${zonaIndex}`)
        .then(() => {
          setTimeout(() => {
            fetch(`/off?zona=${zonaIndex}`);
          }, 5000);
        });
    }
    
    // Actualizar hora manual con hora actual del sistema
    document.addEventListener('DOMContentLoaded', () => {
      const now = new Date();
      const hours = String(now.getHours()).padStart(2, '0');
      const minutes = String(now.getMinutes()).padStart(2, '0');
      document.getElementById('manual-time').value = `${hours}:${minutes}`;
      
      initChart();
      initWebSocket();
    });
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
      zonas[zonaIndex].manual = true;
      setZonaActiva(zonaIndex, encender);

      Serial.printf("Zona %d %s manualmente\n",
                    zonaIndex + 1, encender ? "encendida" : "apagada");
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