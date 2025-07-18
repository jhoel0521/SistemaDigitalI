# Sistema de Control de Luces Inteligente

<div align="center">
  <img src="https://www.upds.edu.bo/wp-content/uploads/2020/10/upds_logo-1-1-1.png" alt="UPDS Logo" width="300"/>
  
  **Universidad Privada Domingo Savio (UPDS)**  
  **Facultad de Ingeniería**  
  **Materia: Sistemas Digitales II** (Tercer Semestre)
  
  ---
  
  **Desarrollado por:**  
  🎓 **Jhoel** (💻 Software) | 🎓 **Alan** (⚡ Hardware) | 🎓 **Nuvia** (🔧 Ensamblado) | 🎓 **Erick** (📋 Documentación)
  
  🌐 [www.upds.edu.bo](https://www.upds.edu.bo/)
</div>

---

## 📋 **Descripción del Proyecto**

Sistema inteligente de control de luces basado en **ESP32** que utiliza sensores PIR para detección de movimiento y gestiona el encendido/apagado automático de luminarias según horarios laborales predefinidos. El sistema incluye una interfaz web responsive para monitoreo y control remoto en tiempo real.

### 🎯 **Objetivos Académicos**
- Aplicar conceptos de **sistemas embebidos** y **microcontroladores**
- Implementar **comunicación WiFi** y **protocolos web** (HTTP/WebSocket)
- Desarrollar **interfaces de usuario** responsivas
- Integrar **sensores** y **actuadores** en tiempo real
- Aplicar **arquitecturas de software** modulares y escalables

---

## 🚀 **Características Principales**

### 🏢 **Control Inteligente de Horarios**
- ⏰ **Horario Laboral**: Luces manuales, sensores desactivados
- 🌙 **Fuera de Horario**: Control automático por sensores PIR
- 🕐 **Configuración Flexible**: Múltiples rangos horarios
- 🔄 **Transiciones Automáticas**: Cambio automático de modo

### 🎛️ **Interfaz Web Avanzada**
- 📱 **Responsive Design**: Compatible con móviles y desktop
- 🌐 **Tiempo Real**: Comunicación WebSocket bidireccional
- 🎨 **UI Moderna**: Diseño intuitivo con indicadores visuales
- 🔧 **Control Remoto**: Encendido/apagado manual de zonas

### 🤖 **Automatización Inteligente**
- 👁️ **Sensores PIR**: Detección de movimiento por zona
- 🔋 **Ahorro Energético**: PIR solo extiende tiempo, nunca enciende zonas apagadas fuera de horario
- ⏱️ **Apagado Temporizado**: 5 minutos sin actividad
- 🎛️ **Control Híbrido**: Manual durante horario laboral, automático fuera de horario

### 🔋 **Sistema de Ahorro Energético**
**Comportamiento Inteligente según Horario:**

#### Durante Horario Laboral (8:00 AM - 6:00 PM):
- ❌ **Sensores PIR inactivos** (control manual únicamente)
- ✅ **Sin apagado automático** (no interrumpe el trabajo)

#### Fuera de Horario (6:00 PM - 8:00 AM):
- ✅ **Sensores PIR activos** con lógica de ahorro
- ❌ **PIR NO enciende zonas apagadas** (ahorro energético)
- ✅ **PIR extiende tiempo de zonas encendidas** (seguridad)
- ⏰ **Apagado automático tras 5 min sin movimiento**

> 📖 **Documentación completa**: Ver [COMPORTAMIENTO_AHORRO_ENERGETICO.md](./COMPORTAMIENTO_AHORRO_ENERGETICO.md)
- 🔄 **Extensión Inteligente**: Tiempo renovado por movimiento continuo

### 🌐 **Conectividad y Control**
- 📶 **WiFi AP Mode**: Punto de acceso independiente
- 🔗 **API RESTful**: Endpoints para control programático
- 🔄 **WebSocket**: Comunicación en tiempo real
- 📊 **Monitoreo**: Estado de sensores y zonas en tiempo real

---

## 🛠️ **Arquitectura Técnica**

### 🔧 **Hardware**
- **Microcontrolador**: ESP32 DevKit v1
- **Sensores**: PIR HC-SR501 (2 zonas)
- **Actuadores**: Módulos relay de 2 canales
- **Comunicación**: WiFi integrado ESP32

### 💻 **Software**
- **Framework**: Arduino + PlatformIO
- **Librerías**: WebServer, WebSockets, ArduinoJson
- **Frontend**: HTML5, CSS3, JavaScript ES6
- **Arquitectura**: Modular, orientada a eventos

### 📁 **Estructura del Proyecto**
```
SistemaDigital/
├── src/                    # Código fuente principal
│   ├── main.cpp           # Punto de entrada del sistema
│   ├── config.h/cpp       # Configuración global
│   ├── zones.h/cpp        # Gestión de zonas y relays
│   ├── time_utils.h/cpp   # Manejo de tiempo y horarios
│   ├── mi_webserver.h/cpp # Servidor web e interfaz
│   ├── websocket.h/cpp    # Comunicación en tiempo real
│   └── interrupts.h/cpp   # Lectura de sensores PIR
├── test/                  # Tests unitarios
│   ├── test_control_remoto/
│   ├── test_extension_movimiento/
│   └── test_apagado_5min/
├── platformio.ini         # Configuración del proyecto
└── README.md              # Este archivo
```

---

## 🔌 **Configuración de Hardware**

### 📍 **Conexiones ESP32**

#### **Zona 1 - Oficina Principal**
- **PIR Sensor**: Pin GPIO 13
- **Relay 1**: Pin GPIO 32
- **Relay 2**: Pin GPIO 25

#### **Zona 2 - Área de Reuniones**
- **PIR Sensor**: Pin GPIO 15
- **Relay 1**: Pin GPIO 26
- **Relay 2**: Pin GPIO 21

#### **Alimentación**
- **ESP32**: 5V/3.3V via USB o fuente externa
- **Relays**: 5V (VCC), GND común
- **Sensores PIR**: 5V (VCC), GND común

### 🔧 **Esquema de Conexión**
```
ESP32           PIR HC-SR501      Relay Module
                                  
GPIO 13  -----> OUT (Zona 1)      
GPIO 15  -----> OUT (Zona 2)      
                                  
GPIO 32  ------------------> IN1 (Zona 1 - Relay 1)
GPIO 25  ------------------> IN2 (Zona 1 - Relay 2)
GPIO 26  ------------------> IN1 (Zona 2 - Relay 1)
GPIO 21  ------------------> IN2 (Zona 2 - Relay 2)

5V       -----> VCC -----------> VCC
GND      -----> GND -----------> GND
```

---

## 🚀 **Instalación y Configuración**

### 📋 **Prerrequisitos**
- [PlatformIO](https://platformio.org/) instalado
- [VS Code](https://code.visualstudio.com/) con extensión PlatformIO
- Cable USB para programación
- Hardware según esquema de conexión

### 🔽 **Instalación**

1. **Clonar el repositorio**:
   ```bash
   git clone [URL_DEL_REPOSITORIO]
   cd SistemaDigital
   ```

2. **Abrir en VS Code**:
   ```bash
   code .
   ```

3. **Instalar dependencias**:
   PlatformIO descargará automáticamente las librerías necesarias:
   - WebSockets v2.6.1
   - ArduinoJson v7.4.2

4. **Configurar puerto serie**:
   Editar `platformio.ini` si es necesario:
   ```ini
   upload_port = COM4  ; Windows
   # upload_port = /dev/ttyUSB0  ; Linux
   ```

5. **Compilar y subir**:
   ```bash
   pio run --target upload
   ```

### 📶 **Configuración WiFi**

El sistema crea un punto de acceso WiFi:
- **SSID**: `SistemaDigitales`
- **Contraseña**: `12345678`
- **IP del ESP32**: `192.168.4.1`

### 🌐 **Acceso a la Interfaz Web**

#### 🏠 **Dominios Personalizados Disponibles:**

**Opción 1 - Dominio Principal:**
- 🌐 **http://micasita.com** (Fácil de recordar)
- 📱 **http://micasita.local** (Protocolo mDNS)

**Opción 2 - Acceso Directo:**
- 🔧 **http://192.168.4.1** (IP del ESP32)

#### 📋 **Pasos de Acceso:**

1. **Conectar dispositivo** a la red WiFi `SistemaDigitales`
2. **Abrir navegador** en cualquiera de los dominios:
   - `http://micasita.com` ← **Recomendado**
   - `http://micasita.local`
   - `http://192.168.4.1`
3. **La interfaz se carga automáticamente**

#### 🔧 **Tecnología del Dominio:**

- **Captive Portal**: Cualquier URL redirige al sistema
- **mDNS**: Resolución de nombres local
- **DNS Server**: Captura todos los dominios
- **Auto-redirección**: URLs desconocidas → `micasita.com`

#### 🎯 **Ventajas:**

- ✅ **Fácil acceso**: No necesitas recordar IPs
- ✅ **Múltiples opciones**: Varios dominios funcionan
- ✅ **Auto-redirección**: Cualquier URL funciona
- ✅ **Compatible**: Funciona en móviles y desktop

---

## 🎮 **Uso del Sistema**

### 🕐 **Configuración de Horarios**

1. **Acceder a Configuración** en la interfaz web
2. **Horario 1**: Ejemplo `08:00 - 12:00`
3. **Horario 2**: Ejemplo `14:00 - 18:00`
4. **Guardar cambios** con el botón "Actualizar Horarios"

### ⏰ **Configuración de Hora**

**Opción 1 - Automática**:
- Al conectar, se sincroniza automáticamente con el navegador

**Opción 2 - Manual**:
- Ingresar hora en formato `HH:MM`
- Clic en "Establecer Hora Manualmente"

### 🎛️ **Control de Zonas**

#### **Durante Horario Laboral**:
- ✅ Control manual habilitado
- ❌ Sensores PIR desactivados
- 🔄 Encender/apagar con switches en interfaz

#### **Fuera de Horario**:
- 🤖 Control automático por sensores
- ⚡ Auto-encendido por movimiento
- ⏱️ Apagado automático después de 5 minutos sin actividad
- 🔄 Tiempo extendido por movimiento continuo

### 📊 **Monitoreo en Tiempo Real**

- 🟢 **Indicador de conexión**: Estado del WebSocket
- 🕐 **Reloj en tiempo real**: Hora del sistema
- 🚨 **Modo actual**: Horario Laboral vs Fuera de Horario
- 👁️ **Estado de sensores**: Actividad de movimiento
- 💡 **Estado de zonas**: Encendido/apagado de cada zona
- ⏱️ **Countdown**: Tiempo restante antes del apagado automático

---

## 🧪 **Testing y Validación**

### � **Validación Manual del Sistema**

Dado que este es un sistema embebido que interactúa con hardware real, la validación principal se realiza de forma manual en el dispositivo:

#### ✅ **Funcionalidades Validadas:**

**1. Control Manual (Horario Laboral):**
- ✅ Encendido/apagado via interfaz web funcionando
- ✅ Control independiente por zona (cada zona se maneja por separado)
- ✅ Sensores PIR desactivados durante horario laboral
- ✅ Sin apagado automático durante trabajo

**2. Control Automático (Fuera de Horario):**
- ✅ PIR SOLO extiende tiempo de zonas ya encendidas (ahorro energético)
- ✅ PIR NO enciende zonas apagadas fuera de horario
- ✅ Apagado automático individual por zona tras 5 min sin movimiento
- ✅ **CORREGIDO**: Cronómetros se resetean correctamente al detectar movimiento

**3. Interfaz Web en Tiempo Real:**
- ✅ Comunicación WebSocket bidireccional
- ✅ Actualización en tiempo real de estados
- ✅ **CORREGIDO**: Countdown individual por zona (no global)
- ✅ Dominios personalizados: http://micasita.com, http://micasita.local

**4. Correcciones Implementadas:**
- 🔧 **SOLUCIONADO**: Apagado independiente por zona (antes se apagaban todas cuando una llegaba a 5 min)
- 🔧 **SOLUCIONADO**: Cronómetros de countdown se actualizan correctamente en la interfaz web
- 🔧 **SOLUCIONADO**: Cada zona se controla individualmente según su propio movimiento

#### 🔬 **Tests de Desarrollo (Código Fuente):**

Los tests unitarios están disponibles en la carpeta `test/` para referencia de desarrollo:
- `test_calibrar_pir/`: Validación de configuración de pines
- `test_control_manual/`: Tests de control remoto
- `test_ahorro_energetico/`: Validación de comportamiento energético
- `test_apagado_automatico/`: Tests de apagado temporizado

**Nota**: Los tests están diseñados para referencia de desarrollo. La validación principal se realiza en hardware real.

#### 📋 **Procedimiento de Validación Recomendado:**

1. **Compilar y subir firmware**:
   ```bash
   C:\Users\ESTE-PC-01\.platformio\penv\Scripts\platformio.exe run --target upload
   ```
2. **Conectarse al WiFi**: `SistemaDigitales` (password: `12345678`)
3. **Acceder a interfaz**: http://micasita.com
4. **Validar horario laboral**: Control manual, PIR inactivos
5. **Validar fuera de horario**: PIR extiende tiempo, apagado automático
6. **Verificar cronómetros**: Se resetean al detectar movimiento

---

## 🔧 **Arquitectura de Software**

### 🏗️ **Patrón de Diseño**

El sistema utiliza una **arquitectura modular basada en eventos**:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Interfaz Web  │◄───┤   WebSocket     │◄───┤   ESP32 Core    │
│   (Frontend)    │    │   (Real-time)   │    │   (Backend)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                       ▲
                                                       │
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Sensores PIR  │────┤  Interrupts     │────┤   Control de    │
│   (Input)       │    │  (Polling)      │    │   Zonas         │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                                       │
                                                       ▼
                                               ┌─────────────────┐
                                               │   Relays        │
                                               │   (Output)      │
                                               └─────────────────┘
```

### 📚 **Módulos Principales**

1. **`main.cpp`**: Coordinador principal, loop de eventos
2. **`zones.cpp`**: Gestión de estados y control de relays
3. **`time_utils.cpp`**: Manejo de tiempo y verificación de horarios
4. **`mi_webserver.cpp`**: Servidor HTTP e interfaz web
5. **`websocket.cpp`**: Comunicación tiempo real
6. **`interrupts.cpp`**: Lectura de sensores por polling

### 🔄 **Flujo de Datos**

1. **Sensores PIR** → Lectura cada 100ms
2. **Detección de movimiento** → Auto-encendido de zona
3. **Estado actualizado** → Envío via WebSocket
4. **Interfaz web** → Actualización en tiempo real
5. **Control manual** → HTTP POST → Actualización de estado

---

## 🔍 **Funcionalidades Avanzadas**

### 🧠 **Algoritmo de Control Inteligente**

#### **Durante Horario Laboral**:
```cpp
if (estaEnHorarioLaboral) {
    // Sensores PIR ignorados
    // Control manual habilitado
    // Sin apagado automático
}
```

#### **Fuera de Horario**:
```cpp
if (!estaEnHorarioLaboral && movimientoDetectado) {
    encenderZonaAutomaticamente();
    iniciarContador5Minutos();
}

if (tiempoSinMovimiento >= 5_MINUTOS) {
    apagarZonaAutomaticamente();
}
```

### 📡 **Protocolo de Comunicación WebSocket**

#### **Mensaje de Estado** (ESP32 → Web):
```json
{
  "hora": 14,
  "minuto": 30,
  "segundo": 45,
  "modo": "Fuera de Horario",
  "modoActivo": false,
  "zonas": [
    {
      "activo": true,
      "movimiento": 120,
      "countdown": 180
    },
    {
      "activo": false,
      "movimiento": 9999,
      "countdown": 0
    }
  ]
}
```

#### **Control Manual** (Web → ESP32):
```http
GET /on?zona=0   // Encender zona 1
GET /off?zona=1  // Apagar zona 2
POST /settime    // Configurar hora
POST /update     // Actualizar horarios
```

### 🔄 **Optimizaciones de Performance**

1. **Polling PIR**: 100ms (óptimo para retardo interno PIR)
2. **WebSocket**: 500ms (tiempo real sin saturar)
3. **Memoria**: Variables no-volátiles, menos RAM
4. **CPU**: Sin interrupciones, mejor distribución de carga

---

## 📊 **Métricas del Proyecto**

### 📈 **Estadísticas de Código**
- **Líneas de código**: ~1,200 líneas
- **Archivos fuente**: 12 archivos (.cpp/.h)
- **Tests unitarios**: 15 casos de prueba
- **Cobertura**: Control, sensores, comunicación

### 🎯 **Objetivos Académicos Cumplidos**
- ✅ **Sistemas Embebidos**: ESP32, GPIO, timers
- ✅ **Comunicación**: WiFi, HTTP, WebSocket
- ✅ **Sensores**: PIR, lectura digital, polling
- ✅ **Actuadores**: Relays, control de cargas
- ✅ **Interfaz**: HTML5, CSS3, JavaScript
- ✅ **Testing**: Unit tests, validación funcional
- ✅ **Documentación**: README, comentarios, arquitectura

### 📚 **Conceptos Aplicados**
- **Arquitectura de Software**: Modular, escalable
- **Patrones de Diseño**: Observer (WebSocket), State Machine (horarios)
- **Concurrencia**: Loop cooperativo, no-bloqueante
- **Comunicación**: Cliente-servidor, tiempo real
- **Testing**: TDD, casos de uso, validación

---

## 🎓 **Conclusiones Académicas**

### 🎯 **Logros del Proyecto**

1. **Integración Completa**: Hardware + Software funcionando en conjunto
2. **Experiencia Real**: Sistema aplicable en entornos reales
3. **Tecnologías Modernas**: ESP32, WebSocket, responsive design
4. **Metodología**: Testing, documentación, versionado
5. **Trabajo en Equipo Efectivo**: Roles especializados y coordinados

### 🤝 **Contribuciones del Equipo**

- **💻 Jhoel (Software)**: Desarrollo completo del código, arquitectura del sistema, implementación de funcionalidades
- **⚡ Alan (Hardware)**: Diseño de circuitos, prototipado, pruebas físicas, validación de conexiones
- **🔧 Nuvia (Ensamblado)**: Montaje profesional, conexiones físicas, integración hardware-software
- **📋 Erick (Documentación)**: Informe técnico, presentación, manual de usuario, documentación académica

### 🚀 **Competencias Desarrolladas**

- **Programación Embebida**: C++, Arduino Framework
- **Desarrollo Web**: Frontend/Backend integration
- **Arquitectura de Sistemas**: Diseño modular y escalable
- **Testing y Validación**: Casos de prueba automatizados
- **Documentación Técnica**: README, comentarios, diagramas

### 🔮 **Posibles Mejoras Futuras**

- 🌐 **Conectividad**: Integración con WiFi externa, MQTT
- 📱 **App Móvil**: Aplicación nativa Android/iOS
- 🤖 **Machine Learning**: Predicción de patrones de uso
- 🔐 **Seguridad**: Autenticación, encriptación
- 📊 **Analytics**: Base de datos, histórico de uso
- 🌡️ **Sensores Adicionales**: Temperatura, luminosidad
- 🔊 **Alertas**: Notificaciones push, buzzer

---

## 📞 **Información de Contacto**

### 🏫 **Institución**
**Universidad Privada Domingo Savio (UPDS)**  
📍 **Dirección**: [Dirección de la universidad]  
🌐 **Web**: [www.upds.edu.bo](https://www.upds.edu.bo/)  
📧 **Email**: [infoupds.santacruz@upds.edu.bo](mailto:infoupds.santacruz@upds.edu.bo)

### 👥 **Equipo de Desarrollo**
- 🎓 **Jhoel** - 💻 **Desarrollo de Software** (Programación completa del sistema)
- 🎓 **Alan** - ⚡ **Diseño de Circuitos** (Prototipado y pruebas físicas)
- 🎓 **Nuvia** - 🔧 **Ensamblado** (Montaje y conexiones del hardware)
- 🎓 **Erick** - 📋 **Documentación** (Informe técnico y presentación)

### 📚 **Materia**
**Sistemas Digitales II** (Tercer Semestre)  
👨‍🏫 **Docente**: Alex Elder Escobar Peralta  
📅 **Período**: 2025  
🏆 **Calificación**: Pendiente (En evaluación)

---

## 📄 **Licencia**

Este proyecto es desarrollado con fines **académicos y educativos** para la Universidad Privada Domingo Savio (UPDS). 

**© 2025 - Equipo de Desarrollo UPDS**  
*Sistemas Digitales II - Ingeniería en Sistemas - Tercer Semestre*  
**Docente**: Alex Elder Escobar Peralta

---

<div align="center">
  
  **¡Gracias por revisar nuestro proyecto!** 🎉
  
  *Desarrollado con ❤️ por estudiantes de UPDS*  
  **Jhoel** 💻 | **Alan** ⚡ | **Nuvia** 🔧 | **Erick** 📋
  
  [![UPDS](https://img.shields.io/badge/UPDS-Universidad%20Privada%20Domingo%20Savio-blue?style=for-the-badge)](https://www.upds.edu.bo/)
  [![ESP32](https://img.shields.io/badge/ESP32-Arduino-red?style=for-the-badge)](https://www.arduino.cc/)
  [![PlatformIO](https://img.shields.io/badge/PlatformIO-Professional-orange?style=for-the-badge)](https://platformio.org/)
  
</div>
