from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from contextlib import asynccontextmanager
import uvicorn
import json
import asyncio
import aiomqtt
import sys

from reconocimiento_colores import reconocer_cara
from subir_archivo import abrir_selector_archivos
from generar_movimientos import generar_scramble_aleatorio
from resolver_cubo import resolver_kociemba
from estado_cubo import *
from configuracion_servo import *

### --> CONFIGURACIÓN Y VARIABLES MQTT
BROKER_IP = "127.0.0.1"
BROKER_PORT = 1883
BOT_ID = "bot1"

TOPICO_COMANDOS = f"scramblerbot/{BOT_ID}/comandos"
TOPICO_ESTADO = f"scramblerbot/{BOT_ID}/estado"
TOPICO_IMAGEN = f"scramblerbot/{BOT_ID}/imagen"

mqtt_client = None
evento_ack_mqtt = asyncio.Event()
ultimo_mensaje_mqtt = {}

# Variables globales de control de tareas concurrentes
tarea_proceso_actual = None
cancel_event = None
estado_ajustes = obtener_estado_resuelto()


######## ELIMINAR LUEGO CUANDO TENGA EL CUBO MAGNETICO
async def esperar_o_cancelar(segundos: float, cancel_evt: asyncio.Event) -> bool:
    # Pausa de 5 segundos entre movimientos
    try:
        await asyncio.wait_for(cancel_evt.wait(), timeout=segundos)
        return True   # Se activó la cancelación
    except asyncio.TimeoutError:
        return False  # Se cumplió el tiempo de pausa normal



### --> CICLO DE VIDA DE FASTAPI (MQTT RUNNER)
async def escuchar_mqtt(client: aiomqtt.Client):
    global ultimo_mensaje_mqtt, evento_ack_mqtt
    try:
        await client.subscribe(TOPICO_ESTADO)
        await client.subscribe(TOPICO_IMAGEN)

        async for message in client.messages:
            if message.topic.matches(TOPICO_ESTADO):
                payload = json.loads(message.payload.decode())
                ultimo_mensaje_mqtt = payload
                evento_ack_mqtt.set()  # Despierta a Python: la ESP32 respondió

            elif message.topic.matches(TOPICO_IMAGEN):
                # Guardar imagen JPEG recibida en la carpeta capturas/
                num_captura = ultimo_mensaje_mqtt.get("num_captura", 1)
                with open(f"capturas/captura{num_captura}.png", "wb") as f:
                    f.write(message.payload)
                evento_ack_mqtt.set()
    except Exception as e:
        print(f"Error escuchando MQTT: {e}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    global mqtt_client
    print("Conectando al Broker MQTT...")
    try:
        async with aiomqtt.Client(BROKER_IP, BROKER_PORT) as client:
            mqtt_client = client
            print("Conectado al Broker MQTT exitosamente!!!")
            tarea_escuchar = asyncio.create_task(escuchar_mqtt(client))
            yield
            tarea_escuchar.cancel()
    except Exception as e:
        print(f"No se pudo conectar al Broker MQTT: {e}")
        yield

app = FastAPI(lifespan=lifespan)


### --> FUNCION DE ESPERA Y SINCRONIZACION
async def enviar_comando_y_esperar_ack(payload: dict, cancel_evt: asyncio.Event, timeout: float = 10.0) -> bool:
    # Envia MQTT al ESP32 y espera respuesta
    # True: recibio respuesta
    # False: cancelado o timeout
    global evento_ack_mqtt, ultimo_mensaje_mqtt, mqtt_client
    if not mqtt_client:
        return False

    evento_ack_mqtt.clear()
    ultimo_mensaje_mqtt = {}

    # Enviar comando a la ESP32
    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps(payload))

    tarea_ack = asyncio.create_task(evento_ack_mqtt.wait())
    tarea_cancel = asyncio.create_task(cancel_evt.wait())

    done, pending = await asyncio.wait(
        [tarea_ack, tarea_cancel],
        timeout=timeout,
        return_when=asyncio.FIRST_COMPLETED
    )

    for t in pending:
        t.cancel()

    # Si el usuario presiono DETENER
    if cancel_evt.is_set():
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
        return False

    # Si la ESP32 respondió dentro del tiempo
    if tarea_ack in done:
        return True

    # Timeout
    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
    return False


