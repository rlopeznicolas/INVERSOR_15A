#include <Arduino.h>
#include <Wire.h>
#include <MCP342x.h>
#include <math.h>

// --- Configuración de Pines ---
#define PIN_PULSADOR 16
#define PIN_RL1 32
#define PIN_RL2 33
#define PIN_RL3 25
#define PIN_RL4 26
#define PIN_LED1 23  // Reposo
#define PIN_LED2 19  // Dirección A
#define PIN_LED3 18  // Dirección B
#define PIN_LED4 17  // Corriente detectada

// --- Parámetros ---
const float UMBRAL_CORRIENTE = 0.100; // 100mA
const float CONST_V_A = 0.04167;      // 41.67 mV/A para el LXSR15
const long TIEMPO_DEBOUNCE = 100;
const long TIEMPO_DEBUG = 2000;
const long TIEMPO_MUESTREO = 100;

MCP342x adc(0x68); 
enum Estado { REPOSO, MODO_A, REPOSO_HACIA_B, MODO_B };
Estado estadoActual = REPOSO;

float lecturasCorriente[8] = {0};
int indiceLectura = 0;
float ultimaCorriente = 0; 
float mediaCorriente = 0;  
float ultimoVoltaje = 0;

unsigned long lastDebounceTime = 0, lastSampleTime = 0, lastDebugTime = 0;
bool ultimoEstadoBoton = HIGH, botonPresionado = false;

MCP342x::Config configADC(MCP342x::channel1, MCP342x::continuous, MCP342x::resolution16, MCP342x::gain1);

void actualizarSalidas() {
    // Reset de Relés
    digitalWrite(PIN_RL1, LOW); digitalWrite(PIN_RL2, LOW);
    digitalWrite(PIN_RL3, LOW); digitalWrite(PIN_RL4, LOW);
    
    // Reset de LEDs de Estado (Importante para que no se queden encendidos varios a la vez)
    digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, LOW);
    digitalWrite(PIN_LED3, LOW);

    // Activación según estado
    switch (estadoActual) {
        case REPOSO:
        case REPOSO_HACIA_B:
            digitalWrite(PIN_LED1, HIGH);
            break;
        case MODO_A:
            digitalWrite(PIN_RL1, HIGH);
            digitalWrite(PIN_RL4, HIGH);
            digitalWrite(PIN_LED2, HIGH);
            break;
        case MODO_B:
            digitalWrite(PIN_RL2, HIGH);
            digitalWrite(PIN_RL3, HIGH);
            digitalWrite(PIN_LED3, HIGH);
            break;
    }
}

void leerADC() {
    long valorRaw;
    MCP342x::Config resConfig;
    
    if (adc.read(valorRaw, resConfig) == MCP342x::errorNone) {
        MCP342x::normalise(valorRaw, resConfig);
        ultimoVoltaje = (float)valorRaw / 524288.0; 
        ultimaCorriente = ultimoVoltaje / CONST_V_A;

        // Media móvil
        lecturasCorriente[indiceLectura] = ultimaCorriente;
        indiceLectura = (indiceLectura + 1) % 8;
        float suma = 0;
        for(int i=0; i<8; i++) suma += lecturasCorriente[i];
        mediaCorriente = suma / 8.0;
    }
}

bool esSeguroConmutar() {
    return (fabs(mediaCorriente) < UMBRAL_CORRIENTE && fabs(ultimaCorriente) < UMBRAL_CORRIENTE);
}

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22); 

    pinMode(PIN_PULSADOR, INPUT_PULLUP);
    int pins[] = {PIN_RL1, PIN_RL2, PIN_RL3, PIN_RL4, PIN_LED1, PIN_LED2, PIN_LED3, PIN_LED4};
    for(int p : pins) pinMode(p, OUTPUT);

    adc.convert(configADC);
    actualizarSalidas();
    Serial.println("Firmware Inversor v1.1 - Control de LEDs corregido");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Gestión de Corriente
    if (currentMillis - lastSampleTime >= TIEMPO_MUESTREO) {
        lastSampleTime = currentMillis;
        leerADC();
        // LED4 independiente: se enciende si hay flujo de corriente real
        digitalWrite(PIN_LED4, (fabs(ultimaCorriente) >= UMBRAL_CORRIENTE) ? HIGH : LOW);
    }

    // 2. Gestión de Pulsador
    bool lecturaBoton = digitalRead(PIN_PULSADOR);
    if (lecturaBoton != ultimoEstadoBoton) lastDebounceTime = currentMillis;

    if ((currentMillis - lastDebounceTime) > TIEMPO_DEBOUNCE) {
        if (lecturaBoton == LOW && !botonPresionado) {
            botonPresionado = true;
            
            if (esSeguroConmutar()) {
                if (estadoActual == REPOSO) estadoActual = MODO_A;
                else if (estadoActual == MODO_A) estadoActual = REPOSO_HACIA_B;
                else if (estadoActual == REPOSO_HACIA_B) estadoActual = MODO_B;
                else estadoActual = REPOSO;
                
                actualizarSalidas();
            } else {
                Serial.println("Bloqueado: Corriente presente.");
            }
        }
    }
    if (lecturaBoton == HIGH) botonPresionado = false;
    ultimoEstadoBoton = lecturaBoton;

    // 3. Debug
    if (currentMillis - lastDebugTime >= TIEMPO_DEBUG) {
        lastDebugTime = currentMillis;
        Serial.printf("V: %.5f | I: %.3fA | Avg: %.3fA | Est: %d\n", 
                      ultimoVoltaje, ultimaCorriente, mediaCorriente, estadoActual);
    }
}