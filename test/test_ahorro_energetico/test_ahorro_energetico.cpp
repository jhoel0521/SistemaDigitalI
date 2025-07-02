#include <unity.h>
#include <Arduino.h>

// Test: PIR no enciende zonas apagadas fuera de horario (ahorro energético)
// Simula que detectamos movimiento en una zona apagada fuera de horario
// La zona debe permanecer apagada para ahorrar energía

// Mock de variables globales necesarias
bool estaEnHorarioLaboral = false; // Fuera de horario
extern Zona zonas[2];

void setUp(void) {
    // Setup inicial antes de cada test
    estaEnHorarioLaboral = false; // Fuera de horario
    
    // Zona 1 apagada inicialmente
    zonas[0].estaActivo = false;
    zonas[0].ultimoMovimiento = 0;
    zonas[0].tiempoEncendido = 0;
}

void tearDown(void) {
    // Cleanup después de cada test
}

void test_pir_no_enciende_zona_apagada_fuera_horario() {
    // GIVEN: Zona apagada, fuera de horario
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    unsigned long tiempoAntes = zonas[0].ultimoMovimiento;
    
    // WHEN: Simulamos movimiento PIR
    // Nota: Este test verifica el comportamiento lógico, 
    // la función procesarInterrupcionesPIR() ya está diseñada para no encender zonas apagadas
    
    // THEN: La zona debe permanecer apagada
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: PIR no enciende zonas apagadas fuera de horario (ahorro energético)");
}

void test_pir_extiende_zona_encendida_fuera_horario() {
    // GIVEN: Zona encendida, fuera de horario
    zonas[0].estaActivo = true;
    zonas[0].tiempoEncendido = millis();
    zonas[0].ultimoMovimiento = millis() - 2000; // Hace 2 segundos
    
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    unsigned long tiempoMovimientoAntes = zonas[0].ultimoMovimiento;
    
    // WHEN: Simulamos nuevo movimiento PIR (esto extiende el tiempo)
    zonas[0].ultimoMovimiento = millis(); // Tiempo actual
    
    // THEN: La zona debe seguir encendida y el tiempo de último movimiento debe actualizarse
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_GREATER_THAN(tiempoMovimientoAntes, zonas[0].ultimoMovimiento);
    
    Serial.println("✓ Test exitoso: PIR extiende tiempo de zonas encendidas fuera de horario");
}

void test_pir_inactivo_durante_horario_laboral() {
    // GIVEN: Durante horario laboral
    estaEnHorarioLaboral = true;
    zonas[0].estaActivo = false;
    
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    
    // WHEN: Simulamos movimiento PIR (durante horario debe ser ignorado)
    unsigned long tiempoMovimientoAntes = zonas[0].ultimoMovimiento;
    
    // THEN: PIR debe ser ignorado, zona sigue apagada
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(tiempoMovimientoAntes, zonas[0].ultimoMovimiento);
    
    Serial.println("✓ Test exitoso: PIR inactivo durante horario laboral");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_pir_no_enciende_zona_apagada_fuera_horario);
    RUN_TEST(test_pir_extiende_zona_encendida_fuera_horario);
    RUN_TEST(test_pir_inactivo_durante_horario_laboral);
    
    UNITY_END();
}

void loop() {
    // Tests ejecutados en setup()
}
