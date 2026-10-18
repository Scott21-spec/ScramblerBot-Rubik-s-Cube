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

# Variables globales de control de tareas concurrentes
tarea_proceso_actual = None
cancel_event = None


async def esperar_o_cancelar(segundos: float, cancel_evt: asyncio.Event) -> bool:
    """
    Pausa el proceso durante 'segundos', pero si el usuario presiona DETENER,
    se interrumpe inmediatamente volviendo True.
    """
    try:
        await asyncio.wait_for(cancel_evt.wait(), timeout=segundos)
        return True  # Fue cancelado
    except asyncio.TimeoutError:
        return False  # Pasó el tiempo normal sin cancelación


async def ejecutar_proceso_resolver(websocket: WebSocket, cancel_evt: asyncio.Event):
    try:
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Iniciando escaneo del cubo..."}))
        caras_reconocidas = 0
        intentos = 0
        caras = []
        orden = ['U', 'B', 'D', 'F', 'R', 'L']

        # Reconocimiento de las 6 caras del cubo
        while caras_reconocidas < 6:
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Escaneo cancelado por el usuario."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

            if intentos == 3:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Error. No fue posible reconocer el cubo."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

            # --> Aquí iría el MQTT con el ESP32 para sacar la foto
            # --> Guardar imagen en capturas/
            if await esperar_o_cancelar(2.0, cancel_evt):
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Escaneo detenido por el usuario."}))
                return

            # Reconocimiento de imagen
            cara_detectada = await asyncio.to_thread(reconocer_cara, f"capturas/captura{caras_reconocidas + 1}.png")

            if "Desconocido" in cara_detectada:
                intentos += 1
                await websocket.send_text(json.dumps(
                    {"tipo": "terminal", "mensaje": f"Error en cara {caras_reconocidas + 1}. Intento {intentos}"}))
                continue

            caras.append(cara_detectada)
            caras_reconocidas += 1
            intentos = 0
            await websocket.send_text(
                json.dumps({"tipo": "terminal", "mensaje": f"Lista la cara {caras_reconocidas}."}))
            # --> MQTT de que se reconocio bien y que cambiemos a la siguiente posicion

        #  Calculo de la solucion
        if cancel_evt.is_set():
            return

        estado_actual = estado_cubo.mapear_caras_capturadas(caras, orden)

        # Renderizar estado inicial capturado en 3D
        await websocket.send_text(json.dumps({
            "tipo": "actualizar_estado_cubo",
            "estado": estado_actual,
            "animar": False
        }))

        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Calculando solución con Kociemba..."}))

        solucion = await asyncio.to_thread(resolver_kociemba, caras, orden)

        if not solucion:
            await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "ERROR. No hay solución disponible."}))
            return

        if cancel_evt.is_set():
            await websocket.send_text(
                json.dumps({"tipo": "terminal", "mensaje": "Proceso detenido antes de ejecutar movimientos."}))
            return

        await websocket.send_text(json.dumps({"tipo": "movimientos_resolver", "datos": solucion}))
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Solución calculada con éxito. Ejecutando en ESP32..."}))

        # Transmisión movimiento a movimiento
        movimientos = solucion.split()
        for idx, mov in enumerate(movimientos):
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Ejecución de movimientos detenida por el usuario."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

            # --> MQTT con el ESP32 para que realice los movimientos
            # Esperamos confirmación del movimiento (simulación de tiempo de giro)
            # Hay que reemplazar esta funcion por otra que espere a que le llegue la confirmacion del movimiento
            if await esperar_o_cancelar(1.0, cancel_evt):
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Movimientos detenidos por el usuario."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

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

        # Finalización exitosa
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Proceso de resolución finalizado con éxito."}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))
        # --> Recibir resumen MQTT y guardamos en el csv de resolucion el resumen del proceso

    except Exception as e:
        print(f"Error en proceso resolver: {e}")
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"Error en proceso: {e}"}))


async def ejecutar_proceso_mezclar(websocket: WebSocket, secuencia: str, cancel_evt: asyncio.Event):
    try:
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Enviando secuencia de mezcla al ESP32..."}))
        estado_actual = estado_cubo.obtener_estado_resuelto()
        movimientos = secuencia.split()

        for idx, mov in enumerate(movimientos):
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Mezcla detenida por el usuario."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

            # --> MQTT en el ESP32 y realiza los movimientos de mezcla
            # Debe cambiarse esta funcion por otra que espere a que le llegue la confirmacion del movimiento
            if await esperar_o_cancelar(1.0, cancel_evt):
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Mezcla interrumpida por el usuario."}))
                # --> MQTT para que el ESP32 detenga el proceso
                return

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

        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Mezcla completada."}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))

    except Exception as e:
        print(f"Error en proceso mezclar: {e}")

