

import json
import socket
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PUERTO = 5000


class Handler(BaseHTTPRequestHandler):
    timeout = 5  # si una conexión se queda colgada, se suelta a los 5 s

    def do_POST(self):
        if self.path != "/alerta":
            self.send_response(404)
            self.end_headers()
            return
        largo = int(self.headers.get("Content-Length", 0))
        datos = self.rfile.read(largo).decode(errors="ignore")
        try:
            datos = json.loads(datos)
        except json.JSONDecodeError:
            pass
        hora = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        evento = datos.get("evento") if isinstance(datos, dict) else None
        if evento == "CONECTADA":
            print(f"[{hora}] ESP32 conectada al WiFi desde {self.client_address[0]}")
        else:
            print("\a")  # pitido de consola
            print("=" * 50)
            print(f"  ¡ALERTA RECIBIDA!  {hora}")
            print(f"  Desde: {self.client_address[0]}  Datos: {datos}")
            print("=" * 50)
        self.send_response(200)
        self.send_header("Content-Length", "2")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(b"OK")

    def log_message(self, *args):
        pass  # silencia el log por defecto


if __name__ == "__main__":
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except OSError:
        ip = "desconocida"
    finally:
        s.close()
    print(f"Escuchando en http://{ip}:{PUERTO}/alerta")
    print("Pon esa IP en PC_URL del sketch del ESP32.")
    ThreadingHTTPServer(("0.0.0.0", PUERTO), Handler).serve_forever()  # atiende varios mensajes a la vez
