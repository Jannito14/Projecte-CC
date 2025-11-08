// ===================================
// 🛰️ ARDUINO SATÉLITE - VERSIÓN 2.1
// Con sensor ultrasónico + servo + protocolo
// ===================================
#include <SoftwareSerial.h>
#include <DHT.h>
#include <Servo.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int led1 = 13;
SoftwareSerial mySerial(10, 11);

// --------------------
// ⭐️ NUEVO 5.3 ⭐️ Variables sensor de distancia
// --------------------
Servo servoMotor;
const int trigPin = 6;
const int echoPin = 7;
bool modoBarrido = true;          // por defecto barrido automático
int anguloActual = 0;             // posición actual del servo
bool falloDistancia = false;      // detecta fallo del sensor
unsigned long tiempoUltimoBarrido = 0;
const unsigned long intervaloBarrido = 500;  // cada 0.5s cambia ángulo

bool transmitiendo = false;
unsigned long lastMillis = 0;
unsigned long intervalo = 3000;

bool esperandoTimeout = false;
unsigned long nextTimeoutHT = 0;
unsigned long nextHT = 0;

// ⭐️ NUEVO 5.3 ⭐️ Función medición ultrasónica
float medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 25000); // timeout 25ms (~4m)
  if (duracion == 0) return -1; // fallo
  float distancia = duracion * 0.034 / 2;
  return distancia;
}

// ⭐️ NUEVO 5.3 ⭐️ Barrido automático del servo
void barridoUltrasonico() {
  if (!modoBarrido) return;
  if (millis() - tiempoUltimoBarrido > intervaloBarrido) {
    tiempoUltimoBarrido = millis();
    anguloActual += 15;
    if (anguloActual > 180) anguloActual = 0;
    servoMotor.write(anguloActual);

    float distancia = medirDistancia();
    if (distancia < 0 || distancia > 400) {
      // fallo de lectura
      falloDistancia = true;
      mySerial.println("6:"); // código 6 = fallo sensor distancia ⭐️ NUEVO 5.3 ⭐️
    } else {
      falloDistancia = false;
      // Enviar distancia y ángulo
      mySerial.print("2:");
      mySerial.print(anguloActual);
      mySerial.print(":");
      mySerial.println(distancia);
    }
  }
}

// --------------------
// Función de comandos
// --------------------
void procesarComando(String comando) {
  int fin = comando.indexOf(':');
  if (fin == -1) return;
  int codigo = comando.substring(0, fin).toInt();
  int inicio = fin + 1;

  if (codigo == 1) { // Cambiar periodo envío DHT
    unsigned long nuevoPeriodo = comando.substring(inicio).toInt() * 1000;
    if (nuevoPeriodo >= 1000 && nuevoPeriodo <= 10000) {
      intervalo = nuevoPeriodo;
      mySerial.println("5:Periodo cambiado");
    }
  } 
  else if (codigo == 2) { // Cambiar orientación del servo manualmente
    int angulo = comando.substring(inicio).toInt();
    servoMotor.write(angulo);
    modoBarrido = false; // desactiva barrido automático ⭐️ NUEVO 5.3 ⭐️
    mySerial.print("5:Servo en ");
    mySerial.println(angulo);
  } 
  else if (codigo == 3) { // Parar transmisión
    transmitiendo = false;
    mySerial.println("5:Transmisión detenida");
  } 
  else if (codigo == 4) { // Reanudar transmisión
    transmitiendo = true;
    mySerial.println("5:Transmisión reanudada");
  } 
  else if (codigo == 5) { // Solicitud de estado
    mySerial.println("5:OK");
  }
  else if (codigo == 6) { // ⭐️ NUEVO 5.3 ⭐️ Activar/desactivar barrido automático
    String modo = comando.substring(inicio);
    if (modo == "1") {
      modoBarrido = true;
      mySerial.println("5:Barrido activado");
    } else {
      modoBarrido = false;
      mySerial.println("5:Barrido detenido");
    }
  }
}

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servoMotor.attach(9);
  dht.begin();
  Serial.begin(9600);
  mySerial.begin(9600);
  mySerial.println("5:Satélite listo");
}

void loop() {
  // Comandos desde tierra
  if (mySerial.available()) {
    String comando = mySerial.readStringUntil('\n');
    comando.trim();
    procesarComando(comando);
  }

  // Envío de datos DHT
  if (transmitiendo && millis() >= nextHT) {
    nextHT = millis() + intervalo;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      if (!esperandoTimeout) {
        esperandoTimeout = true;
        nextTimeoutHT = millis() + 5000;
      }
      if (esperandoTimeout && millis() >= nextTimeoutHT) {
        mySerial.println("3:"); // fallo DHT
        esperandoTimeout = false;
      }
    } else {
      esperandoTimeout = false;
      mySerial.print("1:");
      mySerial.print(t);
      mySerial.print(":");
      mySerial.println(h);
      digitalWrite(led1, HIGH);
      delay(200);
      digitalWrite(led1, LOW);
    }
  }

  // ⭐️ NUEVO 5.3 ⭐️ Barrido automático
  if (transmitiendo) {
    barridoUltrasonico();
  }
}
