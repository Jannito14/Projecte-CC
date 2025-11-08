// ==============================
// 🧪 TEST UNITARIO - SENSOR HC-SR04 + SERVO
// ==============================
#include <Servo.h>

const int trigPin = 6;
const int echoPin = 7;
Servo servoMotor;

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servoMotor.attach(9);
  Serial.println("Iniciando test de sensor ultrasónico...");
}

float medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 25000);
  if (duracion == 0) return -1;
  return duracion * 0.034 / 2;
}

void loop() {
  for (int angulo = 0; angulo <= 180; angulo += 15) {
    servoMotor.write(angulo);
    delay(300);
    float distancia = medirDistancia();
    if (distancia < 0) {
      Serial.println("⚠️  Fallo de lectura del sensor");
    } else {
      Serial.print("Ángulo: ");
      Serial.print(angulo);
      Serial.print("°, Distancia: ");
      Serial.print(distancia);
      Serial.println(" cm");
    }
  }
  delay(1000);
}
