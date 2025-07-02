# Comportamiento de Ahorro Energético del Sistema
## Sistema de Control Inteligente de Luces UPDS

### 📋 Resumen del Comportamiento

El sistema implementa un **comportamiento inteligente de ahorro energético** que cambia según el horario:

### 🏢 Durante Horario Laboral (8:00 AM - 6:00 PM)
- **Sensores PIR**: ❌ **INACTIVOS** (ignorados completamente)
- **Control**: ✅ **Manual únicamente** via interfaz web
- **Apagado automático**: ❌ **Deshabilitado**
- **Objetivo**: Permitir trabajo sin interrupciones

### 🌙 Fuera de Horario (6:00 PM - 8:00 AM)
- **Sensores PIR**: ✅ **ACTIVOS** con lógica de ahorro energético
- **Control**: ✅ **Manual + Automático**
- **Apagado automático**: ✅ **Activado** (5 minutos sin movimiento)

#### 🔋 Lógica de Ahorro Energético de Sensores PIR

**IMPORTANTE**: Los sensores PIR fuera de horario **NUNCA encienden zonas apagadas**

1. **Zona APAGADA + Movimiento PIR** → ❌ **NO se enciende** (ahorro energético)
2. **Zona ENCENDIDA + Movimiento PIR** → ✅ **Extiende tiempo** de actividad
3. **Sin movimiento > 5 minutos** → ❌ **Apagado automático**

### 📊 Tabla de Comportamientos

| Situación | Horario Laboral | Fuera de Horario |
|-----------|----------------|------------------|
| PIR detecta movimiento en zona apagada | ❌ Ignorado | ❌ **No enciende** (ahorro) |
| PIR detecta movimiento en zona encendida | ❌ Ignorado | ✅ **Extiende tiempo** |
| Control manual web | ✅ Funciona | ✅ Funciona |
| Apagado automático 5 min | ❌ Deshabilitado | ✅ Activo |

### 🎯 Objetivos Logrados

1. **Ahorro de Energía**: Las luces no se encienden automáticamente fuera de horario
2. **Comodidad**: Durante trabajo, control manual sin interrupciones
3. **Seguridad**: Las luces encendidas se extienden con movimiento para navegación segura
4. **Eficiencia**: Apagado automático previene desperdicio energético

### 🔧 Implementación Técnica

- **Función principal**: `procesarInterrupcionesPIR()` en `src/interrupts.cpp`
- **Polling cada 100ms**: Lectura no-bloqueante de sensores PIR
- **Control de zonas**: `configurarEstadoZona()` en `src/zones.cpp`
- **Apagado automático**: `controlarApagadoAutomatico()` en `src/zones.cpp`

### ✅ Tests Unitarios

El sistema incluye tests que validan:
- ✅ PIR no enciende zonas apagadas fuera de horario
- ✅ PIR extiende tiempo de zonas encendidas fuera de horario
- ✅ PIR inactivo durante horario laboral
- ✅ Apagado automático después de 5 minutos
- ✅ Control manual en todos los horarios

---
**Universidad Privada Domingo Savio (UPDS)**  
**Materia**: Sistemas Digitales II  
**Proyecto**: Sistema de Control Inteligente de Luces ESP32