# --> FUNCIONES PRINCIPALES
async def ejecutar_proceso_resolver(websocket: WebSocket, cancel_evt: asyncio.Event):
    try:
        # Sincronizar ángulos de calibración guardados con el ESP32 antes de mover
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({
            "accion": "configurar_servos",
            "config": cargar_config_servos()
        }))
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Iniciando escaneo del cubo..."}))
        caras_reconocidas = 0
        intentos = 0
        caras = []
        orden = ['U', 'B', 'D', 'F', 'R', 'L']

        # Reconocimiento de las 6 caras
        while caras_reconocidas < 6:
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Escaneo cancelado por el usuario."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            if intentos == 3:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Error. No fue posible reconocer el cubo."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            # Pedir a la ESP32 que capture la foto
            ok = await enviar_comando_y_esperar_ack(
                {"accion": "capturar_foto", "num_captura": caras_reconocidas + 1},
                cancel_evt,
                timeout=15.0
            )

            if not ok:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Error o cancelación al tomar foto."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            # Reconocimiento de la cara recibida
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

            # Indicar a la ESP32 que posicione el cubo para la siguiente posicion
            await enviar_comando_y_esperar_ack({"accion": "siguiente_posicion",
                                                "numero_cara": caras_reconocidas},
                                               cancel_evt, timeout=15.0)

        if cancel_evt.is_set():
            await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
            return

        estado_actual = mapear_caras_capturadas(caras, orden)

        await websocket.send_text(json.dumps({
            "tipo": "actualizar_estado_cubo",
            "estado": estado_actual,
            "animar": False
        }))

        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Calculando solución con Kociemba..."}))
        solucion = await asyncio.to_thread(resolver_kociemba, caras, orden)

        if not solucion:
            await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "ERROR. No hay solución disponible."}))
            await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
            return

        await websocket.send_text(json.dumps({"tipo": "movimientos_resolver", "datos": solucion}))
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Solución calculada. Ejecutando en ESP32..."}))

        # Transmision movimiento a movimiento
        movimientos = solucion.split()
        for idx, mov in enumerate(movimientos):
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Ejecución de movimientos detenida por el usuario."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            # Enviar movimiento por MQTT y esperar confirmación física
            ok = await enviar_comando_y_esperar_ack({"accion": "mover_cubo", "mov": mov},
                                                    cancel_evt, timeout=15.0)
            if not ok:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Movimientos detenidos o error en ESP32."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            estado_actual = actualizar_estado(estado_actual, mov)

            await websocket.send_text(json.dumps({
                "tipo": "actualizar_estado_cubo",
                "estado": estado_actual,
                "movimiento": mov,
                "progreso": idx + 1,
                "total": len(movimientos),
                "modo": "resolver",
                "animar": True
            }))

            #### ELIMINAR CUANDO TENGA EL CUBO MAGNETICO
            if idx < len(movimientos) - 1:  # Solo si no es el último movimiento
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Pausa de 5s: Acomoda el cubo..."}))
                if await esperar_o_cancelar(5.0, cancel_evt):
                    await websocket.send_text(json.dumps(
                        {"tipo": "terminal", "mensaje": "Proceso detenido por el usuario durante la pausa."}))
                    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                    return

        # Finalizacion
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "fin_proceso"}))
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Proceso finalizado con éxito!!!"}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))

    except Exception as e:
        print(f"Error en proceso resolver: {e}")
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": f"Error en proceso resolver: {e}"}))
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))


