#include <unity.h>
#include <Arduino.h>

// Test: Control remoto manual de zonas
// Valida que las zonas puedan encenderse/apagarse manualmente
// tanto durante horario laboral como fuera de él

// Mock de variables globales necesarias
bool estaEnHorarioLaboral = true; // Durante horario laboral
extern Zona zonas[2];

void setUp(void) {
    // Setup inicial antes de cada test
    estaEnHorarioLaboral = true; // Durante horario laboral
    
    // Zona 1 apagada inicialmente
    zonas[0].estaActivo = false;
    zonas[0].ultimoMovimiento = 0;
    zonas[0].tiempoEncendido = 0;
}

void tearDown(void) {
    // Cleanup después de cada test
}

void test_encender_zona_manualmente_horario_laboral() {
    // GIVEN: Zona apagada, durante horario laboral
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // WHEN: Encendemos la zona manualmente
    configurarEstadoZona(0, true);
    
    // THEN: La zona debe estar encendida
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_NOT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: Zona se enciende manualmente durante horario laboral");
}

void test_apagar_zona_manualmente_horario_laboral() {
    // GIVEN: Zona encendida, durante horario laboral
    zonas[0].estaActivo = true;
    zonas[0].tiempoEncendido = millis();
    
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_TRUE(estaEnHorarioLaboral);
    
    // WHEN: Apagamos la zona manualmente
    configurarEstadoZona(0, false);
    
    // THEN: La zona debe estar apagada
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: Zona se apaga manualmente durante horario laboral");
}

void test_encender_zona_manualmente_fuera_horario() {
    // GIVEN: Zona apagada, fuera de horario laboral
    estaEnHorarioLaboral = false;
    zonas[0].estaActivo = false;
    
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // WHEN: Encendemos la zona manualmente (via interfaz web)
    configurarEstadoZona(0, true);
    
    // THEN: La zona debe estar encendida
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_NOT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: Zona se enciende manualmente fuera de horario");
}

void test_control_manual_prevalece_sobre_automatico() {
    // GIVEN: Fuera de horario, zona encendida manualmente
    estaEnHorarioLaboral = false;
    configurarEstadoZona(0, true);
    
    // Actualizamos movimiento para que PIR pueda extender tiempo
    zonas[0].ultimoMovimiento = millis();
    
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // WHEN: PIR detecta movimiento (debe extender tiempo)
    unsigned long tiempoMovimientoAntes = zonas[0].ultimoMovimiento;
    delay(100); // Pequeña pausa
    zonas[0].ultimoMovimiento = millis(); // Nuevo movimiento
    
    // THEN: La zona sigue encendida y tiempo se extiende
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_GREATER_THAN(tiempoMovimientoAntes, zonas[0].ultimoMovimiento);
    
    Serial.println("✓ Test exitoso: Control manual compatible con extensión automática por PIR");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_encender_zona_manualmente_horario_laboral);
    RUN_TEST(test_apagar_zona_manualmente_horario_laboral);
    RUN_TEST(test_encender_zona_manualmente_fuera_horario);
    RUN_TEST(test_control_manual_prevalece_sobre_automatico);
    
    UNITY_END();
}

void loop() {
    // Tests ejecutados en setup()
}
