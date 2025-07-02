# Tests del Sistema de Control de Luces

Este directorio contiene tres conjuntos de tests para validar el funcionamiento del sistema de control de luces inteligente.

## Tests Disponibles

### 1. test_control_remoto
**Objetivo**: Probar el encendido y apagado remoto de las zonas
**Ubicación**: `test/test_control_remoto/`

**Tests incluidos**:
- ✅ Encender zona remotamente via HTTP GET `/on?zona=0`
- ✅ Apagar zona remotamente via HTTP GET `/off?zona=0`
- ✅ Control de múltiples zonas independientes
- ✅ Verificación de estado de relays (HIGH/LOW)

### 2. test_extension_movimiento
**Objetivo**: Probar la extensión del tiempo de encendido por movimiento del sensor cuando está fuera de horario
**Ubicación**: `test/test_extension_movimiento/`

**Tests incluidos**:
- ✅ Activación por movimiento fuera de horario laboral
- ✅ Extensión del tiempo por movimiento continuo
- ✅ Comportamiento diferencial: horario vs fuera de horario
- ✅ Verificación de tiempo exacto de extensión

### 3. test_apagado_5min
**Objetivo**: Probar que las luces se apagan automáticamente 5 minutos después de que termine el horario laboral
**Ubicación**: `test/test_apagado_5min/`

**Tests incluidos**:
- ✅ Apagado automático al terminar horario laboral
- ✅ NO apagar durante horario laboral (sin importar el tiempo)
- ✅ Apagado con tiempo exacto (4min NO, 5+min SÍ)
- ✅ Control diferencial de múltiples zonas
- ✅ Transición exacta de horario laboral

## Cómo Ejecutar los Tests

### Opción 1: PlatformIO CLI
```bash
# Ejecutar todos los tests
pio test

# Ejecutar un test específico
pio test -f test_control_remoto
pio test -f test_extension_movimiento
pio test -f test_apagado_5min

# Ejecutar con verbose para más detalles
pio test -v
```

### Opción 2: VS Code con PlatformIO
1. Abrir la paleta de comandos (Ctrl+Shift+P)
2. Buscar "PlatformIO: Test"
3. Seleccionar el test específico que deseas ejecutar

### Opción 3: Desde Terminal
```powershell
# Navegar al directorio del proyecto
cd "C:\Users\ESTE-PC-01\Documents\GitHub\SistemaDigital"

# Ejecutar tests
C:\Users\ESTE-PC-01\.platformio\penv\Scripts\platformio.exe test
```

## Configuración de Hardware para Tests

### Conexiones Necesarias:
- **Zona 1**: 
  - PIR: Pin 13
  - Relay 1: Pin 32
  - Relay 2: Pin 25
- **Zona 2**:
  - PIR: Pin 15  
  - Relay 1: Pin 26
  - Relay 2: Pin 21

### WiFi para Tests Remotos:
- **SSID**: SistemaDigitales
- **Password**: 12345678
- **IP**: Se asigna automáticamente (usualmente 192.168.4.1)

## Interpretación de Resultados

### Salida Exitosa:
```
=== Iniciando Test: Control Remoto ===
IP del servidor: http://192.168.4.1

--- Test: Encender Zona 1 Remotamente ---
Petición: http://192.168.4.1/on?zona=0
Código HTTP: 200
✓ Zona encendida correctamente

3 Tests 0 Failures 0 Ignored
OK
```

### Salida con Errores:
```
--- Test: Encender Zona 1 Remotamente ---
test_control_remoto.cpp:XX:test_encender_zona_remotamente:FAIL: Expected TRUE Was FALSE
```

## Notas Importantes

1. **Tiempo Real**: Los tests utilizan `millis()` real, algunos tests pueden tardar varios segundos en completarse.

2. **Estado de Hardware**: Los tests modifican el estado real de los pines GPIO, asegúrate de que no haya hardware sensible conectado.

3. **Tests Independientes**: Cada test limpia su estado en `tearDown()`, por lo que son independientes entre sí.

4. **Memoria**: Los tests se ejecutan en el ESP32, ten en cuenta las limitaciones de memoria.

## Resolución de Problemas

### Error de Compilación:
- Verificar que `test_framework = unity` esté en `platformio.ini`
- Verificar que `test_build_src = yes` esté configurado

### Tests Fallan:
- Verificar conexiones de hardware
- Revisar configuración de WiFi
- Comprobar que los pines estén correctamente definidos en `config.h`

### WiFi No Conecta:
- Verificar que el ESP32 tenga suficiente energía
- Comprobar que no haya interferencias WiFi
- Verificar credenciales en `config.cpp`
