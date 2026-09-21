# Importamos la libreria para reconocer colores HSV
# H: tono
# S: saturacion
# V: brillo
import cv2
import numpy as np

# Rangos de colores en HSV
COLOR_RANGES = {
    'U': {'lower': np.array([0, 0, 180]), 'upper': np.array([180, 40, 255])},     # Blanco
    'D': {'lower': np.array([15, 120, 120]), 'upper': np.array([35, 255, 255])},  # Amarillo
    'F': {'lower': np.array([36, 100, 100]), 'upper': np.array([85, 255, 255])},  # Verde
    'B': {'lower': np.array([90, 100, 100]), 'upper': np.array([130, 255, 255])}, # Azul
    'L': {'lower': np.array([10, 120, 120]), 'upper': np.array([22, 255, 255])},  # Naranja
}

# Pasar de posicion a color
COLORES = {
    'U': 'Blanco',
    'D': 'Amarillo',
    'F': 'Verde',
    'B': 'Azul',
    'L': 'Naranja',
    'R': 'Rojo'
}

# Retorna si se encuentra dentro de los dos margenes del rojo
def detectar_rojo(hsv_pixel):
    r1 = (hsv_pixel[0] <= 9) and (hsv_pixel[1] >= 100) and (hsv_pixel[2] >= 100)
    r2 = (hsv_pixel[0] >= 170) and (hsv_pixel[1] >= 100) and (hsv_pixel[2] >= 100)
    return r1 or r2

# Funcion para reconocer los 9 colores de una cara
def reconocer_cara(imagen_path):
    img = cv2.imread(imagen_path)
    num = imagen_path.split(".")[0][-1]

    # Si la imagen no existe
    if img is None:
        print("Error: No se pudo cargar la imagen.")
        return

    # Extraemos el alto y ancho de la imagen
    h, w, _ = img.shape
    hsv_img = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)  # Cambiamos de BGR a HSV

    # Posiciones relativas (16.6%, 50%, 83.3%)
    cols = [int(w * 0.166), int(w * 0.500), int(w * 0.833)]
    rows = [int(h * 0.166), int(h * 0.500), int(h * 0.833)]

    # Vector donde se encuentran los 9 colores
    colores_detectados = []

    # Copiamos la imagen para dibujar los puntos de muestra
    img_debug = img.copy()

    for y in rows:
        for x in cols:
            # Tomamamos una muestra de 9x9 píxeles en el centro
            roi = hsv_img[y - 4:y + 5, x - 4:x + 5]
            promedio_hsv = cv2.mean(roi)[:3]

            sticker_color = "Desconocido"

            # Evaluamos si es rojo
            if detectar_rojo(promedio_hsv):
                sticker_color = 'R'
            else:
                # Evaluamos el resto de colores
                for color_name, rango in COLOR_RANGES.items():
                    if (rango['lower'][0] <= promedio_hsv[0] <= rango['upper'][0] and
                        rango['lower'][1] <= promedio_hsv[1] <= rango['upper'][1] and
                        rango['lower'][2] <= promedio_hsv[2] <= rango['upper'][2]):
                        sticker_color = color_name
                        break

            colores_detectados.append(sticker_color)

            # Dibujamos un círculo azul en el punto exacto de la muestra
            cv2.circle(img_debug, (x, y), 5, (0, 0, 0), -1)

    # Guardamos la imagen con los puntos marcados
    cv2.imwrite(f"cap_recon/captura{num}.png", img_debug)

    return colores_detectados

def pos2colores(posiciones):
    colors = []
    for posicion in posiciones:
        for color in COLORES:
            if posicion == color:
                colors.append(COLORES[color])
                break

    return colors

def convertir_cadena(caras):
    cadena = ""
    for cara in caras:
        for posicion in cara:
            cadena += posicion

    return cadena