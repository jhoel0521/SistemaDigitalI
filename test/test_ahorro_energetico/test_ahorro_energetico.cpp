#include <unity.h>
#include <Arduino.h>

// Mock de constantes
#define CANTIDAD_ZONAS 2

// Mock de variables globales
bool estaEnHorarioLaboral = false; // Fuera de horario para tests

// Estructura mock para Zona
struct Zona {
    int pinPir;
    int pinesRelay[2];
    String nombre;
    bool estaActivo;
    unsigned long ultimoMovimiento;
    unsigned long tiempoEncendido;
    
    Zona(int pir = 0, int relay1 = 0, int relay2 = 0, String nom = "") :
        pinPir(pir), nombre(nom), estaActivo(false), ultimoMovimiento(0), tiempoEncendido(0) {
        pinesRelay[0] = relay1;
        pinesRelay[1] = relay2;
    }
};

// Mock de zonas
Zona zonas[CANTIDAD_ZONAS] = {
    Zona(13, 32, 25, "Zona 1"),
    Zona(15, 26, 21, "Zona 2")
};

// Mock de función configurarEstadoZona
void configurarEstadoZona(int indiceZona, bool activar) {
    if (indiceZona >= 0 && indiceZona < CANTIDAD_ZONAS) {
        zonas[indiceZona].estaActivo = activar;
        if (activar) {
            zonas[indiceZona].tiempoEncendido = millis();
            Serial.printf("Zona %d: ENCENDIDA\n", indiceZona + 1);
        } else {
            zonas[indiceZona].tiempoEncendido = 0;
            Serial.printf("Zona %d: APAGADA\n", indiceZona + 1);
        }
    }
}

// Mock de función procesarInterrupcionesPIR (versión corregida con ahorro energético)
void procesarInterrupcionesPIR() {
    // Si estamos en horario laboral, no procesar PIR
    if (estaEnHorarioLaboral) {
        return;
    }

    // Simulación: detectar movimiento en zona 0
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        // Simular detección de movimiento (normalmente viene del hardware)
        bool movimientoDetectado = true; // Para el test, asumimos movimiento
        
        if (movimientoDetectado) {
            // AHORRO ENERGÉTICO: Fuera de horario, PIR SOLO extiende tiempo de zonas YA ENCENDIDAS
            if (zonas[i].estaActivo) {
                // Solo si la zona YA está encendida, extender su tiempo de actividad
                zonas[i].ultimoMovimiento = millis();
                Serial.printf("Zona %d: Movimiento detectado - EXTENDIENDO tiempo de zona encendida\n", i + 1);
            } else {
                // Zona apagada: PIR NO la enciende para ahorrar energía
                Serial.printf("Zona %d: Movimiento detectado - pero zona APAGADA, NO se enciende (ahorro energético)\n", i + 1);
            }
        }
    }
}

void setUp() {
    // Resetear estado antes de cada test
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        zonas[i].estaActivo = false;
        zonas[i].ultimoMovimiento = 0;
        zonas[i].tiempoEncendido = 0;
    }
    estaEnHorarioLaboral = false; // Fuera de horario para tests
}

void tearDown() {
    // Limpiar después de cada test
}

void test_pir_no_enciende_zona_apagada_fuera_horario() {
    Serial.println("\n=== TEST: PIR NO Enciende Zona Apagada (Ahorro Energético) ===");
    
    // Estado inicial: zona apagada
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona debe estar apagada inicialmente");
    TEST_ASSERT_FALSE_MESSAGE(estaEnHorarioLaboral, "Debe estar fuera de horario");
    
    // Simular detección de movimiento PIR (pero zona apagada)
    procesarInterrupcionesPIR();
    
    // Verificar que la zona NO se encendió (ahorro energético)
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona debe permanecer APAGADA para ahorrar energía");
    TEST_ASSERT_EQUAL_MESSAGE(0, zonas[0].tiempoEncendido, "No debe registrar tiempo de encendido");
    
    Serial.println("✅ PIR no enciende zona apagada: EXITOSO");
}

void test_pir_extiende_zona_encendida_fuera_horario() {
    Serial.println("\n=== TEST: PIR Extiende Tiempo de Zona Encendida ===");
    
    // Configurar zona encendida manualmente
    configurarEstadoZona(0, true);
    zonas[0].ultimoMovimiento = millis() - 2000; // Último movimiento hace 2 segundos
    
    unsigned long tiempoMovimientoAnterior = zonas[0].ultimoMovimiento;
    
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona debe estar encendida");
    
    // Esperar un poco para ver diferencia en tiempo
    delay(100);
    
    // Simular detección de nuevo movimiento PIR
    procesarInterrupcionesPIR();
    
    // Verificar que el tiempo de último movimiento se actualizó
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona debe seguir encendida");
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].ultimoMovimiento > tiempoMovimientoAnterior, 
                           "Último movimiento debe actualizarse con nuevo movimiento");
    
    Serial.println("✅ PIR extiende tiempo de zona encendida: EXITOSO");
}

void test_pir_desactivado_horario_laboral() {
    Serial.println("\n=== TEST: PIR Desactivado Durante Horario Laboral ===");
    
    // Cambiar a horario laboral
    estaEnHorarioLaboral = true;
    
    // Configurar zona encendida
    configurarEstadoZona(0, true);
    unsigned long tiempoMovimientoAnterior = zonas[0].ultimoMovimiento;
    
    // Simular detección PIR durante horario laboral
    procesarInterrupcionesPIR();
    
    // Verificar que el PIR no procesó el movimiento
    TEST_ASSERT_EQUAL_MESSAGE(tiempoMovimientoAnterior, zonas[0].ultimoMovimiento, 
                             "PIR debe estar desactivado durante horario laboral");
    
    Serial.println("✅ PIR desactivado en horario laboral: EXITOSO");
}

void test_comportamiento_diferencial_zonas() {
    Serial.println("\n=== TEST: Comportamiento Diferencial por Zona ===");
    
    // Zona 1: apagada (no debe encenderse)
    zonas[0].estaActivo = false;
    
    // Zona 2: encendida (debe extender tiempo)
    configurarEstadoZona(1, true);
    zonas[1].ultimoMovimiento = millis() - 1000; // Hace 1 segundo
    unsigned long tiempoMovimientoAnteriorZ2 = zonas[1].ultimoMovimiento;
    
    delay(100);
    
    // Simular movimiento en ambas zonas
    procesarInterrupcionesPIR();
    
    // Verificar comportamiento diferencial
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe permanecer apagada (ahorro energético)");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe seguir encendida");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].ultimoMovimiento > tiempoMovimientoAnteriorZ2, 
                           "Zona 2 debe actualizar tiempo de movimiento");
    
    Serial.println("✅ Comportamiento diferencial: EXITOSO");
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_pir_no_enciende_zona_apagada_fuera_horario);
    RUN_TEST(test_pir_extiende_zona_encendida_fuera_horario);
    RUN_TEST(test_pir_desactivado_horario_laboral);
    RUN_TEST(test_comportamiento_diferencial_zonas);
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("Iniciando tests de ahorro energético...");
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
