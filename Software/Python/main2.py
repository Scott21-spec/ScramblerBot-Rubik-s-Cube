from fastapi import FastAPI, WebSocket, WebSocketDisconnect
import uvicorn
import json
import asyncio
from reconocimiento_colores import reconocer_cara
from subir_archivo import abrir_selector_archivos
from generar_movimientos import generar_scramble_aleatorio
from resolver_cubo import resolver_kociemba
import estado_cubo
app = FastAPI()

# --- WEBSOCKET ENDPOINT ---
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    print("✅ Interfaz conectada")

    try:
        while True:
            data = await websocket.receive_text()
            mensaje = json.loads(data)
            comando = mensaje.get("comando")

            # ---------------- MODO RESOLVER ----------------
            if comando == "iniciar_resolver":
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Iniciando escaneo del cubo..."}))
                caras_reconocidas = 0
                intentos = 0
                caras = []
                orden = ['U', 'B', 'D', 'F', 'R', 'L']
                while caras_reconocidas < 6:
                    if comando == "detener_proceso":
                        await websocket.send_json({"tipo": "terminal",
                                                   "mensaje": "Proceso detenido por el usuario."})
                        # --> MQTT para que el ESP32 detenga el proceso
                        break
                    if intentos == 3:
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal",
                                        "mensaje": f"Error. No fue posible reconocer el cubo."}))
                        # --> MQTT para que el ESP32 detenga el proceso
                        break

                    # --> Aquí iría el MQTT con el ESP32 para sacar la foto
                    # --> Guardar imagen en capturas/
                    await asyncio.sleep(2.0)
                    # asyncio.to_thread ejecuta el reconocimiento sin congelar FastAPI
                    cara_detectada = await asyncio.to_thread(reconocer_cara,
                                                             f"capturas/captura{caras_reconocidas + 1}.png")
                    caras.append(cara_detectada)
                    if "Desconocido" in caras[caras_reconocidas]:
                        intentos += 1
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal",
                                        "mensaje": f"Error al reconocer cara {caras_reconocidas+1}. Intento nro. {intentos}"}))
                        continue
                    else:
                        caras_reconocidas += 1
                        intentos = 0
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal",
                                        "mensaje": f"Lista la cara {caras_reconocidas}."}))
                        # MQTT de que se reconocio bien y que cambiemos a la siguiente posicion

                    await asyncio.sleep(0.01)

                if caras_reconocidas == 6:
                    # Obtenemos estado actual
                    caras[5][4] = 'Blanco'
                    estado_actual = estado_cubo.mapear_caras_capturadas(caras, orden)

                    # Enviamos el estado inicial a la interfaz
                    await websocket.send_text(json.dumps({
                        "tipo": "actualizar_estado_cubo",
                        "estado": estado_actual,
                        "animar": False
                    }))

                    solucion = await asyncio.to_thread(resolver_kociemba, caras, orden)
                    if solucion:
                        await websocket.send_text(json.dumps({"tipo": "movimientos_resolver", "datos": solucion}))
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal",
                                        "mensaje": f"Solución calculada con exito. Ejecutando en la ESP32..."}))

                        # Transmisión movimiento a movimiento
                        movimientos = solucion.split()
                        for idx, mov in enumerate(movimientos):
                            if comando == "detener_proceso":
                                await websocket.send_json(
                                    {"tipo": "terminal",
                                     "mensaje": "Proceso detenido por el usuario."})
                                # --> MQTT para que el ESP32 detenga el proceso
                                break
                            # --> MQTT con el ESP32 para que realice los movimientos
                            # Esperamos confirmación del movimiento (simulación de tiempo de giro)
                            await asyncio.sleep(1.0)

                            # Actualizamos el estado interno
                            estado_actual = estado_cubo.actualizar_estado(estado_actual, mov)

                            # Enviamos actualización con animación activa
                            await websocket.send_text(json.dumps({
                                "tipo": "actualizar_estado_cubo",
                                "estado": estado_actual,
                                "movimiento": mov,
                                "progreso": idx + 1,
                                "total": len(movimientos),
                                "modo": "resolver",
                                "animar": True
                            }))

                            await asyncio.sleep(0.01)

                        # --> Hacer que isEnded sea true para permitir ver el resumen
                        await websocket.send_text(json.dumps(
                            {"tipo": "terminal", "mensaje": "Proceso de resolución finalizado con éxito."}))
                        # Enviar la señal para actualizar variables en JS
                        await websocket.send_json({
                            "tipo": "proceso_completado"
                        })

                        # --> Guardamos en el csv de resolucion el resumen del proceso
                    else:
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal",
                                        "mensaje": f"ERROR. No hay solucion"}))

            elif comando == "ver_historial_resolver":
                # --> Enviar el csv de resolucion con todos los intentos anteriores
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Abriendo historial de resoluciones..."}))

            elif comando == "recibir_resumen_resolver":
                # --> Leemos la ultima linea del csv de resolucion de base de datos
                # --> Enviamos resumen
                # resumen = "Tiempo: 45s | Movimientos: 22 | Errores: 0"
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Abriendo resumen..."}))

            # ---------------- MODO MEZCLAR ----------------
            elif comando == "abrir_explorador":
                contenido = abrir_selector_archivos()
                if contenido:
                    await websocket.send_text(json.dumps({"tipo": "secuencia_leida", "datos": contenido}))
                    # Enviamos cubo resuelto inicial para visualización en modo Mezclar
                    await websocket.send_text(json.dumps({
                        "tipo": "preparar_mezcla",
                        "estado": estado_cubo.obtener_estado_resuelto()
                    }))
                    await websocket.send_text(
                        json.dumps({"tipo": "terminal", "mensaje": "Archivo cargado correctamente."}))
                else:
                    await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Selección cancelada."}))

            elif comando == "generar_movimientos":
                scramble = generar_scramble_aleatorio()
                await websocket.send_text(json.dumps({"tipo": "secuencia_leida", "datos": scramble}))
                # Enviamos cubo resuelto inicial para visualización en modo Mezclar
                await websocket.send_text(json.dumps({
                    "tipo": "preparar_mezcla",
                    "estado": estado_cubo.obtener_estado_resuelto()
                }))
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Secuencia WCA generada."}))

            elif comando == "iniciar_mezclar":
                secuencia = mensaje.get("secuencia", "")
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Enviando al ESP32..."}))
                estado_actual = estado_cubo.obtener_estado_resuelto()
                movimientos = secuencia.split()

                for idx, mov in enumerate(movimientos):
                    # --> MQTT en el ESP32 y realiza los movimientos de mezcla
                    if comando == "detener_proceso":
                        await websocket.send_json(
                            {"tipo": "terminal",
                             "mensaje": "Proceso detenido por el usuario."})
                        # --> MQTT para que el ESP32 detenga el proceso
                        break
                    estado_actual = estado_cubo.actualizar_estado(estado_actual, mov)

                    await websocket.send_text(json.dumps({
                        "tipo": "actualizar_estado_cubo",
                        "estado": estado_actual,
                        "movimiento": mov,
                        "progreso": idx + 1,
                        "total": len(movimientos),
                        "modo": "mezclar",
                        "animar": True
                    }))
                    await asyncio.sleep(0.01)
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Mezcla completada."}))
                # Enviar la señal para actualizar variables en JS
                await websocket.send_json({
                    "tipo": "proceso_completado"
                })

            elif comando == "ver_historial_mezclar":
                # --> Enviar el csv de mezcla con todos los intentos anteriores
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Abriendo historial de mezclas..."}))

            elif comando == "recibir_resumen_mezclar":
                # --> Leemos la ultima linea del csv de mezcla de base de datos
                # --> Enviamos el resumen
                #resumen = "Tiempo: 12s | Movimientos: 20 | Errores: 0"
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Abriendo resumen..."}))

            # ---------------- MODO AJUSTES ----------------
            elif comando == "mover_servo":
                servo = mensaje.get("servo")
                angulo = mensaje.get("angulo")
                # Aquí enviaremos el payload por MQTT al ESP32
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"{servo} a {angulo}°"}))

            #elif comando == "capturar_cubo":

            # ---------------- COMANDOS GLOBALES ----------------
            elif comando == "detener_proceso":
                # Enviamos comando de parar al ESP32 por MQTT
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "STOP. Servos a posición natural."}))

    except WebSocketDisconnect:
        print("❌ Interfaz desconectada")


if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=8000)