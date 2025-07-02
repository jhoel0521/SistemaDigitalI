#include <unity.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "../src/config.h"
#include "../src/zones.h"
#include "../src/mi_webserver.h"
#include "../src/websocket.h"
#include "../src/time_utils.h"
#include "../src/interrupts.h"

// Variables de test
HTTPClient http;
String baseURL;
bool testSetupCompleted = false;

void setUp(void) {
    if (!testSetupCompleted) {
        Serial.begin(115200);
        Serial.println("=== Iniciando Test: Control Remoto ===");
        
        // Inicializar pines
        for (int i = 0; i < CANTIDAD_ZONAS; i++) {
            pinMode(zonas[i].pinPir, INPUT);
            for (int j = 0; j < 2; j++) {
                pinMode(zonas[i].pinesRelay[j], OUTPUT);
                digitalWrite(zonas[i].pinesRelay[j], VALOR_RELAY_APAGADO);
            }
        }
        
        // Configurar WiFi como punto de acceso
        WiFi.softAP(ssid, password);
        delay(2000);
        
        // Iniciar servidor
        servidor.begin();
        baseURL = "http://" + WiFi.softAPIP().toString();
        
        Serial.println("IP del servidor: " + baseURL);
        testSetupCompleted = true;
        delay(1000);
    }
}

void tearDown(void) {
    // Limpiar después de cada test
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        configurarEstadoZona(i, false);
    }
    delay(100);
}

// Test 1: Encender zona remotamente
void test_encender_zona_remotamente(void) {
    Serial.println("\n--- Test: Encender Zona 1 Remotamente ---");
    
    // Estado inicial - asegurar que está apagada
    configurarEstadoZona(0, false);
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    
    // Simular petición HTTP GET para encender zona 1
    String url = baseURL + "/on?zona=0";
    http.begin(url);
    int httpCode = http.GET();
    
    Serial.printf("Petición: %s\n", url.c_str());
    Serial.printf("Código HTTP: %d\n", httpCode);
    
    // Procesar la petición en el servidor
    servidor.handleClient();
    delay(100);
    
    // Verificar que la zona se encendió
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(LOW, digitalRead(zonas[0].pinesRelay[0])); // Relay activo en LOW
    TEST_ASSERT_EQUAL(LOW, digitalRead(zonas[0].pinesRelay[1])); // Relay activo en LOW
    
    Serial.println("✓ Zona encendida correctamente");
    http.end();
}

// Test 2: Apagar zona remotamente
void test_apagar_zona_remotamente(void) {
    Serial.println("\n--- Test: Apagar Zona 1 Remotamente ---");
    
    // Estado inicial - encender zona
    configurarEstadoZona(0, true);
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    // Simular petición HTTP GET para apagar zona 1
    String url = baseURL + "/off?zona=0";
    http.begin(url);
    int httpCode = http.GET();
    
    Serial.printf("Petición: %s\n", url.c_str());
    Serial.printf("Código HTTP: %d\n", httpCode);
    
    // Procesar la petición en el servidor
    servidor.handleClient();
    delay(100);
    
    // Verificar que la zona se apagó
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(zonas[0].pinesRelay[0])); // Relay inactivo en HIGH
    TEST_ASSERT_EQUAL(HIGH, digitalRead(zonas[0].pinesRelay[1])); // Relay inactivo en HIGH
    
    Serial.println("✓ Zona apagada correctamente");
    http.end();
}

// Test 3: Control remoto de múltiples zonas
void test_control_multiples_zonas(void) {
    Serial.println("\n--- Test: Control Múltiples Zonas ---");
    
    // Encender zona 1
    String url1 = baseURL + "/on?zona=0";
    http.begin(url1);
    http.GET();
    servidor.handleClient();
    http.end();
    delay(100);
    
    // Encender zona 2
    String url2 = baseURL + "/on?zona=1";
    http.begin(url2);
    http.GET();
    servidor.handleClient();
    http.end();
    delay(100);
    
    // Verificar ambas zonas encendidas
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_TRUE(zonas[1].estaActivo);
    
    // Apagar solo zona 1
    String url3 = baseURL + "/off?zona=0";
    http.begin(url3);
    http.GET();
    servidor.handleClient();
    http.end();
    delay(100);
    
    // Verificar estado final
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_TRUE(zonas[1].estaActivo);
    
    Serial.println("✓ Control múltiples zonas correcto");
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_encender_zona_remotamente);
    RUN_TEST(test_apagar_zona_remotamente);
    RUN_TEST(test_control_multiples_zonas);
    
    UNITY_END();
}

void loop() {
    // Tests terminados
}
