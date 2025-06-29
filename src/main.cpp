#include <WiFi.h>
#include <WebServer.h>

// Configuración WiFi
const char *ssid = "HomeLess";
const char *password = "Homeless2022";

// Configuración de pines
#define PIR1_PIN 13
#define PIR2_PIN 15

#define RELAY1_PIN 12
#define RELAY2_PIN 14
#define RELAY3_PIN 27
#define RELAY4_PIN 33

// Estructura de zonas
struct Zona
{
  int pirPin;
  int relayPines[2];
  unsigned long lastMotion;
  bool activo;
  String nombre;
};

Zona zonas[] = {
    {PIR1_PIN, {RELAY1_PIN, RELAY2_PIN}, 0, false, "Zona 1"},
    {PIR2_PIN, {RELAY3_PIN, RELAY4_PIN}, 0, false, "Zona 2"}};

// Horarios laborales
String horarios[][2] = {
    {"08:00", "12:00"},
    {"14:00", "18:00"}};
const int horarioCount = 2;

// Constantes
const unsigned long TIEMPO_APAGADO = 60000; // 60 segundos en ms
const int VALOR_ENCENDIDO_RELAY = LOW;
const int VALOR_APAGADO_RELAY = HIGH;

// Variables globales
bool modoHorario = true;
unsigned long lastMotionGlobal = 0;
bool estadoManual[2] = {false}; // Solo 2 zonas
WebServer server(80);

// Variables para el reloj manual
int horaManual = 8;                        // Hora inicial: 8 AM
int minutoManual = 0;                      // Minutos iniciales
unsigned long ultimoTiempoActualizado = 0; // Para llevar el tiempo transcurrido

// Prototipos de funciones
void IRAM_ATTR handlePIR(int zonaIndex);
void setZonaActiva(int zonaIndex, bool activa);
void manejarApagadoAutomatico();
bool enHorarioLaboral();
void actualizarRelojInterno();
void handleRoot();
void handleManualControl();
void handleUpdateHorarios();
void handleSetTime();
void handleNotFound();

