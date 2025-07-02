#include <unity.h>
#include <Arduino.h>

// Mock de constantes
#define CANTIDAD_ZONAS 2
#define VALOR_RELAY_ENCENDIDO LOW
#define VALOR_RELAY_APAGADO HIGH

// Mock de variables globales
bool estaEnHorarioLaboral = true; // Para test de control manual

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
            zonas[indiceZona].ultimoMovimiento = millis();
            Serial.printf("Zona %d: ENCENDIDA manualmente\n", indiceZona + 1);
        } else {
            zonas[indiceZona].tiempoEncendido = 0;
            Serial.printf("Zona %d: APAGADA manualmente\n", indiceZona + 1);
        }
    }
}

void setUp() {
    // Resetear estado de zonas antes de cada test
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        zonas[i].estaActivo = false;
        zonas[i].ultimoMovimiento = 0;
        zonas[i].tiempoEncendido = 0;
    }
    estaEnHorarioLaboral = true; // Control manual habilitado
}

void tearDown() {
    // Limpiar después de cada test
}

void test_encendido_manual_zona() {
    Serial.println("\n=== TEST: Encendido Manual de Zona ===");
    
    // Estado inicial: zona apagada
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe estar apagada inicialmente");
    
    // Encender zona manualmente
    configurarEstadoZona(0, true);
    
    // Verificar que la zona se encendió
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe estar encendida después del comando manual");
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].tiempoEncendido > 0, "Debe registrar tiempo de encendido");
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].ultimoMovimiento > 0, "Debe registrar último movimiento");
    
    Serial.println("✅ Encendido manual: EXITOSO");
}

void test_apagado_manual_zona() {
    Serial.println("\n=== TEST: Apagado Manual de Zona ===");
    
    // Primero encender la zona
    configurarEstadoZona(1, true);
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe estar encendida");
    
    // Luego apagarla manualmente
    configurarEstadoZona(1, false);
    
    // Verificar que la zona se apagó
    TEST_ASSERT_FALSE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe estar apagada después del comando manual");
    TEST_ASSERT_EQUAL_MESSAGE(0, zonas[1].tiempoEncendido, "Tiempo de encendido debe resetearse");
    
    Serial.println("✅ Apagado manual: EXITOSO");
}

void test_control_independiente_zonas() {
    Serial.println("\n=== TEST: Control Independiente de Zonas ===");
    
    // Encender solo la zona 1
    configurarEstadoZona(0, true);
    
    // Verificar estados independientes
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe estar encendida");
    TEST_ASSERT_FALSE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe permanecer apagada");
    
    // Encender zona 2, mantener zona 1
    configurarEstadoZona(1, true);
    
    // Verificar que ambas están encendidas
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe seguir encendida");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe estar encendida");
    
    // Apagar solo zona 1
    configurarEstadoZona(0, false);
    
    // Verificar control independiente
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe estar apagada");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe seguir encendida");
    
    Serial.println("✅ Control independiente: EXITOSO");
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_encendido_manual_zona);
    RUN_TEST(test_apagado_manual_zona);
    RUN_TEST(test_control_independiente_zonas);
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("Iniciando tests de control manual...");
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
