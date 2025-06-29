#include <WiFi.h>
#include <WebServer.h>

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
};

Zona zonas[] = {
    {PIR1_PIN, {RELAY1_PIN, RELAY2_PIN}, 0, false, false, "Zona 1"},
    {PIR2_PIN, {RELAY3_PIN, RELAY4_PIN}, 0, false, false, "Zona 2"}};

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

// Variables para el reloj manual
int horaManual = 8;                        // Hora inicial: 8 AM
int minutoManual = 0;                      // Minutos iniciales
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
void setup()
{
  // DELAY CRÍTICO PARA ESTABILIZACIÓN
  delay(10000);

  Serial.begin(115200);
  Serial.println("Iniciando sistema...");

  // INICIALIZACIÓN SEGURA DE RELÉS
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, VALOR_APAGADO_RELAY);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, VALOR_APAGADO_RELAY);
  pinMode(RELAY3_PIN, OUTPUT);
  digitalWrite(RELAY3_PIN, VALOR_APAGADO_RELAY);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY4_PIN, VALOR_APAGADO_RELAY);

  // Inicializar pines
  for (int i = 0; i < 2; i++)
  {
    pinMode(zonas[i].pirPin, INPUT);
    // Pull-down para evitar flotación
    digitalWrite(zonas[i].pirPin, LOW);
  }

  // ESTABILIZACIÓN ADICIONAL
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(RELAY1_PIN, VALOR_ENCENDIDO_RELAY);
    delay(100);
    digitalWrite(RELAY1_PIN, VALOR_APAGADO_RELAY);
    delay(100);
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

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop()
{
  server.handleClient();
  actualizarRelojInterno();
  procesarInterrupciones();

  // Verificar modo horario
  bool nuevoModo = enHorarioLaboral();
  if (nuevoModo != modoHorario)
  {
    modoHorario = nuevoModo;
    if (modoHorario)
    {
      Serial.println("Modo: Horario laboral");
      // Resetear banderas manuales al cambiar a horario laboral
      for (int i = 0; i < 2; i++)
      {
        zonas[i].manual = false;
      }
    }
    else
    {
      Serial.println("Modo: Fuera de horario");
      // Resetear banderas manuales al cambiar a fuera de horario
      for (int i = 0; i < 2; i++)
      {
        zonas[i].manual = false;
      }
      // Apagar zonas no manuales
      for (int i = 0; i < 2; i++)
      {
        if (!zonas[i].manual)
        {
          setZonaActiva(i, false);
        }
      }
    }
  }

  // Manejar apagado automático
  manejarApagadoAutomatico();
  delay(50);
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
      // Actualizar ambas zonas en modo fuera de horario
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
      // Actualizar ambas zonas en modo fuera de horario
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
    minutoManual += segundosTranscurridos;

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
    // En modo fuera de horario, apagar si no hay movimiento en cualquier zona
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
      Serial.print("Hora actualizada: ");
      Serial.print(hora);
      Serial.print(":");
      Serial.println(minuto);
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

      Serial.print("Zona ");
      Serial.print(zonaIndex + 1);
      Serial.println(encender ? " encendida manualmente" : " apagada manualmente");
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
      Serial.print("Horario actualizado: ");
      Serial.print(horarios[i][0]);
      Serial.print(" - ");
      Serial.println(horarios[i][1]);
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