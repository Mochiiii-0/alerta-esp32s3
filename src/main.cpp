
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

#include "config.h"
#include "secrets.h"

enum Estado { ESTABLE, PENDIENTE, ENVIADA };
Estado estado = ESTABLE;

unsigned long tInicioAlerta = 0;
unsigned long tUltimoIntento = 0;
bool notificado = false;
bool primerIntento = true;
// Causas de la alerta: se pueden sumar (por ejemplo RITMO + CAIDA)
const uint8_t C_BOTON = 1, C_RITMO = 2, C_CAIDA = 4;
uint8_t causas = 0;

String textoCausas() {
  String t = "";
  if (causas & C_BOTON) t += "BOTON";
  if (causas & C_RITMO) t += (t.length() ? "+RITMO" : "RITMO");
  if (causas & C_CAIDA) t += (t.length() ? "+CAIDA" : "CAIDA");
  return t;
}

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

// =====================================================================
//  Sensor de pulso XD-58C (salida analógica)
// =====================================================================
int bpm = 0;                     // pulsaciones por minuto (0 = sin lectura)
float lineaBase = 2048;          // valor promedio de la señal
float amplitud = 0;              // tamaño típico de la onda de pulso
bool sobreUmbral = false;        // la onda está en su parte alta
unsigned long tUltimoLatido = 0;
unsigned long tMuestraPulso = 0;
unsigned long ibis[5];           // tiempos entre latidos (ms) de los últimos 5 latidos
int nIbis = 0, iIbi = 0;
bool ritmoArmado = true;         // false = ya avisó; espera a que el pulso baje
const char* estadoPulso = "sin lectura (¿dedo bien puesto?)";
unsigned long tRitmoAltoDesde = 0;  // desde cuándo el pulso está alto (0 = no está alto)

void actualizarPulso() {
  if (millis() - tMuestraPulso < MUESTREO_MS) return;
  tMuestraPulso = millis();
  unsigned long ahora = tMuestraPulso;

  int v = analogRead(PIN_PULSO);
  lineaBase += (v - lineaBase) * 0.005f;                // promedio lento
  amplitud  += (fabsf(v - lineaBase) - amplitud) * 0.01f;

  // Sensor apagado o desconectado: la señal se queda pegada abajo o arriba
  if (lineaBase < PULSO_BASE_MIN || lineaBase > PULSO_BASE_MAX) {
    bpm = 0; nIbis = 0; sobreUmbral = false;
    estadoPulso = "sensor apagado o desconectado (¿prende su luz verde?)";
    return;
  }
  if (amplitud < PULSO_AMPLITUD_MIN) {                  // señal plana: sin dedo
    bpm = 0; nIbis = 0; sobreUmbral = false;
    estadoPulso = "sin lectura (¿dedo bien puesto?)";
    return;
  }

  // Latido = la señal sube por encima de la mitad de la onda
  if (!sobreUmbral && v > lineaBase + amplitud * 0.5f && ahora - tUltimoLatido > 300) {
    sobreUmbral = true;
    unsigned long ibi = ahora - tUltimoLatido;
    tUltimoLatido = ahora;
    if (ibi >= IBI_MIN_MS && ibi <= IBI_MAX_MS) {       // entre 40 y 180 BPM
      ibis[iIbi] = ibi;
      iIbi = (iIbi + 1) % 5;
      if (nIbis < 5) nIbis++;
    } else {
      nIbis = 0;                                        // latido raro: empezar a contar de nuevo
      bpm = 0;
      estadoPulso = "señal irregular (ruido), se ignora";
    }
    if (nIbis == 5) {
      unsigned long suma = 0, minIbi = ibis[0], maxIbi = ibis[0];
      for (int i = 0; i < 5; i++) {
        suma += ibis[i];
        if (ibis[i] < minIbi) minIbi = ibis[i];
        if (ibis[i] > maxIbi) maxIbi = ibis[i];
      }
      // Un corazón late parejo; el ruido da tiempos muy distintos entre "latidos"
      if (maxIbi > minIbi * PULSO_REGULARIDAD) {
        bpm = 0;
        estadoPulso = "señal irregular (ruido), se ignora";
      } else {
        bpm = 60000UL * 5 / suma;                       // promedio de 5 latidos
      }
    }
  } else if (sobreUmbral && v < lineaBase) {
    sobreUmbral = false;
  }

  if (ahora - tUltimoLatido > 2000) {                   // 2 s sin latidos
    bpm = 0; nIbis = 0;
    estadoPulso = "sin lectura (¿dedo bien puesto?)";
  }
}

// true cuando el pulso lleva RITMO_CONFIRMA_MS seguidos en UMBRAL_BPM o más.
// Después de avisar, vuelve a avisar solo cuando el pulso baje de UMBRAL_BPM - 5.
bool ritmoAlto() {
  if (bpm > 0 && bpm < UMBRAL_BPM - 5) ritmoArmado = true;
  if (bpm < UMBRAL_BPM) { tRitmoAltoDesde = 0; return false; }
  if (tRitmoAltoDesde == 0) tRitmoAltoDesde = millis();
  return ritmoArmado && millis() - tRitmoAltoDesde >= RITMO_CONFIRMA_MS;
}

