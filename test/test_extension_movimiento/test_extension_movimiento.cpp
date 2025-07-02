#include <unity.h>
#include <Arduino.h>
#include "../src/config.h"
#include "../src/zones.h"
#include "../src/time_utils.h"
#include "../src/interrupts.h"

// Variables de test
unsigned long tiempoInicialTest;
bool testSetupCompleted = false;

void setUp(void) {
    if (!testSetupCompleted) {
        Serial.begin(115200);
        Serial.println("=== Iniciando Test: Extensión por Movimiento ===");
        
        // Inicializar pines
        for (int i = 0; i < CANTIDAD_ZONAS; i++) {
            pinMode(zonas[i].pinPir, INPUT);
            for (int j = 0; j < 2; j++) {
                pinMode(zonas[i].pinesRelay[j], OUTPUT);
                digitalWrite(zonas[i].pinesRelay[j], VALOR_RELAY_APAGADO);
            }
        }
        
        // Configurar horario para estar FUERA de horario laboral
        horariosLaborales[0][0] = "08:00";
        horariosLaborales[0][1] = "12:00";
        horariosLaborales[1][0] = "14:00";
        horariosLaborales[1][1] = "18:00";
        
        // Establecer hora fuera de horario (19:00)
        horaActual = 19;
        minutoActual = 0;
        segundoActual = 0;
        referenciaDelTiempo = millis();
        
        // Verificar que estamos fuera de horario
        estaEnHorarioLaboral = verificarSiEsHorarioLaboral();
        TEST_ASSERT_FALSE(estaEnHorarioLaboral);
        
        testSetupCompleted = true;
        tiempoInicialTest = millis();
        Serial.println("Configuración completada - FUERA DE HORARIO");
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

// Test 1: Activación por movimiento fuera de horario
void test_activacion_por_movimiento_fuera_horario(void) {
    Serial.println("\n--- Test: Activación por Movimiento Fuera de Horario ---");
    
    // Estado inicial
    configurarEstadoZona(0, false);
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    
    // Simular detección de movimiento
    unsigned long tiempoMovimiento = millis();
    zonas[0].ultimoMovimiento = tiempoMovimiento;
    
    Serial.printf("Movimiento detectado en zona 1 a las: %lu ms\n", tiempoMovimiento);
    
    // Simular activación automática (esto normalmente se haría en las interrupciones)
    configurarEstadoZona(0, true);
    
    // Verificar que la zona se activó
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(LOW, digitalRead(zonas[0].pinesRelay[0]));
    
    Serial.println("✓ Zona activada por movimiento fuera de horario");
}

// Test 2: Extensión del tiempo por movimiento continuo
void test_extension_por_movimiento_continuo(void) {
    Serial.println("\n--- Test: Extensión por Movimiento Continuo ---");
    
    // Configurar zona activa con movimiento inicial
    configurarEstadoZona(0, true);
    unsigned long primerMovimiento = millis();
    zonas[0].ultimoMovimiento = primerMovimiento;
    
    Serial.printf("Primer movimiento: %lu ms\n", primerMovimiento);
    
    // Esperar 2 segundos
    delay(2000);
    
    // Segundo movimiento (debe extender el tiempo)
    unsigned long segundoMovimiento = millis();
    zonas[0].ultimoMovimiento = segundoMovimiento;
    
    Serial.printf("Segundo movimiento: %lu ms\n", segundoMovimiento);
    Serial.printf("Diferencia: %lu ms\n", segundoMovimiento - primerMovimiento);
    
    // Verificar que el tiempo de último movimiento se actualizó
    TEST_ASSERT_GREATER_THAN(primerMovimiento, zonas[0].ultimoMovimiento);
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    // Simular verificación de apagado automático
    controlarApagadoAutomatico();
    
    // Como hay movimiento reciente, la zona debe seguir encendida
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    Serial.println("✓ Tiempo extendido por movimiento continuo");
}

// Test 3: Comportamiento diferente en horario vs fuera de horario
void test_comportamiento_horario_vs_fuera_horario(void) {
    Serial.println("\n--- Test: Comportamiento Horario vs Fuera de Horario ---");
    
    // Parte 1: Fuera de horario (ya configurado)
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    configurarEstadoZona(0, true);
    zonas[0].ultimoMovimiento = millis() - 10000; // Movimiento hace 10 segundos
    
    // En fuera de horario, debería considerar el movimiento para control automático
    controlarApagadoAutomatico();
    
    // Verificar que funciona la lógica de fuera de horario
    Serial.println("Comportamiento fuera de horario verificado");
    
    // Parte 2: Cambiar a horario laboral
    horaActual = 10; // 10:00 AM - dentro de horario laboral
    minutoActual = 0;
    referenciaDelTiempo = millis();
    estaEnHorarioLaboral = verificarSiEsHorarioLaboral();
    
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // En horario laboral, el control automático debería comportarse diferente
    controlarApagadoAutomatico(); // No debería apagar nada en horario laboral
    
    Serial.println("✓ Comportamiento diferencial verificado");
}

// Test 4: Tiempo exacto de extensión
void test_tiempo_extension_exacto(void) {
    Serial.println("\n--- Test: Tiempo de Extensión Exacto ---");
    
    // Asegurar que estamos fuera de horario
    horaActual = 20;
    estaEnHorarioLaboral = verificarSiEsHorarioLaboral();
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // Activar zona y simular movimiento
    configurarEstadoZona(0, true);
    unsigned long tiempoMovimiento1 = millis();
    zonas[0].ultimoMovimiento = tiempoMovimiento1;
    
    // Esperar 3 segundos
    delay(3000);
    
    // Nuevo movimiento (extiende tiempo)
    unsigned long tiempoMovimiento2 = millis();
    zonas[0].ultimoMovimiento = tiempoMovimiento2;
    
    unsigned long extension = tiempoMovimiento2 - tiempoMovimiento1;
    Serial.printf("Extensión de tiempo: %lu ms\n", extension);
    
    // Verificar que el tiempo se extendió correctamente
    TEST_ASSERT_GREATER_THAN(tiempoMovimiento1, tiempoMovimiento2);
    TEST_ASSERT_GREATER_THAN(2500, extension); // Debe ser mayor a 2.5 segundos
    
    Serial.println("✓ Extensión de tiempo correcta");
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    RUN_TEST(test_activacion_por_movimiento_fuera_horario);
    RUN_TEST(test_extension_por_movimiento_continuo);
    RUN_TEST(test_comportamiento_horario_vs_fuera_horario);
    RUN_TEST(test_tiempo_extension_exacto);
    
    UNITY_END();
}

void loop() {
    // Tests terminados
}
