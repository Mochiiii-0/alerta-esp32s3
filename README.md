# Alerta ESP32-S3

Botón de alerta con LED y notificación al computador por WiFi.

## Funcionamiento

| Estado | LED | Qué pasa |
|---|---|---|
| **ESTABLE** | Apagado | Esperando. Al oprimir el botón pasa a PENDIENTE. |
| **PENDIENTE** | Encendido | Hay 30 s para volver a oprimir y cancelar (vuelve a ESTABLE). |
| **ENVIADA** | Encendido | Pasaron los 30 s: se envía la notificación al PC. Si falla, reintenta cada 3 s. |

```
ESTABLE ──botón──▶ PENDIENTE ──30 s──▶ ENVIADA
   ▲                   │
   └──────botón────────┘
```

## Hardware

| Componente | Conexión |
|---|---|
| Botón | GPIO4 ↔ GND (usa pull-up interno) |
| LED | GPIO2 → resistencia 220–330 Ω → LED → GND |

Los pines y tiempos se cambian en [`include/config.h`](include/config.h).

## Estructura

```
alerta-esp32s3/
├── platformio.ini            # configuración de la placa
├── src/main.cpp              # firmware (máquina de estados)
├── include/
│   ├── config.h              # pines y tiempos
│   └── secrets.example.h     # plantilla de WiFi / URL del PC
└── pc/servidor_pc.py         # servidor que recibe la alerta
```

## Uso

### 1. Servidor en el PC

```bash
python pc/servidor_pc.py
```

Muestra la IP donde está escuchando (ej. `http://192.168.1.100:5000/alerta`). El PC y la ESP32 deben estar en la misma red y el firewall debe permitir el puerto 5000.

### 2. Firmware

1. Instala [VS Code](https://code.visualstudio.com/) y la extensión **PlatformIO IDE** (VS Code la sugiere al abrir la carpeta).
2. Copia `include/secrets.example.h` como `include/secrets.h` y pon tu red WiFi, clave y la URL del paso anterior.
3. Conecta la ESP32-S3 por USB y, en la barra inferior de PlatformIO, dale **Upload** (→) y luego **Monitor** (🔌) para ver los mensajes.

> `secrets.h` está en `.gitignore`, así la clave WiFi no se sube al repo.