async def ejecutar_proceso_mezclar(websocket: WebSocket, secuencia: str, cancel_evt: asyncio.Event):
    try:
        # Sincronizar ángulos de calibración guardados con el ESP32 antes de mover
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({
            "accion": "configurar_servos",
            "config": cargar_config_servos()
        }))
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Enviando secuencia de mezcla al ESP32..."}))
        estado_actual = obtener_estado_resuelto()
        movimientos = secuencia.split()

        for idx, mov in enumerate(movimientos):
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Mezcla detenida por el usuario."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            ok = await enviar_comando_y_esperar_ack({"accion": "mover_cubo", "mov": mov},
                                                    cancel_evt, timeout=15.0)
            if not ok:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Mezcla interrumpida por el usuario o error."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            estado_actual = actualizar_estado(estado_actual, mov)

            await websocket.send_text(json.dumps({
                "tipo": "actualizar_estado_cubo",
                "estado": estado_actual,
                "movimiento": mov,
                "progreso": idx + 1,
                "total": len(movimientos),
                "modo": "mezclar",
                "animar": True
            }))

            #### ELIMINAR CUANDO TENGA EL CUBO MAGNETICO
            if idx < len(movimientos) - 1:  # Solo si no es el último movimiento
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Pausa de 5s: Acomoda el cubo..."}))
                if await esperar_o_cancelar(5.0, cancel_evt):
                    await websocket.send_text(json.dumps(
                        {"tipo": "terminal", "mensaje": "Proceso detenido por el usuario durante la pausa."}))
                    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                    return

        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "fin_proceso"}))
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Mezcla completada."}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))

    except Exception as e:
        print(f"Error en proceso mezclar: {e}")
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": f"Error en proceso mezclar: {e}"}))
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))

async def ejecutar_proceso_capturar(websocket: WebSocket, cancel_evt: asyncio.Event):
    global estado_ajustes
    try:
        # Sincronizar ángulos de calibración guardados con el ESP32 antes de mover
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({
            "accion": "configurar_servos",
            "config": cargar_config_servos()
        }))
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": "Iniciando escaneo del cubo (Ajustes)..."}))
        caras_reconocidas = 0
        intentos = 0
        caras = []
        orden = ['U', 'B', 'D', 'F', 'R', 'L']

        while caras_reconocidas < 6:
            if cancel_evt.is_set():
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Escaneo cancelado por el usuario."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            if intentos == 3:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Error. No fue posible reconocer el cubo."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            ok = await enviar_comando_y_esperar_ack(
                {"accion": "capturar_foto", "num_captura": caras_reconocidas + 1},
                cancel_evt,
                timeout=15.0
            )
            if not ok:
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "Error o cancelación al tomar foto."}))
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                return

            cara_detectada = await asyncio.to_thread(reconocer_cara, f"capturas/captura{caras_reconocidas + 1}.png")

            if "Desconocido" in cara_detectada:
                intentos += 1
                continue

            caras.append(cara_detectada)
            caras_reconocidas += 1
            intentos = 0
            await websocket.send_text(
                json.dumps({"tipo": "terminal", "mensaje": f"Lista la cara {caras_reconocidas}."}))

            # Indicar a la ESP32 que posicione el cubo para la siguiente posicion
            await enviar_comando_y_esperar_ack({"accion": "siguiente_posicion",
                                                "numero_cara": caras_reconocidas},
                                               cancel_evt, timeout=15.0)

        estado_actual = mapear_caras_capturadas(caras, orden)
        estado_ajustes = estado_actual

        await websocket.send_text(json.dumps({
            "tipo": "actualizar_estado_cubo",
            "estado": estado_actual,
            "animar": False
        }))

        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "fin_proceso"}))
        await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Escaneo completado."}))
        await websocket.send_text(json.dumps({"tipo": "proceso_completado"}))

    except Exception as e:
        print(f"Error en captura: {e}")
        await websocket.send_text(
            json.dumps({"tipo": "terminal", "mensaje": f"Error en proceso captura: {e}"}))
        await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))

