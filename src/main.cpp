#include <Arduino.h>

// LED Pin (on-board LED)
#define LED_PIN LED_BUILTIN

void setup()
{
  // Serielle Kommunikation initialisieren
  Serial.begin(115200);

  // LED Pin als Ausgang konfigurieren
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Arduino Sketch gestartet!");
}

void loop()
{
  // LED einschalten
  digitalWrite(LED_PIN, HIGH);
  Serial.println("Hello");
  delay(500);

  // LED ausschalten
  digitalWrite(LED_PIN, LOW);
  delay(500);
}