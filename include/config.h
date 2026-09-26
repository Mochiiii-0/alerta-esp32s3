#pragma once

// ---------- Pines ----------
constexpr int PIN_BOTON = 4;   // botón entre GPIO4 y GND (pull-up interno)
constexpr int PIN_LED   = 2;   // LED + resistencia (220-330 Ω) a GND

// ---------- Tiempos ----------
constexpr unsigned long VENTANA_MS   = 30000;  // 30 s para cancelar la alerta
constexpr unsigned long DEBOUNCE_MS  = 50;
constexpr unsigned long REINTENTO_MS = 3000;   // reintento si falla el envío
