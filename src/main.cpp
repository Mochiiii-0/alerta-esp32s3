
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
    if (estadoBoton == LOW) return true;  
  }
  return false;
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " OK" : " FALLÓ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

bool enviarEvento(const char* evento) {
  conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(PC_URL);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"dispositivo\":\"ESP32-S3\",\"evento\":\"") + evento +
                "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"mac\":\"" + WiFi.macAddress() + "\"}";
  int code = http.POST(body);
  http.end();

  Serial.printf("Evento %s enviado, respuesta HTTP: %d\n", evento, code);
  return code == 200;
}

bool enviarNotificacion() { return enviarEvento("ALERTA"); }

// Parpadea el LED mientras intenta conectar al WiFi (máx. 15 s).
// Al terminar: conectado = LED fijo 2 s; sin WiFi = 10 parpadeos rápidos ( esto solo lo puse porque no sabia si me estaba leyendo:)).
void conectarWiFiConLed() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf(" OK\nIP: %s\n", WiFi.localIP().toString().c_str());
    digitalWrite(PIN_LED, HIGH);
    delay(2000);
  } else {
    Serial.println(" FALLÓ");
    for (int i = 0; i < 20; i++) {
      digitalWrite(PIN_LED, !digitalRead(PIN_LED));
      delay(80);
    }
  }
  digitalWrite(PIN_LED, LOW);
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // espera al monitor USB

  pinMode(PIN_BOTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  conectarWiFiConLed();
  enviarEvento("CONECTADA");  // aviso al PC de que la placa está en línea
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
      if (presion) {  
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
