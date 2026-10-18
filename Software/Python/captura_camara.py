import cv2
import numpy as np
import serial
import time

PUERTO_COM = 'COM8'  # Cambia por tu puerto COM exacto
BAUD_RATE = 921600

print("Conectando con Arduino Mega...")
ser = serial.Serial(PUERTO_COM, BAUD_RATE, timeout=2)

# 1. Esperar 3 segundos a que el Arduino termine su Reset inicial
time.sleep(3)

# 2. Limpiar el texto de inicio ("ACK CMD...") acumulado en el búfer
ser.reset_input_buffer()

# 3. Enviar comando 0x10 (DISPARAR / CAPTURAR FOTO)
print("Enviando comando de captura (0x10)...")
ser.write(bytes([0x10]))

buffer = bytearray()
leyendo_foto = False

while True:
    byte = ser.read(1)
    if not byte:
        continue

    buffer.append(byte[0])

    # Marcador de INICIO de foto JPEG (0xFF 0xD8)
    if len(buffer) >= 2 and buffer[-2] == 0xFF and buffer[-1] == 0xD8:
        buffer = bytearray([0xFF, 0xD8])
        leyendo_foto = True

    # Marcador de FIN de foto JPEG (0xFF 0xD9)
    elif leyendo_foto and len(buffer) >= 2 and buffer[-2] == 0xFF and buffer[-1] == 0xD9:
        leyendo_foto = False

        # Decode de la imagen binaria a formato OpenCV
        img_np = np.frombuffer(buffer, dtype=np.uint8)
        img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)

        if img is not None:
            cv2.imshow("Arducam OV5642 - Capitura", img)
            print("¡Imagen capturada y mostrada con éxito!")
            cv2.waitKey(0)  # Mantiene la foto abierta hasta presionar una tecla
            break

        buffer = bytearray()

ser.close()
cv2.destroyAllWindows()