### --> WEBSOCKET ENDPOINT
@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global tarea_proceso_actual, cancel_event
    await websocket.accept()
    print("--> Interfaz conectada")

    # Al conectar, enviamos los angulos guardados en el archivo a la interfaz
    config_actual = cargar_config_servos()
    await websocket.send_text(json.dumps({
        "tipo": "cargar_servos",
        "config": config_actual
    }))

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
                    await websocket.send_text(
                        json.dumps({"tipo": "preparar_mezcla", "estado": obtener_estado_resuelto()}))
                    await websocket.send_text(
                        json.dumps({"tipo": "terminal", "mensaje": "Archivo cargado correctamente."}))
                else:
                    await websocket.send_text(json.dumps({"tipo": "terminal", "mensaje": "Selección cancelada."}))

            elif comando == "generar_movimientos":
                scramble = generar_scramble_aleatorio()
                await websocket.send_text(json.dumps({"tipo": "secuencia_leida", "datos": scramble}))
                await websocket.send_text(json.dumps({"tipo": "preparar_mezcla", "estado": obtener_estado_resuelto()}))
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
                clave = mensaje.get("clave")

                if clave and angulo is not None:
                    config = cargar_config_servos()
                    config[clave] = angulo
                    guardar_config_servos(config)

                # Publicar posicion de calibración por MQTT
                if mqtt_client:
                    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({
                        "accion": "mover_servo",
                        "servo": servo,
                        "clave": clave,
                        "angulo": angulo
                    }))

                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": f"{servo} posicionado a {angulo}°"}))

            elif comando == "guardar_servo":
                clave = mensaje.get("clave")
                angulo = mensaje.get("angulo")

                if clave and angulo is not None:
                    config = cargar_config_servos()
                    config[clave] = angulo
                    guardar_config_servos(config)

            elif comando == "ejecutar_movimiento_ajustes":
                mov = mensaje.get("movimiento")
                scan = mensaje.get("scan")
                if mov:
                    cancel_event = asyncio.Event()
                    await websocket.send_text(json.dumps({
                        "tipo": "terminal",
                        "mensaje": f"Ejecutando movimiento {mov} en ESP32..."
                    }))
                    ok = await enviar_comando_y_esperar_ack({"accion": "mover_cubo", "mov": mov}, cancel_event,
                                                            timeout=15.0)
                    if ok:
                        if scan:
                            global estado_ajustes
                            estado_ajustes = actualizar_estado(estado_ajustes, mov)
                            await websocket.send_text(json.dumps({
                                "tipo": "actualizar_estado_cubo",
                                "estado": estado_ajustes,
                                "movimiento": mov,
                                "modo": "ajustes",
                                "animar": True
                            }))
                        await websocket.send_text(
                            json.dumps({"tipo": "terminal", "mensaje": f"Movimiento {mov} realizado con éxito."}))

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
                    cancel_event.set()
                if mqtt_client:
                    await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "detener"}))
                await websocket.send_text(
                    json.dumps({"tipo": "terminal", "mensaje": "STOP. Servos a posición natural."}))

            elif comando == "resetear_orientacion":
                await mqtt_client.publish(TOPICO_COMANDOS, json.dumps({"accion": "resetear_orientacion"}))

    except WebSocketDisconnect:
        print("X-> Interfaz desconectada")
        if cancel_event:
            cancel_event.set()


if __name__ == "__main__":
    # Configuración del servidor Uvicorn
    config = uvicorn.Config(app=app, host="127.0.0.1", port=8000, loop="asyncio")
    server = uvicorn.Server(config)

    # Forzar el SelectorEventLoop en Windows antes de iniciar Uvicorn
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
        loop = asyncio.WindowsSelectorEventLoopPolicy().new_event_loop()
        asyncio.set_event_loop(loop)
        loop.run_until_complete(server.serve())
    else:
        server.run()