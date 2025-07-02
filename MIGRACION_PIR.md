# Migración de Interrupciones PIR a Lectura por Loop

## ✅ **Cambio Realizado: Interrupciones → Polling**

### 🔧 **Antes (Interrupciones)**:
```cpp
attachInterrupt(digitalPinToInterrupt(zonas[0].pinPir), manejarPIR1, RISING);
attachInterrupt(digitalPinToInterrupt(zonas[1].pinPir), manejarPIR2, RISING);
```

### 🔧 **Ahora (Lectura por Loop)**:
```cpp
void procesarInterrupcionesPIR() {
    for (int i = 0; i < CANTIDAD_ZONAS; i++) {
        bool estadoActual = digitalRead(zonas[i].pinPir);
        if (estadoActual && !estadosAnterioresPIR[i]) {
            // Nuevo movimiento detectado
        }
    }
}
```

## 🎯 **Ventajas del Nuevo Sistema**

### ✅ **Estabilidad**:
- ❌ **Antes**: Múltiples interrupciones por movimiento continuo
- ✅ **Ahora**: Solo detecta transiciones LOW→HIGH (nuevo movimiento)

### ✅ **Performance**:
- ❌ **Antes**: Interrupciones interfieren con WebSocket/Serial
- ✅ **Ahora**: Lectura controlada cada 100ms, no bloquea nada

### ✅ **Escalabilidad**:
- ❌ **Antes**: Limitado por pines de interrupción del ESP32
- ✅ **Ahora**: Fácil agregar más zonas sin límite de pines

### ✅ **Simplicidad**:
- ❌ **Antes**: Variables `volatile`, funciones `IRAM_ATTR`, flags complejas
- ✅ **Ahora**: Código simple, fácil de debuggear

### ✅ **Funcionalidad**:
- ❌ **Antes**: Solo actualiza `ultimoMovimiento`
- ✅ **Ahora**: Detecta movimiento + enciende automáticamente la zona

## 🔧 **Detalles Técnicos**

### **Frecuencia de Lectura**: 100ms
- **¿Por qué?**: PIR tiene retardo interno de 2-10 segundos
- **Resultado**: No perdemos detecciones, consumo mínimo

### **Detección de Cambios**:
```cpp
bool estadoActual = digitalRead(zonas[i].pinPir);
if (estadoActual && !estadosAnterioresPIR[i]) {
    // Solo cuando pasa de LOW a HIGH = movimiento nuevo
}
```

### **Auto-encendido**:
```cpp
if (!zonas[i].estaActivo) {
    configurarEstadoZona(i, true);
    Serial.printf("Zona %d: Encendida automáticamente\n", i + 1);
}
```

## 📊 **Comparación de Performance**

| Aspecto | Interrupciones | Polling (100ms) |
|---------|---------------|-----------------|
| **CPU Usage** | Variable (picos) | Constante (mínimo) |
| **Estabilidad** | Media | Alta |
| **Debugging** | Difícil | Fácil |
| **Escalabilidad** | Limitada | Ilimitada |
| **Interferencias** | Sí (WebSocket) | No |

## 🧪 **Testing**

### **Casos a Probar**:
1. ✅ Movimiento único → Debe encender zona
2. ✅ Movimiento continuo → No múltiples logs
3. ✅ Horario laboral → PIR ignorado
4. ✅ Múltiples zonas → Funcionan independientes
5. ✅ WebSocket → No interferencias

### **Verificación de Logs**:
```
Zona 1: Movimiento detectado (PIR pin 13) en tiempo 1234 ms
Zona 1: Encendida automáticamente por movimiento
Zona 2: Movimiento detectado (PIR pin 15) en tiempo 5678 ms
Zona 2: Encendida automáticamente por movimiento
```

## 🔄 **Compatibilidad**

- ✅ **Loop principal**: Sin cambios (sigue llamando `procesarInterrupcionesPIR()`)
- ✅ **Tests**: Siguen funcionando
- ✅ **WebSocket**: Mejor performance
- ✅ **API**: Sin cambios

## 📝 **Notas de Migración**

1. **Funciones eliminadas**: `manejarPIR1()`, `manejarPIR2()` han sido completamente removidas
2. **Variables eliminadas**: `pir1Disparado`, `pir2Disparado`, etc. ya no se usan
3. **Nuevo comportamiento**: Auto-encendido de zonas por movimiento (antes solo actualizaba tiempo)
4. **Código más limpio**: Sin funciones obsoletas o variables `volatile` innecesarias
