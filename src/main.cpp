#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
// Declaraciones de funciones
void manejarPaginaPrincipal();
void enviarJavaScript();
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

// Configuración WiFi  servidor.sendContent_P(PSTR("if(zona.countdown&&zona.countdown>0){"));
  servidor.sendContent_P(PSTR("const countdownElement=document.getElementById(`zone-${idx}-countdown`);"));
  servidor.sendContent_P(PSTR("countdownElement.style.display='block';"));
  servidor.sendContent_P(PSTR("const minutes=Math.floor(zona.countdown/60);"));
  servidor.sendContent_P(PSTR("const seconds=zona.countdown%60;"));
  servidor.sendContent_P(PSTR("countdownElement.textContent=`Apagado en: ${minutes}:${String(seconds).padStart(2,'0')}`;"));
  servidor.sendContent_P(PSTR("}else{"));
  servidor.sendContent_P(PSTR("const countdownElement=document.getElementById(`zone-${idx}-countdown`);"));
  servidor.sendContent_P(PSTR("if(isHorarioLaboral&&zona.activo){"));
  servidor.sendContent_P(PSTR("countdownElement.style.display='block';"));
  servidor.sendContent_P(PSTR("countdownElement.textContent='Modo trabajo: Sin límite de tiempo';"));
  servidor.sendContent_P(PSTR("countdownElement.style.color='#28a745';"));
  servidor.sendContent_P(PSTR("}else{"));
  servidor.sendContent_P(PSTR("countdownElement.style.display='none';"));
  servidor.sendContent_P(PSTR("countdownElement.style.color='var(--warning)';"));
  servidor.sendContent_P(PSTR("}}"));char *ssid = "SistemaDigitales";
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
int horaActual = 19;                   // Hora inicial: 7 PM (fuera de horario para probar)
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
    Serial.printf("*** CAMBIO DE MODO *** De %s a %s\n", 
      !estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario",
      estaEnHorarioLaboral ? "Horario Laboral" : "Fuera de Horario");

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
        // EN HORARIO LABORAL: SIN COUNTDOWN - Las luces permanecen encendidas para trabajar
        tiempoRestante = 0; // No hay countdown en horario laboral
      }
      else
      {
        // FUERA DE HORARIO: Sistema de seguridad con countdown
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
  
  // Debug: mostrar datos enviados cada 10 segundos para no saturar
  static unsigned long ultimoDebug = 0;
  if (millis() - ultimoDebug > 10000) {
    Serial.printf("WebSocket - Modo: %s (%s), Hora: %02d:%02d:%02d\n", 
      estaEnHorarioLaboral ? "Laboral" : "Fuera", 
      estaEnHorarioLaboral ? "true" : "false",
      horaActual, minutoActual, segundoActual);
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
    ultimoDebug = millis();
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
    if (minutoActual != ultimoMinutoMostrado) {
      Serial.printf("Hora actual: %02d:%02d:%02d\n", horaActual, minutoActual, segundoActual);
      ultimoMinutoMostrado = minutoActual;
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

  if (estaEnHorarioLaboral)
  {
    // EN HORARIO LABORAL: NO se apagan automáticamente las luces
    // Las luces permanecen encendidas para permitir el trabajo sin interrupciones
    return; // No hacer nada durante horario laboral
  }
  else
  {
    // FUERA DE HORARIO: Sistema de seguridad - Lógica principal de control
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
          Serial.printf("Zona %d apagada por falta de movimiento (fuera de horario)\n", i + 1);
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
            Serial.printf("Zona %d apagada por timeout máximo (5 min) - fuera de horario\n", i + 1);
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

// Handler para página principal - VERSIÓN OPTIMIZADA PARA MEMORIA
void manejarPaginaPrincipal()
{
  // Configurar respuesta chunked para envío eficiente
  servidor.setContentLength(CONTENT_LENGTH_UNKNOWN);
  servidor.send(200, "text/html", "");

  // Fragmento 1: DOCTYPE y head con CSS
  servidor.sendContent_P(PSTR("<!DOCTYPE html><html lang=\"es\"><head>"));
  servidor.sendContent_P(PSTR("<meta charset=\"UTF-8\">"));
  servidor.sendContent_P(PSTR("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no\">"));
  servidor.sendContent_P(PSTR("<title>Control de Luces Inteligente</title>"));
  
  // CSS en fragmentos
  servidor.sendContent_P(PSTR("<style>"));
  servidor.sendContent_P(PSTR(":root{--primary:#4361ee;--secondary:#3f37c9;--success:#4cc9f0;--danger:#f72585;--warning:#f8961e;--dark:#212529;--light:#f8f9fa;}"));
  servidor.sendContent_P(PSTR("*{box-sizing:border-box;margin:0;padding:0;font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;touch-action:manipulation;}"));
  servidor.sendContent_P(PSTR("body{background-color:#f5f7fa;color:#333;line-height:1.6;padding:10px;font-size:14px;}"));
  servidor.sendContent_P(PSTR(".container{max-width:100%;margin:0 auto;display:grid;grid-template-columns:1fr;gap:15px;}"));
  servidor.sendContent_P(PSTR(".card{background:white;border-radius:10px;box-shadow:0 2px 4px rgba(0,0,0,0.1);padding:15px;}"));
  servidor.sendContent_P(PSTR(".card-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px;padding-bottom:8px;border-bottom:1px solid #eee;}"));
  servidor.sendContent_P(PSTR(".card-title{font-size:1.1rem;font-weight:600;color:var(--primary);}"));
  servidor.sendContent_P(PSTR(".clock-container{text-align:center;padding:12px;background:linear-gradient(135deg,#4361ee,#3a0ca3);color:white;border-radius:8px;margin-bottom:15px;}"));
  servidor.sendContent_P(PSTR(".mode-banner{text-align:center;padding:15px;border-radius:8px;margin-bottom:15px;font-size:1.3rem;font-weight:700;text-transform:uppercase;letter-spacing:1px;}"));
  servidor.sendContent_P(PSTR(".mode-horario{background:linear-gradient(135deg,#28a745,#20c997);color:white;border:2px solid #1e7e34;}"));
  servidor.sendContent_P(PSTR(".mode-fuera-horario{background:linear-gradient(135deg,#dc3545,#fd7e14);color:white;border:2px solid #721c24;animation:pulse 2s infinite;}"));
  servidor.sendContent_P(PSTR("@keyframes pulse{0%{transform:scale(1);}50%{transform:scale(1.02);}100%{transform:scale(1);}}"));
  servidor.sendContent_P(PSTR("#real-time-clock{font-size:2.2rem;font-weight:700;letter-spacing:1px;font-family:'Courier New',monospace;}"));
  servidor.sendContent_P(PSTR(".mode-indicator{font-size:1rem;margin-top:5px;}"));
  servidor.sendContent_P(PSTR(".grid-2{display:grid;grid-template-columns:repeat(2,1fr);gap:10px;}"));
  servidor.sendContent_P(PSTR(".zone-card{text-align:center;padding:12px;border-radius:8px;background-color:#f8f9fa;}"));
  servidor.sendContent_P(PSTR(".zone-title{font-weight:600;margin-bottom:8px;font-size:1rem;}"));
  servidor.sendContent_P(PSTR(".zone-status{font-size:1rem;font-weight:700;margin-bottom:8px;}"));
  servidor.sendContent_P(PSTR(".sensor-indicator{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:4px;}"));
  servidor.sendContent_P(PSTR(".sensor-active{background-color:#4cc9f0;box-shadow:0 0 6px #4cc9f0;}"));
  servidor.sendContent_P(PSTR(".sensor-inactive{background-color:#adb5bd;}"));
  servidor.sendContent_P(PSTR(".btn{display:inline-block;padding:8px 12px;border:none;border-radius:5px;cursor:pointer;font-weight:500;transition:all 0.3s;text-decoration:none;font-size:0.9rem;}"));
  servidor.sendContent_P(PSTR(".btn-primary{background-color:var(--primary);color:white;}"));
  servidor.sendContent_P(PSTR(".form-group{margin-bottom:12px;}.form-row{display:flex;gap:8px;}"));
  servidor.sendContent_P(PSTR("input[type='time'],input[type='text']{width:100%;padding:8px;border:1px solid #ddd;border-radius:5px;font-size:0.9rem;}"));
  servidor.sendContent_P(PSTR(".connection-status{display:flex;align-items:center;justify-content:center;margin-top:5px;font-size:0.8rem;}"));
  servidor.sendContent_P(PSTR(".connection-dot{width:10px;height:10px;border-radius:50%;margin-right:5px;}"));
  servidor.sendContent_P(PSTR(".connected{background-color:#4cc9f0;}.disconnected{background-color:#f72585;}"));
  servidor.sendContent_P(PSTR(".switch{position:relative;display:inline-block;width:60px;height:34px;margin-top:8px;}"));
  servidor.sendContent_P(PSTR(".switch input{opacity:0;width:0;height:0;}"));
  servidor.sendContent_P(PSTR(".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.4s;border-radius:34px;}"));
  servidor.sendContent_P(PSTR(".slider:before{position:absolute;content:\"\";height:26px;width:26px;left:4px;bottom:4px;background-color:white;transition:.4s;border-radius:50%;}"));
  servidor.sendContent_P(PSTR("input:checked+.slider{background-color:var(--success);}input:checked+.slider:before{transform:translateX(26px);}"));
  servidor.sendContent_P(PSTR(".countdown-display{font-size:0.8rem;color:var(--warning);margin-top:5px;font-weight:600;}"));
  servidor.sendContent_P(PSTR("</style></head>"));

  // Fragmento 2: Body y contenido principal
  servidor.sendContent_P(PSTR("<body><div class=\"container\">"));
  servidor.sendContent_P(PSTR("<div class=\"card\"><div class=\"clock-container\">"));
  servidor.sendContent_P(PSTR("<div id=\"real-time-clock\">--:--:--</div>"));
  servidor.sendContent_P(PSTR("<div class=\"mode-indicator\" id=\"mode-indicator\">Modo: Cargando...</div>"));
  servidor.sendContent_P(PSTR("<div class=\"connection-status\">"));
  servidor.sendContent_P(PSTR("<div class=\"connection-dot disconnected\" id=\"connection-dot\"></div>"));
  servidor.sendContent_P(PSTR("<span id=\"connection-status\">Desconectado</span></div></div>"));
  servidor.sendContent_P(PSTR("<div class=\"mode-banner mode-horario\" id=\"mode-banner\">🕐 HORARIO LABORAL ACTIVO</div>"));

  // Fragmento 3: Zonas
  servidor.sendContent_P(PSTR("<div class=\"grid-2\">"));
  servidor.sendContent_P(PSTR("<div class=\"zone-card\" id=\"zone-1-card\">"));
  servidor.sendContent_P(PSTR("<div class=\"zone-title\">Zona 1</div>"));
  servidor.sendContent_P(PSTR("<div class=\"zone-status\" id=\"zone-1-status\">APAGADO</div>"));
  servidor.sendContent_P(PSTR("<div><span class=\"sensor-indicator\" id=\"zone-1-sensor\"></span> Sensor de movimiento</div>"));
  servidor.sendContent_P(PSTR("<div class=\"actions\"><label class=\"switch\">"));
  servidor.sendContent_P(PSTR("<input type=\"checkbox\" id=\"zone-1-switch\" onchange=\"toggleZone(0,this.checked)\">"));
  servidor.sendContent_P(PSTR("<span class=\"slider\"></span></label>"));
  servidor.sendContent_P(PSTR("<div class=\"countdown-display\" id=\"zone-1-countdown\" style=\"display:none;\"></div></div></div>"));

  servidor.sendContent_P(PSTR("<div class=\"zone-card\" id=\"zone-2-card\">"));
  servidor.sendContent_P(PSTR("<div class=\"zone-title\">Zona 2</div>"));
  servidor.sendContent_P(PSTR("<div class=\"zone-status\" id=\"zone-2-status\">APAGADO</div>"));
  servidor.sendContent_P(PSTR("<div><span class=\"sensor-indicator\" id=\"zone-2-sensor\"></span> Sensor de movimiento</div>"));
  servidor.sendContent_P(PSTR("<div class=\"actions\"><label class=\"switch\">"));
  servidor.sendContent_P(PSTR("<input type=\"checkbox\" id=\"zone-2-switch\" onchange=\"toggleZone(1,this.checked)\">"));
  servidor.sendContent_P(PSTR("<span class=\"slider\"></span></label>"));
  servidor.sendContent_P(PSTR("<div class=\"countdown-display\" id=\"zone-2-countdown\" style=\"display:none;\"></div></div></div></div></div>"));

  // Fragmento 4: Estado de sensores
  servidor.sendContent_P(PSTR("<div class=\"card\"><div class=\"card-header\">"));
  servidor.sendContent_P(PSTR("<div class=\"card-title\">Estado de Sensores de Movimiento</div></div>"));
  servidor.sendContent_P(PSTR("<div style=\"padding:10px;\">"));
  servidor.sendContent_P(PSTR("<p id=\"movimiento-zona-1\" style=\"margin:8px 0;padding:8px;border-radius:5px;background-color:#f8f9fa;\">Zona 1: Sin movimiento</p>"));
  servidor.sendContent_P(PSTR("<p id=\"movimiento-zona-2\" style=\"margin:8px 0;padding:8px;border-radius:5px;background-color:#f8f9fa;\">Zona 2: Sin movimiento</p>"));
  servidor.sendContent_P(PSTR("</div></div>"));

  // Fragmento 5: Configuración con valores dinámicos
  servidor.sendContent_P(PSTR("<div class=\"card\"><div class=\"card-header\">"));
  servidor.sendContent_P(PSTR("<div class=\"card-title\">Configuración</div></div>"));
  servidor.sendContent_P(PSTR("<form action=\"/update\" method=\"post\"><div class=\"form-row\">"));
  servidor.sendContent_P(PSTR("<div class=\"form-group\"><label>Horario 1:</label>"));
  servidor.sendContent_P(PSTR("<input type=\"time\" name=\"inicio0\" value=\""));
  servidor.sendContent(horariosLaborales[0][0]);
  servidor.sendContent_P(PSTR("\"><input type=\"time\" name=\"fin0\" value=\""));
  servidor.sendContent(horariosLaborales[0][1]);
  servidor.sendContent_P(PSTR("\" style=\"margin-top:5px;\"></div>"));
  servidor.sendContent_P(PSTR("<div class=\"form-group\"><label>Horario 2:</label>"));
  servidor.sendContent_P(PSTR("<input type=\"time\" name=\"inicio1\" value=\""));
  servidor.sendContent(horariosLaborales[1][0]);
  servidor.sendContent_P(PSTR("\"><input type=\"time\" name=\"fin1\" value=\""));
  servidor.sendContent(horariosLaborales[1][1]);
  servidor.sendContent_P(PSTR("\" style=\"margin-top:5px;\"></div></div>"));
  servidor.sendContent_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%;margin-top:10px;\">Actualizar Horarios</button></form>"));
  servidor.sendContent_P(PSTR("<form action=\"/settime\" method=\"post\" style=\"margin-top:15px;\">"));
  servidor.sendContent_P(PSTR("<div class=\"form-group\"><label>Hora actual (HH:MM):</label>"));
  servidor.sendContent_P(PSTR("<input type=\"text\" name=\"time\" id=\"manual-time\" placeholder=\"HH:MM\"></div>"));
  servidor.sendContent_P(PSTR("<button type=\"submit\" class=\"btn btn-primary\" style=\"width:100%;\">Establecer Hora</button></form></div></div>"));

  // Fragmento 6: JavaScript
  enviarJavaScript();
  
  servidor.sendContent_P(PSTR("</body></html>"));
  servidor.sendContent("");  // Finalizar respuesta chunked
}

// Función auxiliar para enviar JavaScript en fragmentos
void enviarJavaScript()
{
  servidor.sendContent_P(PSTR("<script>"));
  servidor.sendContent_P(PSTR("const connectionDot=document.getElementById('connection-dot');"));
  servidor.sendContent_P(PSTR("const connectionStatus=document.getElementById('connection-status');"));
  servidor.sendContent_P(PSTR("let socket;const host=window.location.hostname;"));
  servidor.sendContent_P(PSTR("function initWebSocket(){"));
  servidor.sendContent_P(PSTR("socket=new WebSocket(`ws://${host}:81/`);"));
  servidor.sendContent_P(PSTR("socket.addEventListener('open',()=>{"));
  servidor.sendContent_P(PSTR("connectionDot.classList.remove('disconnected');"));
  servidor.sendContent_P(PSTR("connectionDot.classList.add('connected');"));
  servidor.sendContent_P(PSTR("connectionStatus.textContent='Conectado';syncTimeAutomatically();});"));
  
  servidor.sendContent_P(PSTR("socket.addEventListener('message',(event)=>{"));
  servidor.sendContent_P(PSTR("const data=JSON.parse(event.data);"));
  servidor.sendContent_P(PSTR("console.log('WebSocket recibido:',data);"));  // Debug
  servidor.sendContent_P(PSTR("document.getElementById('real-time-clock').textContent="));
  servidor.sendContent_P(PSTR("`${String(data.hora).padStart(2,'0')}:${String(data.minuto).padStart(2,'0')}:${String(data.segundo).padStart(2,'0')}`;"));
  servidor.sendContent_P(PSTR("document.getElementById('mode-indicator').textContent=`Modo: ${data.modo}`;"));
  
  servidor.sendContent_P(PSTR("const modeBanner=document.getElementById('mode-banner');"));
  servidor.sendContent_P(PSTR("const isHorarioLaboral=data.modoActivo;"));  // Usar directamente el boolean
  servidor.sendContent_P(PSTR("console.log('Modo activo:',isHorarioLaboral);"));  // Debug
  servidor.sendContent_P(PSTR("if(isHorarioLaboral){"));
  servidor.sendContent_P(PSTR("modeBanner.className='mode-banner mode-horario';"));
  servidor.sendContent_P(PSTR("modeBanner.innerHTML='🕐 HORARIO LABORAL ACTIVO<br><small>Sensores desactivados</small>';"));
  servidor.sendContent_P(PSTR("}else{"));
  servidor.sendContent_P(PSTR("modeBanner.className='mode-banner mode-fuera-horario';"));
  servidor.sendContent_P(PSTR("modeBanner.innerHTML='🚨 FUERA DE HORARIO<br><small>Sensores de seguridad activos</small>';"));
  servidor.sendContent_P(PSTR("}"));
  
  servidor.sendContent_P(PSTR("data.zonas.forEach((zona,index)=>{"));
  servidor.sendContent_P(PSTR("const idx=index+1;"));
  servidor.sendContent_P(PSTR("document.getElementById(`zone-${idx}-status`).textContent=zona.activo?'ENCENDIDO':'APAGADO';"));
  servidor.sendContent_P(PSTR("document.getElementById(`zone-${idx}-status`).style.color=zona.activo?'#4cc9f0':'#f72585';"));
  
  servidor.sendContent_P(PSTR("const switchElement=document.getElementById(`zone-${idx}-switch`);"));
  servidor.sendContent_P(PSTR("switchElement.onchange=null;switchElement.checked=zona.activo;"));
  servidor.sendContent_P(PSTR("switchElement.onchange=function(){toggleZone(index,this.checked);};"));
  
  servidor.sendContent_P(PSTR("if(zona.countdown&&zona.countdown>0){"));
  servidor.sendContent_P(PSTR("const countdownElement=document.getElementById(`zone-${idx}-countdown`);"));
  servidor.sendContent_P(PSTR("countdownElement.style.display='block';"));
  servidor.sendContent_P(PSTR("const minutes=Math.floor(zona.countdown/60);"));
  servidor.sendContent_P(PSTR("const seconds=zona.countdown%60;"));
  servidor.sendContent_P(PSTR("countdownElement.textContent=`Apagado en: ${minutes}:${String(seconds).padStart(2,'0')}`;"));
  servidor.sendContent_P(PSTR("}else{document.getElementById(`zone-${idx}-countdown`).style.display='none';}"));
  
  servidor.sendContent_P(PSTR("const tiempoSinMovimiento=zona.movimiento;"));
  servidor.sendContent_P(PSTR("const sensorActivo=tiempoSinMovimiento<10;"));
  servidor.sendContent_P(PSTR("const sensor=document.getElementById(`zone-${idx}-sensor`);"));
  servidor.sendContent_P(PSTR("sensor.classList.toggle('sensor-active',sensorActivo);"));
  servidor.sendContent_P(PSTR("sensor.classList.toggle('sensor-inactive',!sensorActivo);"));
  
  servidor.sendContent_P(PSTR("const movimientoElement=document.getElementById(`movimiento-zona-${idx}`);"));
  servidor.sendContent_P(PSTR("if(isHorarioLaboral){"));
  servidor.sendContent_P(PSTR("movimientoElement.textContent=`Zona ${idx}: Sensor desactivado (horario laboral)`;"));
  servidor.sendContent_P(PSTR("movimientoElement.style.color='#6c757d';movimientoElement.style.fontWeight='normal';"));
  servidor.sendContent_P(PSTR("}else{"));
  servidor.sendContent_P(PSTR("let textoMovimiento;"));
  servidor.sendContent_P(PSTR("if(tiempoSinMovimiento>=999999){textoMovimiento='Sin movimiento registrado';}"));
  servidor.sendContent_P(PSTR("else if(tiempoSinMovimiento<60){textoMovimiento=`Último movimiento: hace ${tiempoSinMovimiento}s`;}"));
  servidor.sendContent_P(PSTR("else{const minutos=Math.floor(tiempoSinMovimiento/60);"));
  servidor.sendContent_P(PSTR("const segundos=tiempoSinMovimiento%60;"));
  servidor.sendContent_P(PSTR("textoMovimiento=`Último movimiento: hace ${minutos}m ${segundos}s`;}"));
  servidor.sendContent_P(PSTR("movimientoElement.textContent=`Zona ${idx}: ${textoMovimiento}`;"));
  servidor.sendContent_P(PSTR("if(tiempoSinMovimiento<10){movimientoElement.style.color='#dc3545';movimientoElement.style.fontWeight='bold';}"));
  servidor.sendContent_P(PSTR("else if(tiempoSinMovimiento<30){movimientoElement.style.color='#f8961e';movimientoElement.style.fontWeight='600';}"));
  servidor.sendContent_P(PSTR("else{movimientoElement.style.color='#28a745';movimientoElement.style.fontWeight='normal';}}"));
  servidor.sendContent_P(PSTR("});});"));
  
  servidor.sendContent_P(PSTR("socket.addEventListener('close',()=>{"));
  servidor.sendContent_P(PSTR("connectionDot.classList.remove('connected');connectionDot.classList.add('disconnected');"));
  servidor.sendContent_P(PSTR("connectionStatus.textContent='Desconectado, reconectando...';setTimeout(initWebSocket,3000);});"));
  servidor.sendContent_P(PSTR("socket.addEventListener('error',()=>socket.close());}"));
  
  servidor.sendContent_P(PSTR("document.addEventListener('DOMContentLoaded',()=>{"));
  servidor.sendContent_P(PSTR("const now=new Date();"));
  servidor.sendContent_P(PSTR("document.getElementById('manual-time').value="));
  servidor.sendContent_P(PSTR("`${String(now.getHours()).padStart(2,'0')}:${String(now.getMinutes()).padStart(2,'0')}`;"));
  servidor.sendContent_P(PSTR("initWebSocket();});"));
  
  servidor.sendContent_P(PSTR("function toggleZone(zona,estado){"));
  servidor.sendContent_P(PSTR("const url=estado?`/on?zona=${zona}`:`/off?zona=${zona}`;"));
  servidor.sendContent_P(PSTR("fetch(url).then(response=>{if(!response.ok){console.error('Error al cambiar estado de zona');}})"));
  servidor.sendContent_P(PSTR(".catch(err=>{console.error('Error:',err);});}"));
  
  servidor.sendContent_P(PSTR("function syncTimeAutomatically(){"));
  servidor.sendContent_P(PSTR("const now=new Date();"));
  servidor.sendContent_P(PSTR("const timeString=`${String(now.getHours()).padStart(2,'0')}:${String(now.getMinutes()).padStart(2,'0')}`;"));
  servidor.sendContent_P(PSTR("fetch('/settime',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"));
  servidor.sendContent_P(PSTR("body:`time=${timeString}`}).then(()=>{console.log('Hora sincronizada automáticamente:',timeString);})"));
  servidor.sendContent_P(PSTR(".catch(err=>{console.error('Error sincronizando hora:',err);});}"));
  servidor.sendContent_P(PSTR("</script>"));
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