// ===================================
// 🛰️ ARDUINO SATÉLITE - VERSIÓN 2.6
// DHT11 + Ultrasonidos + Servo + Media en Arduino opcional + Protocolo + Simulador órbita + CHECKSUM
// ===================================

#include <SoftwareSerial.h>
#include <DHT.h>
#include <Servo.h>
#include <math.h>

// --------------------
// 🔵 NUEVO CHECKSUM: función auxiliar
// --------------------
byte calcularChecksum(String mensaje) { // 🔵 NUEVO CHECKSUM
  byte suma = 0;
  for (int i = 0; i < mensaje.length(); ++i) suma += (byte)mensaje[i];
  return suma;
}

// --------------------
// 📡 Comunicación y sensores (existing)
// --------------------
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int led1 = 13;
SoftwareSerial mySerial(10, 11); // Comunicación con Arduino Tierra (RX,TX)

Servo servoMotor;
const int trigPin = 6;
const int echoPin = 7;
bool modoBarrido = true;
int anguloActual = 0;
bool falloDistancia = false;
unsigned long tiempoUltimoBarrido = 0;
unsigned long intervaloBarrido = 500;

bool transmitiendo = false;
unsigned long nextHT = 0;
unsigned long intervalo = 3000;
bool esperandoTimeout = false;
unsigned long nextTimeoutHT = 0;

// Media Arduino
bool arduinoComputeAvg = false;
const int MAX_WINDOW = 100;
float tempBuffer[MAX_WINDOW];
int tempCount = 0;
int windowN = 10;

bool corrupt_mode = false; // 🔵 NUEVO CHECKSUM: si true, corrompe mensajes después de calcular CS (testing)

// --------------------
// Funciones medias
// --------------------
void pushTemp(float t) {
  if (tempCount < MAX_WINDOW) tempBuffer[tempCount++] = t;
  else {
    for (int i = 0; i < MAX_WINDOW-1; ++i) tempBuffer[i] = tempBuffer[i+1];
    tempBuffer[MAX_WINDOW-1] = t;
  }
}
float computeAvgArduino() {
  int usable = min(tempCount, windowN);
  if (usable <= 0) return NAN;
  float s = 0;
  for (int i = tempCount - usable; i < tempCount; ++i) s += tempBuffer[i];
  return s / (float)usable;
}

// --------------------
// Función medición ultrasónica
// --------------------
float medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duracion = pulseIn(echoPin, HIGH, 25000);
  if (duracion == 0) return -1;
  float distancia = duracion * 0.034 / 2.0;
  return distancia;
}

// --------------------
// Barrido ultrasónico
// --------------------
void barridoUltrasonico() {
  if (!modoBarrido) return;
  if (millis() - tiempoUltimoBarrido >= intervaloBarrido) {
    tiempoUltimoBarrido = millis();
    anguloActual += 15;
    if (anguloActual > 180) anguloActual = 0;
    servoMotor.write(anguloActual);

    float distancia = medirDistancia();
    String payload;
    if (distancia < 0 || distancia > 400) {
      falloDistancia = true;
      payload = "6:"; // fallo distancia
    } else {
      falloDistancia = false;
      payload = "2:" + String(anguloActual) + ":" + String(distancia, 2);
    }

    // 🔵 NUEVO CHECKSUM: calculo y envío con checksum
    byte cs = calcularChecksum(payload);
    String toSend = payload + "*" + String(cs);
    // 🔵 NUEVO CHECKSUM: posibilidad de corromper intencionalmente (para pruebas)
    if (corrupt_mode && payload.length() > 0) {
      // corrompemos cambiando un carácter del payload (después de calcular checksum)
      toSend = String((char) (payload[0] == 'X' ? 'Y' : 'X')) + payload.substring(1) + "*" + String(cs);
    }
    mySerial.println(toSend); // envío al Arduino Tierra
  }
}

// --------------------
// 6.2: SIMULADOR ORBITAL (constantes y funciones)
// --------------------
// 🟢 NUEVO ORBITA
const double G = 6.67430e-11;
const double M = 5.97219e24;
const double R_EARTH = 6371000;
const double ALTITUDE = 400000;
const double EARTH_ROTATION_RATE = 7.2921159e-5;
const unsigned long MILLIS_BETWEEN_UPDATES = 1000;
const double TIME_COMPRESSION = 90.0;
unsigned long nextOrbitMillis = 0;
double r_orbit = 0.0;
double real_orbital_period = 0.0;
bool orbit_simulate = true;

