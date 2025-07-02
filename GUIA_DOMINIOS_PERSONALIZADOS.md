# 🏠 Guía de Dominios Personalizados - Sistema ESP32

## 🎯 **Objetivo**
Configurar dominios personalizados como `http://micasita.com` para acceder al ESP32 sin necesidad de recordar direcciones IP.

---

## 🛠️ **Implementación Técnica**

### 📡 **Tecnologías Utilizadas**

#### **1. mDNS (Multicast DNS)**
```cpp
#include <ESPmDNS.h>

// Configurar mDNS para micasita.local
if (MDNS.begin("micasita")) {
    Serial.println("✅ mDNS configurado: http://micasita.local");
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
}
```

#### **2. DNS Server (Captive Portal)**
```cpp
#include <DNSServer.h>

DNSServer dnsServer;
const byte DNS_PORT = 53;

// Capturar TODAS las peticiones DNS
dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
```

#### **3. Redirección HTTP**
```cpp
servidor.onNotFound([]() {
    String host = servidor.hostHeader();
    
    if (host == "micasita.com" || host == "micasita.local") {
        // Servir página principal
        manejarPaginaPrincipal();
    } else {
        // Redirigir a dominio principal
        servidor.sendHeader("Location", "http://micasita.com");
        servidor.send(302, "text/plain", "Redirigiendo...");
    }
});
```

---

## 🌐 **Dominios Disponibles**

### 🏡 **Dominio Principal**
- **URL**: `http://micasita.com`
- **Tecnología**: DNS Server + Captive Portal
- **Ventaja**: Fácil de recordar, funciona siempre

### 📱 **Dominio Local**
- **URL**: `http://micasita.local`
- **Tecnología**: mDNS (Bonjour/Zeroconf)
- **Ventaja**: Estándar de redes locales
- **Nota**: Puede no funcionar en algunas redes corporativas

### 🔧 **Acceso Directo**
- **URL**: `http://192.168.4.1`
- **Tecnología**: IP estática del ESP32
- **Ventaja**: Siempre funciona, método tradicional

---

## 🔄 **Flujo de Funcionamiento**

```
Usuario escribe cualquier URL
         ↓
┌────────────────────────┐
│   DNS Server del ESP32 │  ← Captura TODAS las peticiones DNS
│   (Captive Portal)     │
└────────────────────────┘
         ↓
┌────────────────────────┐
│   Servidor HTTP ESP32  │  ← Recibe petición HTTP
└────────────────────────┘
         ↓
    ¿Dominio válido?
    ├─ SÍ → Servir interfaz web
    └─ NO → Redirigir a micasita.com
```

---

## 🔧 **Configuración del Código**

### **1. Incluir Librerías**
```cpp
#include <ESPmDNS.h>    // Para micasita.local
#include <DNSServer.h>  // Para micasita.com
```

### **2. Variables Globales**
```cpp
DNSServer dnsServer;
const byte DNS_PORT = 53;
```

### **3. Configuración en setup()**
```cpp
// mDNS para http://micasita.local
if (MDNS.begin("micasita")) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
}

// DNS Server para http://micasita.com
dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
```

### **4. Mantenimiento en loop()**
```cpp
void loop() {
    servidor.handleClient();
    dnsServer.processNextRequest();  // Captive Portal
    MDNS.update();                   // mDNS
    // ... resto del código
}
```

---

## 💡 **Ventajas del Sistema**

### ✅ **Para Usuarios**
- **Fácil acceso**: `micasita.com` es fácil de recordar
- **Múltiples opciones**: Varios dominios funcionan
- **Auto-redirección**: Cualquier URL lleva al sistema
- **Sin configuración**: No necesita editar hosts o DNS

### ✅ **Para Desarrollo**
- **Profesional**: Parece un sitio web real
- **Escalable**: Fácil cambiar el dominio
- **Compatible**: Funciona en todos los dispositivos
- **Robusto**: Múltiples métodos de acceso

---

## 🧪 **Pruebas de Funcionamiento**

### **Test 1: Dominio Principal**
```bash
# En navegador o terminal
curl http://micasita.com
# Debe mostrar la interfaz web
```

### **Test 2: mDNS Local**
```bash
# En navegador
http://micasita.local
# Debe funcionar en la mayoría de dispositivos
```

### **Test 3: Redirección**
```bash
# Escribir cualquier dominio
http://ejemplo.com
http://google.com
# Debe redirigir a micasita.com
```

### **Test 4: IP Directa**
```bash
# Acceso tradicional
http://192.168.4.1
# Debe funcionar siempre
```

---

## 🔍 **Resolución de Problemas**

### ❌ **micasita.local no funciona**
- **Causa**: Red corporativa bloquea mDNS
- **Solución**: Usar `micasita.com` o IP directa

### ❌ **micasita.com no funciona**
- **Causa**: DNS del dispositivo no apunta al ESP32
- **Solución**: Conectar primero al WiFi `SistemaDigitales`

### ❌ **Página no carga**
- **Causa**: ESP32 no responde
- **Solución**: Verificar conexión WiFi y reiniciar ESP32

### ❌ **Redirección infinita**
- **Causa**: Conflicto en configuración DNS
- **Solución**: Usar IP directa `192.168.4.1`

---

## 🚀 **Mejoras Futuras**

### 🌐 **HTTPS**
```cpp
// Certificado SSL para https://micasita.com
#include <WiFiClientSecure.h>
```

### 🔐 **Autenticación**
```cpp
// Login con usuario/contraseña
servidor.on("/login", HTTP_POST, manejarLogin);
```

### 📱 **App Móvil**
```cpp
// API REST para app nativa
servidor.on("/api/status", HTTP_GET, manejarAPI);
```

### 🏠 **Dominio Real**
- Registrar dominio real: `micasita.duckdns.org`
- Configurar DDNS para acceso remoto
- Integración con servicios cloud

---

## 📚 **Referencias Técnicas**

- **ESP32 mDNS**: [ESP32 Arduino mDNS Library](https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS)
- **DNS Server**: [ESP32 DNSServer Library](https://github.com/zhouhan0126/DNSServer---esp32)
- **Captive Portal**: [ESP32 Captive Portal Guide](https://randomnerdtutorials.com/esp32-wi-fi-manager-asyncwebserver/)

---

**🏠 Sistema de Control de Luces - UPDS**  
**🎓 Desarrollado por: Jhoel, Nuvia, Erick, Alan**  
**📚 Materia: Sistemas Digitales II**
