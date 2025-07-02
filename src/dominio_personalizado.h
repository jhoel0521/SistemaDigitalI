#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "mi_webserver.h"

// Configuración DNS personalizado
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Dominios personalizados que queremos capturar
const char* DOMINIO_PERSONALIZADO = "micasita.com";
const char* DOMINIO_LOCAL = "micasita.local";

class DominioPersonalizado {
public:
    static void configurar() {
        // Configurar mDNS para dominio local
        if (MDNS.begin("micasita")) {
            Serial.println("✅ mDNS configurado: http://micasita.local");
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("ws", "tcp", 81);
        }
        
        // Configurar DNS Server para capturar cualquier dominio
        dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        Serial.println("✅ DNS Server configurado para capturar todos los dominios");
        Serial.printf("✅ Acceso via: http://%s\n", DOMINIO_PERSONALIZADO);
        Serial.printf("✅ También: http://%s\n", DOMINIO_LOCAL);
        Serial.printf("✅ IP directa: http://%s\n", WiFi.softAPIP().toString().c_str());
    }
    
    static void actualizar() {
        dnsServer.processNextRequest();
    }
    
    static void manejarDominioPersonalizado(WebServer& servidor) {
        // Interceptar todas las peticiones y redirigir si es necesario
        servidor.onNotFound([&servidor]() {
            String host = servidor.hostHeader();
            Serial.printf("Petición recibida de host: %s\n", host.c_str());
            
            // Si no es nuestro dominio, redirigir
            if (host != DOMINIO_PERSONALIZADO && 
                host != DOMINIO_LOCAL && 
                host != WiFi.softAPIP().toString()) {
                
                String redirect = "http://" + String(DOMINIO_PERSONALIZADO);
                servidor.sendHeader("Location", redirect);
                servidor.send(302, "text/plain", "Redirigiendo a " + redirect);
                return;
            }
            
            // Si es nuestro dominio, servir la página principal
            manejarPaginaPrincipal();
        });
    }
};

// Función para incluir en main.cpp
void configurarDominioPersonalizado() {
    DominioPersonalizado::configurar();
}

void actualizarDominioPersonalizado() {
    DominioPersonalizado::actualizar();
}
