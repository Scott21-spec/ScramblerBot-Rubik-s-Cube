from reconocimiento_colores import reconocer_cara, pos2colores, convertir_cadena

if __name__ == "__main__":
    caras = []
    for i in range(1,7):
        posiciones = reconocer_cara(f"capturas/captura{i}.png")
        resultado = pos2colores(posiciones)
        caras.append(posiciones)
        print("Resultado:", resultado)

    print(convertir_cadena(caras))