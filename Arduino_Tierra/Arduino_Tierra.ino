#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11);

const int ledDatos = 12;
const int ledFalloDHT = 13;
// ⭐️ NUEVO 5.3 ⭐️ LED adicional para fallo del sensor de distancia
const int ledFalloDistancia = 8;

unsigned long ultimoMensaje = 0;
const unsigned long timeout = 5000;

void setup() {
  pinMode(ledDatos, OUTPUT);
  pinMode(ledFalloDHT, OUTPUT);
  pinMode(ledFalloDistancia, OUTPUT); // ⭐️ NUEVO 5.3 ⭐️
  Serial.begin(9600);
  mySerial.begin(9600);
  ultimoMensaje = millis();
}

void loop() {
  if (mySerial.available()) {
    String data = mySerial.readStringUntil('\n');
    data.trim();
    Serial.println(data);
    ultimoMensaje = millis();

    digitalWrite(ledDatos, HIGH);
    delay(100);
    digitalWrite(ledDatos, LOW);

    int fin = data.indexOf(':');
    if (fin > 0) {
      int codigo = data.substring(0, fin).toInt();
      String valor = data.substring(fin + 1);

      if (codigo == 3) { // fallo DHT
        digitalWrite(ledFalloDHT, HIGH);
      } 
      else if (codigo == 6) { // ⭐️ NUEVO 5.3 ⭐️ fallo sensor distancia
        digitalWrite(ledFalloDistancia, HIGH);
        Serial.println("⚠️  Fallo del sensor de distancia");
      } 
      else {
        digitalWrite(ledFalloDHT, LOW);
        digitalWrite(ledFalloDistancia, LOW);
      }
    }
  }

  if (millis() - ultimoMensaje > timeout) {
    digitalWrite(ledFalloDistancia, HIGH);
    Serial.println("⚠️  Sin comunicación con el satélite");
  }

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    mySerial.println(data);
  }
}
