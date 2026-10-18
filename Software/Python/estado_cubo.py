def obtener_estado_resuelto():
    """Retorna la estructura de un cubo completamente resuelto."""
    return {
        'U': ['Blanco'] * 9,
        'L': ['Naranja'] * 9,
        'F': ['Verde'] * 9,
        'R': ['Rojo'] * 9,
        'B': ['Azul'] * 9,
        'D': ['Amarillo'] * 9
    }


def mapear_caras_capturadas(caras_colores, orden_captura=['U', 'B', 'D', 'F', 'R', 'L']):
    """
    Transforma la lista de capturas al diccionario de caras del cubo.
    """
    estado = {}
    orientaciones = {
        'U': 'inversa',
        'B': 'normal',
        'D': 'inversa',
        'F': 'inversa',
        'R': 'inversa',
        'L': 'normal'
    }

    for i, cara in enumerate(caras_colores):
        pos = orden_captura[i]
        tipo_ori = orientaciones.get(pos, 'normal')
        if tipo_ori == 'inversa':
            estado[pos] = list(cara[::-1])
        else:
            estado[pos] = list(cara)

    return estado


def _rotar_matriz_cw(lista_9):
    """Rota una grilla de 3x3 (9 elementos) 90 grados en sentido horario."""
    return [
        lista_9[6], lista_9[3], lista_9[0],
        lista_9[7], lista_9[4], lista_9[1],
        lista_9[8], lista_9[5], lista_9[2]
    ]


def _aplicar_giro_horario(estado, cara):
    """Aplica la rotación horaria de una cara y desplaza los bordes adyacentes."""
    estado[cara] = _rotar_matriz_cw(estado[cara])

    if cara == 'U':
        tmp = estado['F'][0:3]
        estado['F'][0:3] = estado['R'][0:3]
        estado['R'][0:3] = estado['B'][0:3]
        estado['B'][0:3] = estado['L'][0:3]
        estado['L'][0:3] = tmp

    elif cara == 'D':
        tmp = estado['F'][6:9]
        estado['F'][6:9] = estado['L'][6:9]
        estado['L'][6:9] = estado['B'][6:9]
        estado['B'][6:9] = estado['R'][6:9]
        estado['R'][6:9] = tmp

    elif cara == 'F':
        u_tmp = list(estado['U'][6:9])
        estado['U'][6] = estado['L'][8]
        estado['U'][7] = estado['L'][5]
        estado['U'][8] = estado['L'][2]

        estado['L'][8] = estado['D'][2]
        estado['L'][5] = estado['D'][1]
        estado['L'][2] = estado['D'][0]

        estado['D'][2] = estado['R'][0]
        estado['D'][1] = estado['R'][3]
        estado['D'][0] = estado['R'][6]

        estado['R'][0] = u_tmp[0]
        estado['R'][3] = u_tmp[1]
        estado['R'][6] = u_tmp[2]

    elif cara == 'B':
        u_tmp = [estado['U'][2], estado['U'][1], estado['U'][0]]

        estado['U'][2] = estado['R'][8]
        estado['U'][1] = estado['R'][5]
        estado['U'][0] = estado['R'][2]

        estado['R'][8] = estado['D'][6]
        estado['R'][5] = estado['D'][7]
        estado['R'][2] = estado['D'][8]

        estado['D'][6] = estado['L'][0]
        estado['D'][7] = estado['L'][3]
        estado['D'][8] = estado['L'][6]

        estado['L'][0] = u_tmp[0]
        estado['L'][3] = u_tmp[1]
        estado['L'][6] = u_tmp[2]

    elif cara == 'R':
        u_tmp = [estado['U'][2], estado['U'][5], estado['U'][8]]

        estado['U'][2] = estado['F'][2]
        estado['U'][5] = estado['F'][5]
        estado['U'][8] = estado['F'][8]

        estado['F'][2] = estado['D'][2]
        estado['F'][5] = estado['D'][5]
        estado['F'][8] = estado['D'][8]

        estado['F'][8] = estado['D'][8]

        estado['D'][2] = estado['B'][6]
        estado['D'][5] = estado['B'][3]
        estado['D'][8] = estado['B'][0]

        estado['B'][6] = u_tmp[0]
        estado['B'][3] = u_tmp[1]
        estado['B'][0] = u_tmp[2]

    elif cara == 'L':
        u_tmp = [estado['U'][0], estado['U'][3], estado['U'][6]]

        estado['U'][0] = estado['B'][8]
        estado['U'][3] = estado['B'][5]
        estado['U'][6] = estado['B'][2]

        estado['B'][8] = estado['D'][0]
        estado['B'][5] = estado['D'][3]
        estado['B'][2] = estado['D'][6]

        estado['D'][0] = estado['F'][0]
        estado['D'][3] = estado['F'][3]
        estado['D'][6] = estado['F'][6]

        estado['F'][0] = u_tmp[0]
        estado['F'][3] = u_tmp[1]
        estado['F'][6] = u_tmp[2]


def actualizar_estado(estado_actual, movimiento):
    if not movimiento:
        return estado_actual

    cara = movimiento[0].upper()
    mod = movimiento[1:] if len(movimiento) > 1 else ""

    veces = 1
    if mod == "2":
        veces = 2
    elif mod == "'":
        veces = 3  # 3 giros horarios equivalen a 1 antihorario

    for _ in range(veces):
        _aplicar_giro_horario(estado_actual, cara)

    return estado_actual