#pragma once

// ---------- Pines ----------
constexpr int PIN_BOTON = 4;   // botón entre GPIO4 y GND (pull-up interno)
constexpr int PIN_LED   = 2;   // LED + resistencia (220-330 Ω) a GND

// ---------- Tiempos ----------
constexpr unsigned long VENTANA_MS   = 30000;  // 30 s para cancelar la alerta
constexpr unsigned long DEBOUNCE_MS  = 50;
constexpr unsigned long REINTENTO_MS = 3000;   // reintento si falla el envío

// ---------- Sensores ----------
constexpr int PIN_PULSO = 5;   // XD-58C: pata S (señal) -> GPIO5 (entrada analógica)
constexpr int PIN_SDA   = 8;   // MPU6050: SDA -> GPIO8
constexpr int PIN_SCL   = 9;   // MPU6050: SCL -> GPIO9

constexpr unsigned long MUESTREO_MS = 10;  // leer los sensores cada 10 ms

// Ritmo cardíaco
constexpr int UMBRAL_BPM         = 110;    // desde aquí empieza el conteo de 30 s
constexpr int PULSO_AMPLITUD_MIN = 25;     // por debajo = no hay dedo / señal plana

// Filtros anti-ruido: APAGADOS (lee como al inicio, aunque sea con ruido).
// Para prenderlos: BASE 300/3800, REGULARIDAD 1.3, IBI 333/1500, CONFIRMA 5000.
constexpr int PULSO_BASE_MIN      = 0;      // 0 = no revisa si el sensor está apagado
constexpr int PULSO_BASE_MAX      = 4096;   // 4096 = no revisa si el sensor está apagado
constexpr float PULSO_REGULARIDAD = 100.0f; // 100 = acepta latidos disparejos
constexpr unsigned long IBI_MIN_MS = 300;   // 200 BPM
constexpr unsigned long IBI_MAX_MS = 2000;  // 30 BPM
constexpr unsigned long RITMO_CONFIRMA_MS = 0;  // 0 = dispara apenas llega a UMBRAL_BPM

// Caídas
constexpr float CAIDA_LIBRE_G             = 0.5f;  // casi sin peso = está cayendo
constexpr float IMPACTO_G                 = 2.5f;  // golpe fuerte = llegó al suelo
constexpr unsigned long VENTANA_IMPACTO_MS = 1000; // el golpe debe llegar en menos de 1 s
