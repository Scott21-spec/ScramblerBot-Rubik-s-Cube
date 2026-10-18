import random

def generar_scramble_aleatorio():
    movimientos_generados = []
    ultimo_movimiento = ""
    movimientos = ["U", "U'", "U2", "D", "D'", "D2", "R", "R'", "R2", "L", "L'", "L2", "F", "F'", "F2", "B", "B'", "B2"]
    while len(movimientos_generados)<20:
        nuevo_movimiento = random.choice(movimientos)
        if nuevo_movimiento[0] != ultimo_movimiento:
            movimientos_generados.append(nuevo_movimiento)
            ultimo_movimiento = nuevo_movimiento[0]
    return " ".join(movimientos_generados)