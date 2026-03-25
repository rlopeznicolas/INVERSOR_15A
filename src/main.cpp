#include <Arduino.h>
#include <Wire.h>
#include <MCP342x.h>

const int PIN_BOTON = 16;   // GPIO16
const int PIN_LED   = 23;   // GPIO23
MCP342x adc;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Serial.println("LXSR15-NPS Dual Channel Monitor");
  Serial.println("CH1: OUT-Vref (diferencial)");
}

void loop() {
  // === CANAL 1 DIFERENCIAL ===
  long result1;
  MCP342x::Config status1;
  error_t error1;
  
  error1 = adc.convertAndRead(MCP342x::channel1, MCP342x::oneShot,
                             MCP342x::resolution16, MCP342x::gain1,
                             5000000UL, result1, status1);
  
  if (error1 == 0 && status1.isReady()) {
    MCP342x::normalise(result1, status1); //21bits
    float volts_diff = result1 / 524288.0f;
    
    float corriente_A = volts_diff / 0.1042f;  // 104,2mV/A LXSR15-NPS
    Serial.print("CH1 Diff (OUT-Vref): ");
    Serial.print(volts_diff, 6); Serial.print(" V → ");
    Serial.print(corriente_A, 3); Serial.println(" A");
  }
  
  Serial.println("---");
  delay(2000);
}

/*
  // Leemos el estado del pulsador
  int estado = digitalRead(PIN_BOTON);

  // Si usas INPUT_PULLUP: reposo = HIGH, pulsado = LOW
  // Aquí simplemente copiamos el nivel al LED
  digitalWrite(PIN_LED, estado);
  */