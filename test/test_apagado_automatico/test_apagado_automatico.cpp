#include <unity.h>
#include <Arduino.h>

// Mock de constantes
#define CANTIDAD_ZONAS 2
#define TIEMPO_MAXIMO_ENCENDIDO 300000 // 5 minutos en milisegundos

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
            Serial.printf("Zona %d: APAGADA por timeout\n", indiceZona + 1);
        }
    }
}

// Mock de función controlarApagadoAutomatico (versión corregida)
void controlarApagadoAutomatico() {
    unsigned long tiempoActual = millis();

    if (estaEnHorarioLaboral) {
        // EN HORARIO LABORAL: NO se apagan automáticamente las luces
        return;
    }
    
    // FUERA DE HORARIO: Control independiente por zona
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        if (zonas[i].estaActivo) {  // Solo verificar zonas que están encendidas
            unsigned long tiempoSinMovimiento = tiempoActual - zonas[i].ultimoMovimiento;
            
            // Si esta zona específica excede 5 minutos sin movimiento, apagarla
            if (tiempoSinMovimiento > TIEMPO_MAXIMO_ENCENDIDO) {
                configurarEstadoZona(i, false);
                Serial.printf("Zona %d apagada por timeout (5 min sin movimiento)\n", i + 1);
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

void test_apagado_automatico_5_minutos() {
    Serial.println("\n=== TEST: Apagado Automático después de 5 Minutos ===");
    
    unsigned long tiempoBase = millis();
    
    // Configurar zona encendida con último movimiento hace más de 5 minutos
    zonas[0].estaActivo = true;
    zonas[0].tiempoEncendido = tiempoBase - (6 * 60 * 1000); // Encendida hace 6 min
    zonas[0].ultimoMovimiento = tiempoBase - (6 * 60 * 1000); // Último movimiento hace 6 min
    
    Serial.printf("Estado inicial - Zona activa: %s, Sin movimiento por: %lu min\n", 
                  zonas[0].estaActivo ? "SI" : "NO",
                  (tiempoBase - zonas[0].ultimoMovimiento) / 60000);
    
    // Ejecutar control de apagado automático
    controlarApagadoAutomatico();
    
    // Verificar que la zona se apagó (6 min > 5 min límite)
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona debe apagarse después de 5+ minutos sin movimiento");
    TEST_ASSERT_EQUAL_MESSAGE(0, zonas[0].tiempoEncendido, "Tiempo de encendido debe resetearse");
    
    Serial.println("✅ Apagado automático: EXITOSO");
}

void test_no_apagado_con_movimiento_reciente() {
    Serial.println("\n=== TEST: NO Apagado con Movimiento Reciente ===");
    
    unsigned long tiempoBase = millis();
    
    // Configurar zona encendida con movimiento reciente (menos de 5 min)
    zonas[1].estaActivo = true;
    zonas[1].tiempoEncendido = tiempoBase - (2 * 60 * 1000); // Encendida hace 2 min
    zonas[1].ultimoMovimiento = tiempoBase - (2 * 60 * 1000); // Último movimiento hace 2 min
    
    Serial.printf("Estado inicial - Zona activa: %s, Sin movimiento por: %lu min\n", 
                  zonas[1].estaActivo ? "SI" : "NO",
                  (tiempoBase - zonas[1].ultimoMovimiento) / 60000);
    
    // Ejecutar control de apagado automático
    controlarApagadoAutomatico();
    
    // Verificar que la zona NO se apagó (2 min < 5 min límite)
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona debe seguir encendida con movimiento reciente");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].tiempoEncendido > 0, "Tiempo de encendido debe mantenerse");
    
    Serial.println("✅ No apagado con movimiento reciente: EXITOSO");
}

void test_control_independiente_timeout() {
    Serial.println("\n=== TEST: Control Independiente de Timeout ===");
    
    unsigned long tiempoBase = millis();
    
    // Zona 1: sin movimiento hace 6 minutos (debe apagarse)
    zonas[0].estaActivo = true;
    zonas[0].ultimoMovimiento = tiempoBase - (6 * 60 * 1000);
    
    // Zona 2: movimiento hace 2 minutos (debe seguir encendida)
    zonas[1].estaActivo = true;
    zonas[1].ultimoMovimiento = tiempoBase - (2 * 60 * 1000);
    
    Serial.printf("Estado inicial - Zona 1: 6min sin movimiento, Zona 2: 2min sin movimiento\n");
    
    // Ejecutar control de apagado automático
    controlarApagadoAutomatico();
    
    // Verificar comportamiento independiente
    TEST_ASSERT_FALSE_MESSAGE(zonas[0].estaActivo, "Zona 1 debe apagarse (6 min sin movimiento)");
    TEST_ASSERT_TRUE_MESSAGE(zonas[1].estaActivo, "Zona 2 debe seguir encendida (2 min sin movimiento)");
    
    Serial.println("✅ Control independiente timeout: EXITOSO");
}

void test_horario_laboral_no_apaga() {
    Serial.println("\n=== TEST: Horario Laboral NO Apaga Automáticamente ===");
    
    // Cambiar a horario laboral
    estaEnHorarioLaboral = true;
    
    unsigned long tiempoBase = millis();
    
    // Configurar zona con tiempo excedido (normalmente se apagaría)
    zonas[0].estaActivo = true;
    zonas[0].ultimoMovimiento = tiempoBase - (10 * 60 * 1000); // 10 min sin movimiento
    
    // Ejecutar control de apagado automático
    controlarApagadoAutomatico();
    
    // Verificar que NO se apagó durante horario laboral
    TEST_ASSERT_TRUE_MESSAGE(zonas[0].estaActivo, "Zona debe seguir encendida durante horario laboral");
    
    Serial.println("✅ No apagado en horario laboral: EXITOSO");
}

void process() {
    UNITY_BEGIN();
    
    RUN_TEST(test_apagado_automatico_5_minutos);
    RUN_TEST(test_no_apagado_con_movimiento_reciente);
    RUN_TEST(test_control_independiente_timeout);
    RUN_TEST(test_horario_laboral_no_apaga);
    
    UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    Serial.begin(115200);
    Serial.println("Iniciando tests de apagado automático...");
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