// simulate_orbit: genera payload "10:<time>:<x>:<y>:<z>" y lo envía con checksum
void simulate_orbit(unsigned long nowMillis, double inclination_deg = 0.0, bool apply_ecef = false) { // 🟢 NUEVO ORBITA
  double time_s = ((double)nowMillis / 1000.0) * TIME_COMPRESSION;
  double angle = 2.0 * M_PI * ( time_s / real_orbital_period );
  double incl = inclination_deg * (M_PI / 180.0);
  double x = r_orbit * cos(angle);
  double y = r_orbit * sin(angle) * cos(incl);
  double z = r_orbit * sin(angle) * sin(incl);

  if (apply_ecef) {
    double theta = EARTH_ROTATION_RATE * time_s;
    double x_ecef = x * cos(theta) - y * sin(theta);
    double y_ecef = x * sin(theta) + y * cos(theta);
    x = x_ecef;
    y = y_ecef;
  }

  String payload = "10:" + String(time_s,1) + ":" + String(x,2) + ":" + String(y,2) + ":" + String(z,2);
  // 🔵 NUEVO CHECKSUM
  byte cs = calcularChecksum(payload);
  String toSend = payload + "*" + String(cs);
  if (corrupt_mode) {
    // forzar corrupción para testing
    toSend = String('X') + payload.substring(1) + "*" + String(cs);
  }
  mySerial.println(toSend);
  Serial.println(toSend); // también en puerto USB para depuración
}

// --------------------
// Procesamiento de comandos entrantes (AHORA con verificación de checksum)
// --------------------
void procesarComandoValido(String payload) {
  // payload es el contenido sin checksum validado
  payload.trim();
  if (payload.length() == 0) return;

  // Comandos literales
  if (payload == "Iniciar") {
    transmitiendo = true;
    nextHT = millis() + intervalo;
    mySerial.println("5:Transmisión iniciada*" + String(calcularChecksum("5:Transmisión iniciada"))); // 🔵 NUEVO CHECKSUM
    return;
  }
  if (payload == "Parar") {
    transmitiendo = false;
    mySerial.println("5:Transmisión detenida*" + String(calcularChecksum("5:Transmisión detenida")));
    return;
  }

  int fin = payload.indexOf(':');
  int codigo = (fin == -1) ? payload.toInt() : payload.substring(0, fin).toInt();
  int inicio = (fin == -1) ? payload.length() : fin + 1;

  switch (codigo) {
    case 1: {
      String param = payload.substring(inicio);
      unsigned long nuevoSegs = param.toInt();
      if (nuevoSegs >= 1 && nuevoSegs <= 3600) {
        intervalo = nuevoSegs * 1000UL;
        nextHT = millis() + intervalo;
        mySerial.println("5:Periodo DHT cambiado*" + String(calcularChecksum("5:Periodo DHT cambiado")));
      } else {
        mySerial.println("5:Periodo DHT inválido*" + String(calcularChecksum("5:Periodo DHT inválido")));
      }
      break;
    }
    case 2: {
      String p = payload.substring(inicio);
      int ang = p.toInt();
      if (ang < 0) ang = 0; if (ang > 180) ang = 180;
      servoMotor.write(ang);
      anguloActual = ang;
      modoBarrido = false;
      String ack = "5:Servo en " + String(ang);
      mySerial.println(ack + "*" + String(calcularChecksum(ack)));
      break;
    }
    case 6: {
      String modo = payload.substring(inicio);
      if (modo == "1") { modoBarrido = true; mySerial.println("5:Barrido activado*" + String(calcularChecksum("5:Barrido activado"))); }
      else { modoBarrido = false; mySerial.println("5:Barrido detenido*" + String(calcularChecksum("5:Barrido detenido"))); }
      break;
    }
    case 7: {
      String p = payload.substring(inicio);
      unsigned long msval = p.toInt();
      if (msval < 50) msval = 50; if (msval > 10000) msval = 10000;
      intervaloBarrido = msval;
      String ack = "5:Intervalo ultrasonidos=" + String(intervaloBarrido);
      mySerial.println(ack + "*" + String(calcularChecksum(ack)));
      break;
    }
    case 8: {
      String p = payload.substring(inicio);
      if (p == "1") { arduinoComputeAvg = true; mySerial.println("5:Media en Arduino activada*" + String(calcularChecksum("5:Media en Arduino activada"))); }
      else { arduinoComputeAvg = false; mySerial.println("5:Media en Arduino desactivada*" + String(calcularChecksum("5:Media en Arduino desactivada"))); }
      break;
    }
    case 9: {
      String p = payload.substring(inicio);
      int n = p.toInt();
      if (n < 1) n = 1; if (n > MAX_WINDOW) n = MAX_WINDOW;
      windowN = n;
      String ack = "5:WindowN=" + String(windowN);
      mySerial.println(ack + "*" + String(calcularChecksum(ack)));
      break;
    }
    case 11: {
      String p = payload.substring(inicio);
      if (p == "1") { orbit_simulate = true; mySerial.println("5:Orbit simulation ON*" + String(calcularChecksum("5:Orbit simulation ON"))); }
      else { orbit_simulate = false; mySerial.println("5:Orbit simulation OFF*" + String(calcularChecksum("5:Orbit simulation OFF"))); }
      break;
    }
    case 12: { // 🔵 NUEVO CHECKSUM: toggle corruption mode for testing -> "12:1" / "12:0"
      String p = payload.substring(inicio);
      if (p == "1") { corrupt_mode = true; mySerial.println("5:Corrupt ON*" + String(calcularChecksum("5:Corrupt ON"))); }
      else { corrupt_mode = false; mySerial.println("5:Corrupt OFF*" + String(calcularChecksum("5:Corrupt OFF"))); }
      break;
    }
    default: {
      mySerial.println("5:Comando desconocido*" + String(calcularChecksum("5:Comando desconocido")));
      break;
    }
  }
}

