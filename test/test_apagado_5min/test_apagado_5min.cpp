#include <unity.h>
#include <Arduino.h>
#include "../src/config.h"
#include "../src/zones.h"
#include "../src/time_utils.h"
#include "../src/interrupts.h"

// Variables de test
bool testSetupCompleted = false;
unsigned long simulatedTime = 0;

void setUp(void) {
    if (!testSetupCompleted) {
        Serial.begin(115200);
        Serial.println("=== Iniciando Test: Apagado Automático 5min ===");
        
        // Inicializar pines
        for (int i = 0; i < CANTIDAD_ZONAS; i++) {
            pinMode(zonas[i].pinPir, INPUT);
            for (int j = 0; j < 2; j++) {
                pinMode(zonas[i].pinesRelay[j], OUTPUT);
                digitalWrite(zonas[i].pinesRelay[j], VALOR_RELAY_APAGADO);
            }
        }
        
        // Configurar horarios laborales
        horariosLaborales[0][0] = "08:00";
        horariosLaborales[0][1] = "12:00";
        horariosLaborales[1][0] = "14:00";
        horariosLaborales[1][1] = "18:00";
        
        testSetupCompleted = true;
        Serial.println("Configuración completada");
    }
}

void tearDown(void) {
    // Limpiar después de cada test
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        configurarEstadoZona(i, false);
        zonas[i].ultimoMovimiento = 0;
    }
    delay(100);
}

// Función auxiliar para simular el paso del tiempo
void simularTiempo(int horas, int minutos) {
    horaActual = horas;
    minutoActual = minutos;
    segundoActual = 0;
    referenciaDelTiempo = millis();
    estaEnHorarioLaboral = verificarSiEsHorarioLaboral();
}

// Test 1: Verificar apagado después de terminar horario laboral
void test_apagado_fin_horario_laboral(void) {
    Serial.println("\n--- Test: Apagado al Terminar Horario Laboral ---");
    
    // Configurar tiempo dentro de horario laboral (17:30)
    simularTiempo(17, 30);
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // Encender zona durante horario laboral
    configurarEstadoZona(0, true);
    zonas[0].ultimoMovimiento = millis();
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    Serial.println("Zona encendida durante horario laboral");
    
    // Cambiar a fuera de horario (18:30)
    simularTiempo(18, 30);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    Serial.println("Cambio a fuera de horario laboral");
    
    // Simular que han pasado exactamente 5 minutos sin movimiento
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO + 1000); // 5min + 1seg
    
    // Ejecutar control automático
    controlarApagadoAutomatico();
    
    // Verificar que la zona se apagó
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(zonas[0].pinesRelay[0]));
    
    Serial.println("✓ Zona apagada después de 5 minutos fuera de horario");
}

// Test 2: No apagar durante horario laboral
void test_no_apagar_durante_horario_laboral(void) {
    Serial.println("\n--- Test: No Apagar Durante Horario Laboral ---");
    
    // Configurar tiempo dentro de horario laboral
    simularTiempo(10, 0); // 10:00 AM
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // Encender zona
    configurarEstadoZona(0, true);
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    // Simular mucho tiempo sin movimiento (más de 5 minutos)
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO * 2); // 10 minutos
    
    // Ejecutar control automático
    controlarApagadoAutomatico();
    
    // Verificar que la zona NO se apagó (porque estamos en horario laboral)
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(LOW, digitalRead(zonas[0].pinesRelay[0]));
    
    Serial.println("✓ Zona mantenida encendida durante horario laboral");
}

// Test 3: Apagado progresivo con tiempo exacto
void test_apagado_tiempo_exacto(void) {
    Serial.println("\n--- Test: Apagado con Tiempo Exacto ---");
    
    // Configurar fuera de horario
    simularTiempo(19, 0);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // Encender zona y registrar tiempo
    configurarEstadoZona(0, true);
    unsigned long tiempoInicial = millis();
    zonas[0].ultimoMovimiento = tiempoInicial;
    
    Serial.printf("Zona encendida a: %lu ms\n", tiempoInicial);
    
    // Test: 4 minutos después - NO debe apagarse
    zonas[0].ultimoMovimiento = tiempoInicial;
    // Simular 4 minutos (240 segundos)
    unsigned long tiempo4min = tiempoInicial;
    
    // Hackear millis() no es posible, así que simulamos
    // Establecer último movimiento hace 4 minutos
    zonas[0].ultimoMovimiento = millis() - 240000; // 4 minutos atrás
    
    controlarApagadoAutomatico();
    TEST_ASSERT_TRUE(zonas[0].estaActivo); // Aún debe estar encendida
    
    Serial.println("4 minutos: Zona aún encendida ✓");
    
    // Test: 5 minutos y 10 segundos después - SÍ debe apagarse
    zonas[0].ultimoMovimiento = millis() - 310000; // 5min 10seg atrás
    
    controlarApagadoAutomatico();
    TEST_ASSERT_FALSE(zonas[0].estaActivo); // Debe estar apagada
    
    Serial.println("5+ minutos: Zona apagada ✓");
}

// Test 4: Múltiples zonas con diferentes tiempos
void test_multiples_zonas_diferentes_tiempos(void) {
    Serial.println("\n--- Test: Múltiples Zonas con Diferentes Tiempos ---");
    
    // Configurar fuera de horario
    simularTiempo(20, 0);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // Encender ambas zonas
    configurarEstadoZona(0, true);
    configurarEstadoZona(1, true);
    
    unsigned long tiempoActual = millis();
    
    // Zona 1: movimiento hace 4 minutos (NO debe apagarse)
    zonas[0].ultimoMovimiento = tiempoActual - 240000; // 4 min
    
    // Zona 2: movimiento hace 6 minutos (SÍ debe apagarse)
    zonas[1].ultimoMovimiento = tiempoActual - 360000; // 6 min
    
    Serial.printf("Zona 1 último movimiento: hace 4 min\n");
    Serial.printf("Zona 2 último movimiento: hace 6 min\n");
    
    // Ejecutar control automático
    controlarApagadoAutomatico();
    
    // Verificar resultados
    TEST_ASSERT_TRUE(zonas[0].estaActivo);   // Zona 1 debe seguir encendida
    TEST_ASSERT_FALSE(zonas[1].estaActivo);  // Zona 2 debe estar apagada
    
    Serial.println("✓ Control diferencial de zonas correcto");
}

// Test 5: Transición exacta de horario
void test_transicion_exacta_horario(void) {
    Serial.println("\n--- Test: Transición Exacta de Horario ---");
    
    // Configurar justo al final del horario laboral (17:59)
    simularTiempo(17, 59);
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // Encender zona
    configurarEstadoZona(0, true);
    zonas[0].ultimoMovimiento = millis();
    
    Serial.println("Zona encendida a las 17:59");
    
    // Cambiar a 18:01 (fuera de horario)
    simularTiempo(18, 1);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    Serial.println("Cambio a 18:01 - fuera de horario");
    
    // En este punto, el último movimiento fue hace muy poco
    // La zona debe mantenerse encendida
    controlarApagadoAutomatico();
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    // Simular 5 minutos después del cambio de horario
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO + 1000);
    
    controlarApagadoAutomatico();
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    
    Serial.println("✓ Transición de horario manejada correctamente");
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_apagado_fin_horario_laboral);
    RUN_TEST(test_no_apagar_durante_horario_laboral);
    RUN_TEST(test_apagado_tiempo_exacto);
    RUN_TEST(test_multiples_zonas_diferentes_tiempos);
    RUN_TEST(test_transicion_exacta_horario);
    
    UNITY_END();
}

void loop() {
    // Tests terminados
}
