# Importamos la libreria para reconocer colores HSV
# H: tono
# S: saturacion
# V: brillo
import cv2
import numpy as np

# Rangos de colores en HSV
COLOR_RANGES = {
    'Blanco': {'lower': np.array([0, 0, 160]),   'upper': np.array([180, 55, 255])},
    'Naranja': {'lower': np.array([9, 100, 100]),  'upper': np.array([20, 255, 255])},
    'Amarillo': {'lower': np.array([21, 100, 100]), 'upper': np.array([35, 255, 255])},
    'Verde': {'lower': np.array([36, 70, 70]),   'upper': np.array([85, 255, 255])},
    'Azul': {'lower': np.array([86, 70, 70]),   'upper': np.array([135, 255, 255])},
}

# Retorna si se encuentra dentro de los dos margenes del rojo
def detectar_rojo(hsv_pixel):
    # Rojo abarca el inicio (0-8) y el final (165-180) de la rueda HSV
    r1 = (hsv_pixel[0] <= 8) and (hsv_pixel[1] >= 90) and (hsv_pixel[2] >= 90)
    r2 = (hsv_pixel[0] >= 165) and (hsv_pixel[1] >= 90) and (hsv_pixel[2] >= 90)
    return r1 or r2

# Funcion para reconocer los 9 colores de una cara
def reconocer_cara(imagen_path):
    img = cv2.imread(imagen_path)
    if img is None:
        return None

    num = imagen_path.split(".")[0][-1]
    h, w, _ = img.shape
    hsv_img = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

    cols = [int(w * 0.166), int(w * 0.500), int(w * 0.833)]
    rows = [int(h * 0.166), int(h * 0.500), int(h * 0.833)]

    # Para los puntos al rededor del centro
    dx = int(w * 0.04)
    dy = int(h * 0.04)

    colores_detectados = []
    img_debug = img.copy()

    for y in rows:
        for x in cols:
            puntos = [
                (x, y),  # Centro
                (x - dx, y - dy),  # Arriba-Izquierda
                (x + dx, y - dy),  # Arriba-Derecha
                (x - dx, y + dy),  # Abajo-Izquierda
                (x + dx, y + dy)  # Abajo-Derecha
            ]

            muestras_hsv = []
            for px, py in puntos:
                roi = hsv_img[max(0, py - 1):min(h, py + 2), max(0, px - 1):min(w, px + 2)]
                mean_val = cv2.mean(roi)[:3]
                muestras_hsv.append(mean_val)
                cv2.circle(img_debug, (px, py), 3, (0, 0, 0), -1)

            # Mediana para tomar el valor mas cercano al real
            mediana_hsv = np.median(muestras_hsv, axis=0)

            sticker_color = "Desconocido"

            # Evaluacion de Blanco (saturación muy baja)
            if mediana_hsv[1] <= 55 and mediana_hsv[2] >= 150:
                sticker_color = 'Blanco'
            # Evaluacion de Rojo
            elif detectar_rojo(mediana_hsv):
                sticker_color = 'Rojo'
            # Evaluacion del resto de colores
            else:
                for color_name, rango in COLOR_RANGES.items():
                    if (rango['lower'][0] <= mediana_hsv[0] <= rango['upper'][0] and
                            rango['lower'][1] <= mediana_hsv[1] <= rango['upper'][1] and
                            rango['lower'][2] <= mediana_hsv[2] <= rango['upper'][2]):
                        sticker_color = color_name
                        break

            colores_detectados.append(sticker_color)

    # Guardamos la captura con los roi
    cv2.imwrite(f"cap_recon/captura{num}.png", img_debug)
    return colores_detectados