// 🔵 NUEVO CHECKSUM: función que recibe la línea completa (posiblemente con *CS) y valida
bool validateAndProcessIncoming(String line) { // 🔵 NUEVO CHECKSUM
  line.trim();
  if (line.length() == 0) return false;
  int sep = line.lastIndexOf('*');
  if (sep == -1) {
    // No checksum: ignorar
    return false;
  }
  String payload = line.substring(0, sep);
  String csStr = line.substring(sep + 1);
  byte csRec = (byte)csStr.toInt();
  byte csCalc = calcularChecksum(payload);
  if (csRec != csCalc) {
    // checksum incorrecto -> ignorar
    // opcional: enviar NACK
    Serial.println("ERR:CS_MISMATCH");
    return false;
  }
  // checksum OK -> procesar payload
  procesarComandoValido(payload);
  return true;
}

void setup() {
  pinMode(led1, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servoMotor.attach(9);
  anguloActual = 90;
  servoMotor.write(anguloActual);
  dht.begin();
  Serial.begin(9600);
  mySerial.begin(9600);
  mySerial.println("5:Satélite listo*" + String(calcularChecksum("5:Satélite listo")));

  // Inicializar simulador orbital
  r_orbit = R_EARTH + ALTITUDE;
  real_orbital_period = 2.0 * M_PI * sqrt( (r_orbit * r_orbit * r_orbit) / (G * M) );
  nextOrbitMillis = millis() + MILLIS_BETWEEN_UPDATES;
}

void loop() {
  // 🔵 NUEVO CHECKSUM: ahora leemos solo líneas completas con checksum desde mySerial (estación tierra)
  if (mySerial.available()) {
    String incoming = mySerial.readStringUntil('\n');
    incoming.trim();
    // validar y procesar comando solo si checksum OK
    // Nota: en el flujo normal la Estación Tierra enviará comandos ya con checksum
    if (!validateAndProcessIncoming(incoming)) {
      // Si falla checksum, podríamos enviar un ACK de error (opcional)
      // mySerial.println("ERR:CS");
    }
  }

  if (transmitiendo && (long)(millis() - nextHT) >= 0) {
    nextHT = millis() + intervalo;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      if (!esperandoTimeout) {
        esperandoTimeout = true;
        nextTimeoutHT = millis() + 5000;
      }
      if (esperandoTimeout && millis() >= nextTimeoutHT) {
        // Enviar fallo DHT con checksum
        String payload = "3:";
        byte cs = calcularChecksum(payload);
        mySerial.println(payload + "*" + String(cs));
        esperandoTimeout = false;
      }
    } else {
      esperandoTimeout = false;
      pushTemp(t);
      if (arduinoComputeAvg) {
        float avg = computeAvgArduino();
        String payload = "1:" + String(t,2) + ":" + String(h,2) + ":A:" + (isnan(avg) ? String("nan") : String(avg,2));
        byte cs = calcularChecksum(payload);
        String toSend = payload + "*" + String(cs);
        if (corrupt_mode) toSend = String('X') + payload.substring(1) + "*" + String(cs);
        mySerial.println(toSend);
      } else {
        String payload = "1:" + String(t,2) + ":" + String(h,2);
        byte cs = calcularChecksum(payload);
        String toSend = payload + "*" + String(cs);
        if (corrupt_mode) toSend = String('X') + payload.substring(1) + "*" + String(cs);
        mySerial.println(toSend);
      }
      digitalWrite(led1, HIGH);
      delay(120);
      digitalWrite(led1, LOW);
    }
  }

  if (transmitiendo) barridoUltrasonico();

  // Simulador orbital (🟢 NUEVO ORBITA)
  if (orbit_simulate && (long)(millis() - nextOrbitMillis) >= 0) {
    simulate_orbit(millis(), 0.0, false);
    nextOrbitMillis = millis() + MILLIS_BETWEEN_UPDATES;
  }
}
