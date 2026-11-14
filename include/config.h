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
constexpr int PULSO_BASE_MIN      = 300;    // señal promedio pegada abajo = sensor apagado
constexpr int PULSO_BASE_MAX      = 3800;   // señal promedio pegada arriba = sensor mal conectado
constexpr float PULSO_REGULARIDAD = 1.7f;   // el latido más lento no puede durar 70% más que el más rápido
constexpr unsigned long IBI_MIN_MS = 333;   // 180 BPM: más rápido que esto se toma como ruido
constexpr unsigned long IBI_MAX_MS = 1500;  // 40 BPM: más lento que esto se toma como ruido
constexpr unsigned long RITMO_CONFIRMA_MS = 2000;  // el pulso debe estar alto 2 s seguidos

// Caídas
constexpr float CAIDA_LIBRE_G             = 0.5f;  // casi sin peso = está cayendo
constexpr float IMPACTO_G                 = 2.5f;  // golpe fuerte = llegó al suelo
constexpr unsigned long VENTANA_IMPACTO_MS = 1000; // el golpe debe llegar en menos de 1 s
