#include <unity.h>
#include "../../src/zones.h"
#include "../../src/config.h"
#include "../../src/time_utils.h"

// Declaraciones forward
extern bool estaEnHorarioLaboral;
extern Zona zonas[CANTIDAD_ZONAS];

// Mock para funciones de tiempo
void setUp() {
    // Configurar estado inicial para tests
    estaEnHorarioLaboral = false; // Fuera de horario para test
    
    // Resetear zonas
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        zonas[i].estaActivo = false;
        zonas[i].ultimoMovimiento = 0;
        zonas[i].tiempoEncendido = 0;
    }
}

void tearDown() {
    // Limpiar después de cada test
}

// Test: Verificar que cada zona se apaga independientemente
void test_apagado_independiente_por_zona() {
    Serial.println("\n=== TEST: Apagado independiente por zona ===");
    
    // Simular que ambas zonas están encendidas
    unsigned long tiempoBase = millis();
    
    // Zona 1: encendida hace 4 minutos, último movimiento hace 6 minutos (debe apagarse)
    zonas[0].estaActivo = true;
    zonas[0].tiempoEncendido = tiempoBase - (4 * 60 * 1000); // 4 min ago
    zonas[0].ultimoMovimiento = tiempoBase - (6 * 60 * 1000); // 6 min ago
    
    // Zona 2: encendida hace 2 minutos, último movimiento hace 2 minutos (debe seguir encendida)
    zonas[1].estaActivo = true;
    zonas[1].tiempoEncendido = tiempoBase - (2 * 60 * 1000); // 2 min ago
    zonas[1].ultimoMovimiento = tiempoBase - (2 * 60 * 1000); // 2 min ago
    
    Serial.printf("Estado inicial - Zona 1: %s, Zona 2: %s\n", 
                  zonas[0].estaActivo ? "ENCENDIDA" : "APAGADA",
                  zonas[1].estaActivo ? "ENCENDIDA" : "APAGADA");
    
    // Ejecutar control de apagado automático
    controlarApagadoAutomatico();
    
    // Verificar resultados
    Serial.printf("Estado final - Zona 1: %s, Zona 2: %s\n", 
                  zonas[0].estaActivo ? "ENCENDIDA" : "APAGADA",
                  zonas[1].estaActivo ? "ENCENDIDA" : "APAGADA");
    
    // Zona 1 debe estar apagada (6 min sin movimiento > 5 min límite)
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debería estar APAGADA (6 min sin movimiento)");
    
    // Zona 2 debe seguir encendida (2 min sin movimiento < 5 min límite)
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debería seguir ENCENDIDA (2 min sin movimiento)");
    
    Serial.println("✅ Test apagado independiente: EXITOSO");
}

// Test: Verificar que el countdown se calcula correctamente por zona
void test_countdown_individual_por_zona() {
    Serial.println("\n=== TEST: Countdown individual por zona ===");
    
    unsigned long tiempoBase = millis();
    
    // Zona 1: último movimiento hace 3 minutos (countdown = 2 min)
    zonas[0].estaActivo = true;
    zonas[0].ultimoMovimiento = tiempoBase - (3 * 60 * 1000); // 3 min ago
    
    // Zona 2: último movimiento hace 1 minuto (countdown = 4 min)
    zonas[1].estaActivo = true;
    zonas[1].ultimoMovimiento = tiempoBase - (1 * 60 * 1000); // 1 min ago
    
    // Calcular countdown para cada zona (similar a websocket.cpp)
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        if (zonas[i].estaActivo && zonas[i].ultimoMovimiento > 0) {
            unsigned long tiempoSinMovimiento = millis() - zonas[i].ultimoMovimiento;
            unsigned long tiempoRestante = 0;
            
            if (tiempoSinMovimiento >= TIEMPO_MAXIMO_ENCENDIDO) {
                tiempoRestante = 0;
            } else {
                tiempoRestante = (TIEMPO_MAXIMO_ENCENDIDO - tiempoSinMovimiento) / 1000;
            }
            
            Serial.printf("Zona %d: tiempo sin movimiento = %lu seg, countdown = %lu seg\n", 
                          i + 1, tiempoSinMovimiento / 1000, tiempoRestante);
            
            if (i == 0) {
                // Zona 1: 3 min sin movimiento = ~2 min restantes
                TEST_ASSERT_TRUE_MESSAGE(tiempoRestante >= 110 && tiempoRestante <= 130, 
                                       "Zona 1 debería tener ~2 min de countdown");
            } else if (i == 1) {
                // Zona 2: 1 min sin movimiento = ~4 min restantes  
                TEST_ASSERT_TRUE_MESSAGE(tiempoRestante >= 230 && tiempoRestante <= 250, 
                                       "Zona 2 debería tener ~4 min de countdown");
            }
        }
    }
    
    Serial.println("✅ Test countdown individual: EXITOSO");
}

// Test: Verificar extensión de tiempo por movimiento
void test_extension_tiempo_por_movimiento() {
    Serial.println("\n=== TEST: Extensión de tiempo por movimiento ===");
    
    unsigned long tiempoBase = millis();
    
    // Zona 1: encendida, cerca del límite de tiempo
    zonas[0].estaActivo = true;
    zonas[0].ultimoMovimiento = tiempoBase - (4 * 60 * 1000 + 30 * 1000); // 4.5 min ago
    
    Serial.printf("Antes del movimiento - Tiempo sin movimiento: %.1f min\n", 
                  (millis() - zonas[0].ultimoMovimiento) / 60000.0);
    
    // Simular detección de movimiento (actualizar ultimoMovimiento)
    zonas[0].ultimoMovimiento = millis();
    
    Serial.printf("Después del movimiento - Tiempo sin movimiento: %.1f seg\n", 
                  (millis() - zonas[0].ultimoMovimiento) / 1000.0);
    
    // Ejecutar control de apagado
    controlarApagadoAutomatico();
    
    // La zona debe seguir encendida porque se detectó movimiento
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, 
                           "Zona 1 debe seguir encendida después de detectar movimiento");
    
    Serial.println("✅ Test extensión por movimiento: EXITOSO");
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_apagado_independiente_por_zona);
    RUN_TEST(test_countdown_individual_por_zona);
    RUN_TEST(test_extension_tiempo_por_movimiento);
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    Serial.begin(115200);
    process();
}

void loop() {
    // Tests terminados
}
#else
int main() {
    process();
    return 0;
}
#endif
