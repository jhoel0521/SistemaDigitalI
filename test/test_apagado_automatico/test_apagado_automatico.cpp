#include <unity.h>
#include <Arduino.h>

// Test: Apagado automático después de 5 minutos sin movimiento
// Valida que las zonas se apaguen automáticamente fuera de horario
// cuando no hay movimiento por más de 5 minutos (300,000 ms)

// Mock de variables globales necesarias
bool estaEnHorarioLaboral = false; // Fuera de horario
extern Zona zonas[2];
extern const unsigned long TIEMPO_MAXIMO_ENCENDIDO; // 300000 ms (5 min)

void setUp(void) {
    // Setup inicial antes de cada test
    estaEnHorarioLaboral = false; // Fuera de horario
    
    // Zona 1 encendida inicialmente
    zonas[0].estaActivo = true;
    zonas[0].tiempoEncendido = millis();
    zonas[0].ultimoMovimiento = millis();
}

void tearDown(void) {
    // Cleanup después de cada test
}

void test_apagado_automatico_5_minutos() {
    // GIVEN: Zona encendida, fuera de horario
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_FALSE(estaEnHorarioLaboral);
    
    // Simulamos que han pasado más de 5 minutos sin movimiento
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO + 1000); // 5 min + 1 seg
    zonas[0].tiempoEncendido = millis() - (TIEMPO_MAXIMO_ENCENDIDO + 1000);
    
    // WHEN: Ejecutamos la función de control automático
    controlarApagadoAutomatico();
    
    // THEN: La zona debe haberse apagado automáticamente
    TEST_ASSERT_FALSE(zonas[0].estaActivo);
    TEST_ASSERT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: Zona se apaga automáticamente después de 5 minutos sin movimiento");
}

void test_no_apagado_con_movimiento_reciente() {
    // GIVEN: Zona encendida con movimiento reciente
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    // Movimiento hace menos de 5 minutos
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO - 60000); // 4 min
    
    // WHEN: Ejecutamos la función de control automático
    controlarApagadoAutomatico();
    
    // THEN: La zona debe seguir encendida
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    TEST_ASSERT_NOT_EQUAL(0, zonas[0].tiempoEncendido);
    
    Serial.println("✓ Test exitoso: Zona permanece encendida con movimiento reciente");
}

void test_no_apagado_durante_horario_laboral() {
    // GIVEN: Durante horario laboral, zona encendida sin movimiento
    estaEnHorarioLaboral = true;
    zonas[0].estaActivo = true;
    
    // Sin movimiento por más de 5 minutos
    zonas[0].ultimoMovimiento = millis() - (TIEMPO_MAXIMO_ENCENDIDO + 1000);
    
    // WHEN: Ejecutamos la función de control automático
    controlarApagadoAutomatico();
    
    // THEN: La zona debe seguir encendida (no se apaga durante horario laboral)
    TEST_ASSERT_TRUE(zonas[0].estaActivo);
    
    Serial.println("✓ Test exitoso: No se apagan luces durante horario laboral");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_apagado_automatico_5_minutos);
    RUN_TEST(test_no_apagado_con_movimiento_reciente);
    RUN_TEST(test_no_apagado_durante_horario_laboral);
    
    UNITY_END();
}

void loop() {
    // Tests ejecutados en setup()
}
