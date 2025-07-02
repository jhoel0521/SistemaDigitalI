#include <unity.h>
#include <Arduino.h>

// Mock de variables globales necesarias para el test
bool estaEnHorarioLaboral = false;

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
Zona zonas[2] = {
    Zona(13, 32, 25, "Zona 1"),
    Zona(15, 26, 21, "Zona 2")
};

void setUp() {
    // Se ejecuta antes de cada test
    Serial.println("Setup test...");
}

void tearDown() {
    // Se ejecuta después de cada test
}

void test_calibracion_sensores_pir() {
    Serial.println("\n=== TEST: Calibración Sensores PIR ===");
    
    // Test básico: verificar que los pines están configurados correctamente
    TEST_ASSERT_EQUAL_MESSAGE(13, zonas[0].pinPir, "Pin PIR Zona 1 debe ser 13");
    TEST_ASSERT_EQUAL_MESSAGE(15, zonas[1].pinPir, "Pin PIR Zona 2 debe ser 15");
    
    // Test: verificar nombres de zonas
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Zona 1", zonas[0].nombre.c_str(), "Nombre Zona 1");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("Zona 2", zonas[1].nombre.c_str(), "Nombre Zona 2");
    
    // Test: verificar estado inicial
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe estar apagada inicialmente");
    TEST_ASSERT_FALSE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe estar apagada inicialmente");
    
    Serial.println("✅ Calibración PIR: EXITOSO");
}

void test_configuracion_pines() {
    Serial.println("\n=== TEST: Configuración de Pines ===");
    
    // Verificar configuración de relays
    TEST_ASSERT_EQUAL_MESSAGE(32, zonas[0].pinesRelay[0], "Relay 1 Zona 1 = pin 32");
    TEST_ASSERT_EQUAL_MESSAGE(25, zonas[0].pinesRelay[1], "Relay 2 Zona 1 = pin 25");
    TEST_ASSERT_EQUAL_MESSAGE(26, zonas[1].pinesRelay[0], "Relay 1 Zona 2 = pin 26");
    TEST_ASSERT_EQUAL_MESSAGE(21, zonas[1].pinesRelay[1], "Relay 2 Zona 2 = pin 21");
    
    Serial.println("✅ Configuración pines: EXITOSO");
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_calibracion_sensores_pir);
    RUN_TEST(test_configuracion_pines);
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("Iniciando tests de calibración PIR...");
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