void setup()
{
  Serial.begin(115200);

  // Inicializar pines
  for (int i = 0; i < 2; i++) // Solo 2 zonas
  {
    pinMode(zonas[i].pirPin, INPUT);
    for (int j = 0; j < 2; j++) // 2 relés por zona
    {
      pinMode(zonas[i].relayPines[j], OUTPUT);
      digitalWrite(zonas[i].relayPines[j], VALOR_APAGADO_RELAY);
    }
  }

  // Configurar como punto de acceso WiFi
  WiFi.softAP(ssid, password);
  Serial.println("Punto de acceso creado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // Iniciar el reloj interno
  ultimoTiempoActualizado = millis();

  // Configurar interrupciones PIR
  attachInterrupt(digitalPinToInterrupt(zonas[0].pirPin), []
                  { handlePIR(0); }, RISING);
  attachInterrupt(digitalPinToInterrupt(zonas[1].pirPin), []
                  { handlePIR(1); }, RISING);

  // Configurar rutas del servidor web
  server.on("/", handleRoot);
  server.on("/on", handleManualControl);
  server.on("/off", handleManualControl);
  server.on("/update", HTTP_POST, handleUpdateHorarios);
  server.on("/settime", HTTP_POST, handleSetTime);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado");
  char yourIP[16];
  WiFi.softAPIP().toString().toCharArray(yourIP, sizeof(yourIP));
  Serial.print("Accede al servidor en: http://");
  Serial.println(yourIP);
  delay(15000);
}

void loop()
{
  server.handleClient();
  actualizarRelojInterno();

  // Verificar modo horario
  bool nuevoModo = enHorarioLaboral();
  if (nuevoModo != modoHorario)
  {
    modoHorario = nuevoModo;
    if (modoHorario)
    {
      Serial.println("🕒 Entrando en horario laboral");
    }
    else
    {
      Serial.println("🌙 Saliendo de horario laboral");
      // Apagar todas las zonas (excepto manuales)
      for (int i = 0; i < 2; i++)
      {
        if (!estadoManual[i])
        {
          setZonaActiva(i, false);
        }
      }
      lastMotionGlobal = 0;
    }
  }

  // Manejar apagado automático
  manejarApagadoAutomatico();
  delay(100);
}

// Actualizar reloj interno basado en millis()
void actualizarRelojInterno()
{
  unsigned long tiempoActual = millis();
  unsigned long segundosTranscurridos = (tiempoActual - ultimoTiempoActualizado) / 1000;

  if (segundosTranscurridos > 0)
  {
    minutoManual += segundosTranscurridos / 60;
    segundosTranscurridos %= 60;

    if (minutoManual >= 60)
    {
      horaManual += minutoManual / 60;
      minutoManual %= 60;

      if (horaManual >= 24)
      {
        horaManual %= 24;
      }
    }
    ultimoTiempoActualizado = tiempoActual;
  }
}

// Handler para interrupciones PIR
void IRAM_ATTR handlePIR(int zonaIndex)
{
  if (modoHorario)
  {
    zonas[zonaIndex].lastMotion = millis();
    if (!estadoManual[zonaIndex])
    {
      setZonaActiva(zonaIndex, true);
    }
  }
  else
  {
    lastMotionGlobal = millis();
    for (int i = 0; i < 2; i++)
    {
      if (!estadoManual[i])
      {
        setZonaActiva(i, true);
      }
    }
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
  if (modoHorario)
  {
    for (int i = 0; i < 2; i++)
    {
      if (zonas[i].activo && !estadoManual[i] &&
          (millis() - zonas[i].lastMotion > TIEMPO_APAGADO))
      {
        setZonaActiva(i, false);
      }
    }
  }
  else if (lastMotionGlobal > 0 &&
           (millis() - lastMotionGlobal > TIEMPO_APAGADO))
  {
    for (int i = 0; i < 2; i++)
    {
      if (zonas[i].activo && !estadoManual[i])
      {
        setZonaActiva(i, false);
      }
    }
    lastMotionGlobal = 0;
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

// Handler para página principal
void handleRoot()
{
  char horaActual[6];
  sprintf(horaActual, "%02d:%02d", horaManual, minutoManual);

  String html = R"(
  <html>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
      body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }
      .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
      h1 { color: #333; text-align: center; }
      table { width: 100%; border-collapse: collapse; margin: 20px 0; }
      th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
      th { background-color: #f2f2f2; }
      button { background-color: #4CAF50; color: white; border: none; padding: 10px 15px; border-radius: 5px; cursor: pointer; }
      button:hover { background-color: #45a049; }
      .form-group { margin-bottom: 15px; }
      input[type='time'] { padding: 8px; width: 100%; box-sizing: border-box; }
      input[type='text'] { padding: 8px; width: 100%; box-sizing: border-box; }
    </style>
  </head>
  <body>
    <div class='container'>
      <h1>Control de Luces de Oficina</h1>
      <p>Hora actual: <strong>)";
  html += horaActual;
  html += R"(</strong></p>
      <p>Modo actual: <strong>)";
  html += (modoHorario ? "Horario Laboral" : "Fuera de Horario");
  html += R"(</strong></p>
      
      <h2>Configurar Hora</h2>
      <form action='/settime' method='post'>
        <div class='form-group'>
          <label>Hora actual (HH:MM):</label>
          <input type='text' name='time' value=')";
  html += horaActual;
  html += R"(' placeholder='HH:MM'>
        </div>
        <button type='submit'>Establecer Hora</button>
      </form>
      
      <h2>Configurar Horarios</h2>
      <form action='/update' method='post'>)";

  for (int i = 0; i < horarioCount; i++)
  {
    html += R"(
        <div class='form-group'>
          <label>Turno )";
    html += String(i + 1);
    html += R"(:</label><br>
          <input type='time' name='inicio)";
    html += String(i);
    html += R"(' value=')";
    html += horarios[i][0];
    html += R"('> a 
          <input type='time' name='fin)";
    html += String(i);
    html += R"(' value=')";
    html += horarios[i][1];
    html += R"('>
        </div>)";
  }

  html += R"(
        <button type='submit'>Actualizar Horarios</button>
      </form>
      
      <h2>Control Manual</h2>
      <table>
        <tr><th>Zona</th><th>Estado</th><th>Acciones</th></tr>)";

  for (int i = 0; i < 2; i++)
  {
    bool encendida = zonas[i].activo;
    html += R"(
        <tr>
          <td>)";
    html += zonas[i].nombre;
    html += R"(</td>
          <td><strong>)";
    html += (encendida ? "ENCENDIDO" : "APAGADO");
    html += R"(</strong></td>
          <td>
            <a href='/on?zona=)";
    html += String(i);
    html += R"('><button>Encender</button></a>
            <a href='/off?zona=)";
    html += String(i);
    html += R"('><button>Apagar</button></a>
          </td>
        </tr>)";
  }

  html += R"(
      </table>
    </div>
  </body>
  </html>)";

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
      ultimoTiempoActualizado = millis();
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
      estadoManual[zonaIndex] = true;
      setZonaActiva(zonaIndex, encender);
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