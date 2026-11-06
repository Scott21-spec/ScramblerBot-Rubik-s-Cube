import os
import json

CONFIG_SERVOS_FILE = "archivos/config_servos.json"

DEFAULT_SERVOS = {
    "inf-der": 0,
    "inf-cen": 90,
    "inf-izq": 180,
    "sup-aba": 0,
    "sup-arr": 90,
    "sup-vol": 180
}

def cargar_config_servos():
    if os.path.exists(CONFIG_SERVOS_FILE):
        try:
            with open(CONFIG_SERVOS_FILE, "r") as f:
                return json.load(f)
        except Exception as e:
            print(f"Error leyendo {CONFIG_SERVOS_FILE}: {e}")
            return DEFAULT_SERVOS.copy()
    return DEFAULT_SERVOS.copy()

def guardar_config_servos(config):
    try:
        with open(CONFIG_SERVOS_FILE, "w") as f:
            json.dump(config, f, indent=4)
    except Exception as e:
        print(f"Error guardando {CONFIG_SERVOS_FILE}: {e}")