// =====================================================================
//  Acelerómetro MPU6050 (I2C) - detección de caídas
// =====================================================================
const uint8_t MPU_ADDR = 0x68;
bool mpuOk = false;
bool enCaidaLibre = false;
unsigned long tCaidaLibre = 0;
unsigned long tMuestraMPU = 0;

bool iniciarMPU() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);                   // despertar el sensor
  if (Wire.endTransmission() != 0) return false;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C); Wire.write(0x10);                   // rango ±8 g
  Wire.endTransmission();
  return true;
}

// Devuelve la aceleración total en g (1 g = quieto). -1 si falla la lectura.
float leerAceleracionG() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(MPU_ADDR, (uint8_t)6) != 6) return -1;
  uint8_t b[6];
  for (int i = 0; i < 6; i++) b[i] = Wire.read();       // leer en orden
  int16_t ax = (b[0] << 8) | b[1];
  int16_t ay = (b[2] << 8) | b[3];
  int16_t az = (b[4] << 8) | b[5];
  float x = ax / 4096.0f, y = ay / 4096.0f, z = az / 4096.0f;  // ±8 g -> 4096 por g
  return sqrtf(x * x + y * y + z * z);
}

// Caída = caída libre (casi 0 g) seguida de un golpe fuerte en menos de 1 s.
bool detectarCaida() {
  if (!mpuOk || millis() - tMuestraMPU < MUESTREO_MS) return false;
  tMuestraMPU = millis();
  float g = leerAceleracionG();
  if (g < 0) return false;

  if (g < CAIDA_LIBRE_G) {
    if (!enCaidaLibre) { enCaidaLibre = true; tCaidaLibre = millis(); }
  } else if (enCaidaLibre && g > IMPACTO_G) {
    enCaidaLibre = false;
    Serial.printf("Caída detectada (golpe de %.1f g)\n", g);
    return true;
  }
  if (enCaidaLibre && millis() - tCaidaLibre > VENTANA_IMPACTO_MS) enCaidaLibre = false;
  return false;
}

// Pasa a PENDIENTE: LED encendido y empieza el conteo de 30 s
void iniciarConteo(uint8_t nuevas) {
  estado = PENDIENTE;
  causas = nuevas;
  tInicioAlerta = millis();
  digitalWrite(PIN_LED, HIGH);
  Serial.printf("Estado: ALERTA por %s (30 s para cancelar con el botón)\n", textoCausas().c_str());
}

// Durante el conteo se puede sumar otra causa (por ejemplo, una caída mientras el pulso está alto)
void agregarCausa(uint8_t c) {
  if (causas & c) return;
  causas |= c;
  Serial.printf("Se suma otra causa: ahora la alerta es por %s\n", textoCausas().c_str());
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

bool enviarEvento(const char* evento, const char* causa = "") {
  conectarWiFi();
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(PC_URL);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"dispositivo\":\"ESP32-S3\",\"evento\":\"") + evento +
                "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"mac\":\"" + WiFi.macAddress() + "\"";
  if (causa[0] != '\0') body += String(",\"causa\":\"") + causa + "\"";
  body += "}";
  int code = http.POST(body);
  http.end();

  Serial.printf("Evento %s enviado, respuesta HTTP: %d\n", evento, code);
  return code == 200;
}

bool enviarNotificacion() {
  String c = textoCausas();
  return enviarEvento("ALERTA", c.c_str());
}

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
  analogReadResolution(12);                 // pulso: valores de 0 a 4095
  mpuOk = iniciarMPU();
  Serial.println(mpuOk ? "MPU6050 OK" : "MPU6050 no encontrado: revisa SDA/SCL (caídas desactivadas)");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  conectarWiFiConLed();
  enviarEvento("CONECTADA");  // aviso al PC de que la placa está en línea
  Serial.println("Estado: ESTABLE");
}

void loop() {
  bool presion = botonPresionado();
  actualizarPulso();
  bool caida = detectarCaida();

  // Muestra el pulso cada 2 s en el monitor (para revisar el sensor)
  static unsigned long tImpresion = 0;
  if (millis() - tImpresion >= 2000) {
    tImpresion = millis();
    if (bpm > 0) Serial.printf("Pulso: %d BPM\n", bpm);
    else Serial.printf("Pulso: %s\n", estadoPulso);
  }

  switch (estado) {
    case ESTABLE: {
      // Se revisan las tres causas a la vez: si pasan juntas, la alerta las lleva todas
      uint8_t nuevas = 0;
      if (presion) nuevas |= C_BOTON;
      if (caida)   nuevas |= C_CAIDA;
      if (ritmoAlto()) {
        ritmoArmado = false;          // no volver a avisar hasta que el pulso baje
        nuevas |= C_RITMO;
      }
      if (nuevas) iniciarConteo(nuevas);
      break;
    }

    case PENDIENTE:
      if (presion) {  
        estado = ESTABLE;
        digitalWrite(PIN_LED, LOW);
        Serial.println("Cancelado -> Estado: ESTABLE");
      } else {
        // Si durante el conteo pasa otra cosa, se suma a la alerta
        if (caida) agregarCausa(C_CAIDA);
        if (ritmoAlto()) { ritmoArmado = false; agregarCausa(C_RITMO); }
      }
      if (estado == PENDIENTE && millis() - tInicioAlerta >= VENTANA_MS) {
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