async def ejecutar_proceso_capturar(websocket: WebSocket, cancel_evt: asyncio.Event):
    try:
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Iniciando escaneo del cubo (Ajustes)..."}))
        caras_reconocidas = 0
        intentos = 0
        caras = []
        orden = ['U', 'B', 'D', 'F', 'R', 'L']

        # Escaneo de las 6 caras
        while caras_reconocidas < 6:
            if cancel_evt.is_set():
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Escaneo cancelado por el usuario."}))
                return

            if intentos == 3:
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Error. No fue posible reconocer el cubo."}))
                return

            # Simulación de espera por foto (cancelable al instante)
            if await esperar_o_cancelar(2.0, cancel_evt):
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Escaneo detenido por el usuario."}))
                return

            cara_detectada = await asyncio.to_thread(reconocer_cara, f"capturas/captura{caras_reconocidas + 1}.png")

            if "Desconocido" in cara_detectada:
                intentos += 1
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"Error en cara {caras_reconocidas+1}. Intento {intentos}"}))
                continue

            caras.append(cara_detectada)
            caras_reconocidas += 1
            intentos = 0
            await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"Lista la cara {caras_reconocidas}."}))

        if cancel_evt.is_set():
            return

        estado_actual = estado_cubo.mapear_caras_capturadas(caras, orden)

        # Enviar estado final a la interfaz para pintar el cubo 3D
        await websocket.send_text(json.dumps({
            "tipo": "actualizar_estado_cubo",
            "estado": estado_actual,
            "animar": False
        }))

        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Escaneo del cubo completado con éxito."}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))

    except Exception as e:
        print(f"Error en proceso capturar: {e}")
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"Error en captura: {e}"}))


# --- WEBSOCKET ENDPOINT ---
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global tarea_proceso_actual, cancel_event
    await websocket.accept()
    print("✅ Interfaz conectada")

    try:
        while True:
            data = await websocket.receive_text()
            mensaje = json.loads(data)
            comando = mensaje.get("comando")

            # ---------------- MODO RESOLVER ----------------
            if comando == "iniciar_resolver":
                if tarea_proceso_actual and not tarea_proceso_actual.done():
                    await websocket.send_text(
                        json.dumps({"tipo": "terminal", "mensaje": "Ya hay un proceso en ejecución."}))
                    continue

                cancel_event = asyncio.Event()
                tarea_proceso_actual = asyncio.create_task(ejecutar_proceso_resolver(websocket, cancel_event))

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
                await websocket.send_text(json.dumps({
                    "tipo": "preparar_mezcla",
                    "estado": estado_cubo.obtener_estado_resuelto()
                }))
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Secuencia WCA generada."}))

            elif comando == "iniciar_mezclar":
                if tarea_proceso_actual and not tarea_proceso_actual.done():
                    await websocket.send_text(
                        json.dumps({"tipo": "terminal", "mensaje": "Ya hay un proceso en ejecución."}))
                    continue

                secuencia = mensaje.get("secuencia", "")
                cancel_event = asyncio.Event()
                tarea_proceso_actual = asyncio.create_task(ejecutar_proceso_mezclar(websocket, secuencia, cancel_event))

            elif comando == "ver_historial_mezclar":
                # --> Enviar el csv de mezcla con todos los intentos anteriores
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Abriendo historial de mezclas..."}))

            elif comando == "recibir_resumen_mezclar":
                # --> Leemos la ultima linea del csv de mezcla de base de datos
                # --> Enviamos el resumen
                # resumen = "Tiempo: 12s | Movimientos: 20 | Errores: 0"
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Abriendo resumen..."}))

            # ---------------- MODO AJUSTES ----------------
            elif comando == "mover_servo":
                servo = mensaje.get("servo")
                angulo = mensaje.get("angulo")
                # Aquí enviaremos el payload por MQTT al ESP32
                await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": f"{servo} a {angulo}°"}))


            elif comando == "capturar_cubo":
                if tarea_proceso_actual and not tarea_proceso_actual.done():
                    await websocket.send_text(
                        json.dumps({"tipo": "terminal", "mensaje": "Ya hay un proceso en ejecución."}))
                    continue

                cancel_event = asyncio.Event()
                tarea_proceso_actual = asyncio.create_task(ejecutar_proceso_capturar(websocket, cancel_event))


            # ---------------- COMANDOS GLOBALES ----------------
            elif comando == "detener_proceso":
                if cancel_event:
                    cancel_event.set()  # Cancela instantáneamente el proceso en segundo plano
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "STOP. Servos a posición natural."}))

    except WebSocketDisconnect:
        print("❌ Interfaz desconectada")
        if cancel_event:
            cancel_event.set()


if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=8000)