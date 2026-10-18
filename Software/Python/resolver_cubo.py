# --- PARCHE DE COMPATIBILIDAD PARA PYTHON 3.10+ ---
import collections
import collections.abc
# Mapeamos automáticamente todas las clases abstractas de collections.abc hacia collections
for abc_class in [
    'Iterable', 'Mapping', 'MutableMapping', 'Sequence',
    'MutableSequence', 'Callable', 'Container', 'Set', 'MutableSet'
]:
    if not hasattr(collections, abc_class):
        setattr(collections, abc_class, getattr(collections.abc, abc_class))
# --------------------------------------------------

from rubik_solver import utils

TRADUCCION_RUBIK = {
    'Blanco': 'w',   'U': 'w',
    'Rojo': 'r',     'R': 'r',
    'Azul': 'b',     'B': 'b',
    'Verde': 'g',    'F': 'g',
    'Amarillo': 'y', 'D': 'y',
    'Naranja': 'o',  'L': 'o'
}

# Colores fijos que espera rubik_solver para cada posición espacial
COLOR_ESPERADO_SOLVER = {
    'U': 'y',  # Arriba
    'L': 'b',  # Izquierda
    'F': 'r',  # Frente
    'R': 'g',  # Derecha
    'B': 'o',  # Atrás
    'D': 'w'   # Abajo
}

def transformar_orientacion(cara, orientacion):
    if orientacion == "inversa":
        # de abajo a arriba, de derecha a izquierda
        return cara[::-1]
    # de arriba a abajo, de izquierda a derecha
    return cara

def resolver_kociemba(caras_colores, orden_captura):
    # Reemplazar cada color por su letra correspondiente
    caras_mapeadas = {}
    for i, cara in enumerate(caras_colores):
        posicion = orden_captura[i]
        #cara_convertida = [mapa_colores[color] for color in cara]
        cara_convertida = [TRADUCCION_RUBIK.get(color, color) for color in cara]
        caras_mapeadas[posicion] = cara_convertida

    # Orientacion de cada cara
    orientaciones = {
        'U': 'inversa',
        'B': 'normal',
        'D': 'inversa',
        'F': 'inversa',
        'R': 'inversa',
        'L': 'normal'
    }

    # Aplicar la reorientación a cada cara
    caras_orientadas = {}
    for posicion_cara, lista_stickers in caras_mapeadas.items():
        tipo_orientacion = orientaciones[posicion_cara]
        caras_orientadas[posicion_cara] = transformar_orientacion(lista_stickers, tipo_orientacion)

    # El centro (índice 4) de cada cara determina qué color real está en esa posición espacial
    mapa_dinamico = {}
    for posicion in ['U', 'L', 'F', 'R', 'B', 'D']:
        color_centro_real = caras_orientadas[posicion][4]  # Color real en el centro
        color_esperado = COLOR_ESPERADO_SOLVER[posicion]  # Código que exige el solver
        mapa_dinamico[color_centro_real] = color_esperado

    # Concatenar en el orden exacto que requiere Kociemba
    orden_solver = ['U', 'L', 'F', 'R', 'B', 'D']
    cadena_kociemba = ""

    for posicion_cara in orden_solver:
        cadena_kociemba += "".join(caras_orientadas[posicion_cara])
    print("Cadena original: ", cadena_kociemba)
    cadena_solver = "".join([mapa_dinamico[c] for c in cadena_kociemba])
    print("Cadena traducida: ", cadena_solver)
    try:
        movimientos = utils.solve(cadena_solver, 'Kociemba')
        solucion = " ".join(str(m) for m in movimientos)
        return solucion
    except Exception as e:
        print(f"Error al calcular Kociemba: {e}")
        return None
