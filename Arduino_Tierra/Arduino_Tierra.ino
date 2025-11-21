// =======================
// 🏠 ARDUINO ESTACIÓN DE TIERRA - V2.6
// Reenvío entre satélite (SoftwareSerial) y PC (Serial USB) + verificación checksum
// =======================

#include <SoftwareSerial.h>

// Comunicación con satélite (SoftwareSerial)
SoftwareSerial satSerial(10, 11); // RX, TX hacia satélite

const int ledRx = 12;     // LED que parpadea al recibir datos
const int ledAlerta = 13; // LED de alerta (fallo sensor o checksum)

unsigned long ultimoMensaje = 0;
const unsigned long timeout = 5000; // 5 segundos sin mensaje -> alerta

// 🔵 NUEVO CHECKSUM: función para calcular checksum
byte calcularChecksum(String mensaje) { // 🔵 NUEVO CHECKSUM
  byte suma = 0;
  for (int i = 0; i < mensaje.length(); ++i) suma += (byte)mensaje[i];
  return suma;
}

// 🔵 NUEVO CHECKSUM: validar mensaje con formato PAYLOAD*CS
// Si OK -> devuelve true y pone payloadOut sin el CS
bool validarMensajeConChecksum(String linea, String &payloadOut) { // 🔵 NUEVO CHECKSUM
  linea.trim();
  int sep = linea.lastIndexOf('*');
  if (sep == -1) return false;
  payloadOut = linea.substring(0, sep);
  String csStr = linea.substring(sep + 1);
  byte csRec = (byte)csStr.toInt();
  byte csCalc = calcularChecksum(payloadOut);
  return (csRec == csCalc);
}

void setup() {
  pinMode(ledRx, OUTPUT);
  pinMode(ledAlerta, OUTPUT);

  Serial.begin(9600);
  satSerial.begin(9600);

  Serial.println("Estación de Tierra lista");
  ultimoMensaje = millis();
}

void loop() {
  // 1️⃣ Recepción de datos del satélite (con checksum) ---------------------
  if (satSerial.available()) {
    String line = satSerial.readStringUntil('\n');
    line.trim();
    ultimoMensaje = millis();

    digitalWrite(ledRx, HIGH);
    delay(50);
    digitalWrite(ledRx, LOW);

    String payload;
    // 🔵 NUEVO CHECKSUM: validar antes de reenviar al PC
    if (validarMensajeConChecksum(line, payload)) {
      // ✅ Checksum correcto -> reenviamos SOLO el payload (sin *CS) al PC
      digitalWrite(ledAlerta, LOW);
      Serial.println(payload); // ❗ mantenemos formato antiguo para compatibilidad Python
    } else {
      // ❗ Checksum inválido -> descartar y encender alerta
      digitalWrite(ledAlerta, HIGH);
      Serial.println("ERR:CHECKSUM"); // para depuración en Python
      // opcional: podríamos solicitar retransmisión (no implementado)
    }
  }

  // 2️⃣ Timeout sin comunicación
  if (millis() - ultimoMensaje > timeout) {
    digitalWrite(ledAlerta, HIGH);
    Serial.println("⚠️ Sin comunicación con el satélite");
  }

  // 3️⃣ Reenvío de comandos desde interfaz hacia el satélite (añadir checksum)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() > 0) {
      // 🔵 NUEVO CHECKSUM: calcular CS e insertar "cmd*CS"
      byte cs = calcularChecksum(cmd);
      String toSend = cmd + "*" + String(cs);
      satSerial.println(toSend);
      // opcional: eco para la interfaz (confirmación)
      Serial.println("SENT_TO_SAT:" + toSend);
    }
  }
}
