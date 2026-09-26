// ESP32-S3 - Botón de alerta con ventana de cancelación de 30 s
// ESTABLE --(botón)--> PENDIENTE (LED ON, cuenta 30 s)
// PENDIENTE --(botón antes de 30 s)--> ESTABLE (LED OFF)
// PENDIENTE --(pasan 30 s)--> ENVIADA (notifica al PC por WiFi, LED sigue ON)

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"
#include "secrets.h"

enum Estado { ESTABLE, PENDIENTE, ENVIADA };
Estado estado = ESTABLE;

unsigned long tInicioAlerta = 0;
unsigned long tUltimoIntento = 0;
bool notificado = false;
bool primerIntento = true;

// Debounce
int lecturaAnterior = HIGH;
int estadoBoton = HIGH;
unsigned long tCambio = 0;

bool botonPresionado() {
  int lectura = digitalRead(PIN_BOTON);
  if (lectura != lecturaAnterior) tCambio = millis();
  lecturaAnterior = lectura;

  if (millis() - tCambio > DEBOUNCE_MS && lectura != estadoBoton) {
    estadoBoton = lectura;
    if (estadoBoton == LOW) return true;  // flanco de bajada = presionado
  }
  return false;
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FALLÓ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

bool enviarNotificacion() {
  conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(PC_URL);
  http.addHeader("Content-Type", "application/json");
  String body = "{\"dispositivo\":\"ESP32-S3\",\"evento\":\"ALERTA\",\"mac\":\"" +
                WiFi.macAddress() + "\"}";
  int code = http.POST(body);
  http.end();

  Serial.printf("Notificación enviada, respuesta HTTP: %d\n", code);
  return code == 200;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  WiFi.mode(WIFI_STA);
  conectarWiFi();
  Serial.println("Estado: ESTABLE");
}

void loop() {
  bool presion = botonPresionado();

  switch (estado) {
    case ESTABLE:
      if (presion) {
        estado = PENDIENTE;
        tInicioAlerta = millis();
        digitalWrite(PIN_LED, HIGH);
        Serial.println("Estado: ALERTA (30 s para cancelar)");
      }
      break;

    case PENDIENTE:
      if (presion) {  // cancelado a tiempo
        estado = ESTABLE;
        digitalWrite(PIN_LED, LOW);
        Serial.println("Cancelado -> Estado: ESTABLE");
      } else if (millis() - tInicioAlerta >= VENTANA_MS) {
        estado = ENVIADA;
        notificado = false;
        primerIntento = true;
        Serial.println("30 s cumplidos -> enviando alerta");
      }
      break;

    case ENVIADA:
      // LED queda encendido; se reintenta hasta que el PC confirme
      if (!notificado && (primerIntento || millis() - tUltimoIntento >= REINTENTO_MS)) {
        primerIntento = false;
        tUltimoIntento = millis();
        notificado = enviarNotificacion();
      }
      break;
  }
}
