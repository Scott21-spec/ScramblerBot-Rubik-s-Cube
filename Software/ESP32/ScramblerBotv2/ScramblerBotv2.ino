#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"
#include "esp_heap_caps.h"

//// CONFIGURACION DE RED Y MQTT
const char* WIFI_SSID     = "Cami:)";               // Nombre del Wi-Fi
const char* WIFI_PASSWORD = "kowalewski";          // Contraseña del Wi-Fi
const char* MQTT_BROKER   = "10.169.193.244";       // Dirección IP actual de la PC
const int   MQTT_PORT     = 1883;

const char* BOT_ID        = "bot1";
String topicoComandos     = "scramblerbot/" + String(BOT_ID) + "/comandos";
String topicoEstado       = "scramblerbot/" + String(BOT_ID) + "/estado";
String topicoImagen       = "scramblerbot/" + String(BOT_ID) + "/imagen";


WiFiClient espClient;
PubSubClient mqttClient(espClient);

// La Arducam Mini 5MP Plus OV5642 NO usa esp_camera.h.
// Usa I2C/SCCB para configurar el sensor y SPI para leer su FIFO.
#if !(defined (OV5642_MINI_5MP_PLUS))
#error "Habilita OV5642_MINI_5MP_PLUS en memorysaver.h de la libreria ArduCAM"
#endif

// Pinout documentado de la PCB ScramblerBot
const int CAM_CS   = 48;
const int CAM_MOSI = 47;
const int CAM_MISO = 21;
const int CAM_SCK  = 14;
const int CAM_SDA  = 13;
const int CAM_SCL  = 12;

ArduCAM myCAM(OV5642, CAM_CS);
bool camaraOK = false;
bool procesoCancelado = false;

// Durante el escaneo v8.3 se congela la cámara después de capturar U.
// No se usa ningún preset manual de color: se conservan exactamente
// los valores automáticos que la propia OV5642 encontró.
bool parametrosCamaraBloqueados = false;

bool bloquearParametrosCamaraActuales() {
  if (!camaraOK) return false;

  uint8_t awb[6] = {0};
  uint8_t expo[3] = {0};
  uint8_t gain[2] = {0};
  bool ok = true;

  for (int i = 0; i < 6; i++) {
    if (myCAM.rdSensorReg16_8(0x3400 + i, &awb[i]) != 0) ok = false;
  }
  for (int i = 0; i < 3; i++) {
    if (myCAM.rdSensorReg16_8(0x3500 + i, &expo[i]) != 0) ok = false;
  }
  if (myCAM.rdSensorReg16_8(0x350A, &gain[0]) != 0) ok = false;
  if (myCAM.rdSensorReg16_8(0x350B, &gain[1]) != 0) ok = false;

  if (!ok) {
    Serial.println("--> ERROR leyendo parametros automaticos de OV5642");
    return false;
  }

  // AWB manual con las ganancias actuales.
  if (myCAM.wrSensorReg16_8(0x3406, 0x01) != 0) ok = false;
  for (int i = 0; i < 6; i++) {
    if (myCAM.wrSensorReg16_8(0x3400 + i, awb[i]) != 0) ok = false;
  }

  // AEC + AGC manuales. Conservamos exposición y ganancia actuales.
  if (myCAM.wrSensorReg16_8(0x3503, 0x03) != 0) ok = false;
  for (int i = 0; i < 3; i++) {
    if (myCAM.wrSensorReg16_8(0x3500 + i, expo[i]) != 0) ok = false;
  }
  if (myCAM.wrSensorReg16_8(0x350A, gain[0]) != 0) ok = false;
  if (myCAM.wrSensorReg16_8(0x350B, gain[1]) != 0) ok = false;

  parametrosCamaraBloqueados = ok;

  Serial.printf(
    "--> CAMARA BLOQUEADA | AWB R=%02X%02X G=%02X%02X B=%02X%02X "
    "| EXP=%02X%02X%02X | GAIN=%02X%02X\n",
    awb[0], awb[1], awb[2], awb[3], awb[4], awb[5],
    expo[0], expo[1], expo[2], gain[0], gain[1]
  );

  return ok;
}

bool liberarParametrosCamara() {
  if (!camaraOK) return false;

  bool ok = true;
  if (myCAM.wrSensorReg16_8(0x3406, 0x00) != 0) ok = false; // AWB auto
  if (myCAM.wrSensorReg16_8(0x3503, 0x00) != 0) ok = false; // AEC/AGC auto

  parametrosCamaraBloqueados = false;
  Serial.println("--> CAMARA AUTO: AWB + AEC + AGC restaurados");
  delay(120);
  return ok;
}


//// CONFIGURACION DE SERVOS Y PINOUT
Servo servoInferior;
Servo servoSuperior;

const int PIN_SERVO_INFERIOR = 7;
const int PIN_SERVO_SUPERIOR = 6;

// Variables de calibracion del servo inferior POSICIONAL 270°
int INF_CENTRO    = 130;
int INF_DERECHA   = 0;
int INF_IZQUIERDA = 260;

// Rango PWM del servo inferior 270°. Si el servo no recorre exactamente
// el ángulo esperado, se ajustan los ángulos desde la interfaz.
// No se usa servo.write(angulo) para valores >180.
const int INF_PULSO_MIN_US = 500;
const int INF_PULSO_MAX_US = 2500;
const int INF_RANGO_GRADOS = 270;

int pulsoInferiorDesdeAngulo(int angulo) {
  angulo = constrain(angulo, 0, INF_RANGO_GRADOS);
  return map(angulo, 0, INF_RANGO_GRADOS, INF_PULSO_MIN_US, INF_PULSO_MAX_US);
}

void escribirInferior270(int angulo) {
  angulo = constrain(angulo, 0, INF_RANGO_GRADOS);
  servoInferior.writeMicroseconds(pulsoInferiorDesdeAngulo(angulo));
}

int SUP_ABAJO  = 88;
int SUP_ARRIBA = 52;
int SUP_VOLTEO = 0;

// Tiempos de movimiento
const int T_INFERIOR = 700;
const int T_SUPERIOR = 700;
const int T_VOLTEO   = 700;

int posicionInferior = INF_CENTRO;
int posicionSuperior = SUP_ARRIBA;

// Control de tiempos globales para el resumen final
unsigned long tiempoInicioProceso = 0;


//// ORIENTACION DEL CUBO
char caraU = 'U', caraF = 'F', caraR = 'R', caraD = 'D', caraB = 'B', caraL = 'L';

void actualizarYawDerecha() {
  char oldF = caraF, oldR = caraR, oldB = caraB, oldL = caraL;
  caraF = oldR; caraL = oldF; caraB = oldL; caraR = oldB;
}

void actualizarYawIzquierda() {
  char oldF = caraF, oldR = caraR, oldB = caraB, oldL = caraL;
  caraF = oldL; caraR = oldF; caraB = oldR; caraL = oldB;
}

void actualizarVolteoVertical() {
  char oldU = caraU, oldF = caraF, oldD = caraD, oldB = caraB;
  caraU = oldB; caraF = oldU; caraD = oldF; caraB = oldD;
}

char posicionFisicaDeCara(char objetivo) {
  if (caraU == objetivo) return 'U';
  if (caraF == objetivo) return 'F';
  if (caraR == objetivo) return 'R';
  if (caraD == objetivo) return 'D';
  if (caraB == objetivo) return 'B';
  if (caraL == objetivo) return 'L';
  return '?';
}


//// MOVIMIENTOS DE SERVOS
// Reemplazo para delay() que sigue escuchando MQTT
bool esperarMQTT(int ms) {
  unsigned long inicio = millis();
  while (millis() - inicio < ms) {
    mqttClient.loop(); 
    if (procesoCancelado) {
      return false; // Abortar tiempo de espera si se presionó DETENER
    }
    delay(10);
  }
  return true;      // Timeout
}

void superiorAbajo() {
  if (procesoCancelado) return;
  servoSuperior.write(SUP_ABAJO);
  posicionSuperior = SUP_ABAJO;
  esperarMQTT(T_SUPERIOR);
}

void superiorArriba() {
  servoSuperior.write(SUP_ARRIBA);
  posicionSuperior = SUP_ARRIBA;
  esperarMQTT(T_SUPERIOR);
}

void inferiorCentro() {
  if (procesoCancelado) return;
  escribirInferior270(INF_CENTRO);
  posicionInferior = INF_CENTRO;
  esperarMQTT(T_INFERIOR);
}

void inferiorDerecha() {
  if (procesoCancelado) return;
  escribirInferior270(INF_DERECHA);
  posicionInferior = INF_DERECHA;
  esperarMQTT(T_INFERIOR);
}

void inferiorIzquierda() {
  if (procesoCancelado) return;
  escribirInferior270(INF_IZQUIERDA);
  posicionInferior = INF_IZQUIERDA;
  esperarMQTT(T_INFERIOR);
}

void volteoVertical() {
  if (procesoCancelado) return;
  superiorArriba();
  servoSuperior.write(SUP_VOLTEO);
  posicionSuperior = SUP_VOLTEO;
  esperarMQTT(T_VOLTEO);
  superiorArriba();
  actualizarVolteoVertical();
}

// IMPORTANTE PARA EL SERVO POSICIONAL 270° ACTUAL:
// Por la orientación mecánica del servo, mover la base hacia INF_IZQUIERDA=260°
// produce un YAW FÍSICO A LA DERECHA del cubo, y moverla hacia
// INF_DERECHA=0° produce un YAW FÍSICO A LA IZQUIERDA.
// Las funciones siguientes representan el movimiento REAL DEL CUBO,
// no el nombre del extremo del servo.
bool yawDerechaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;
  superiorArriba();
  inferiorDerecha();      // 130° -> 260° = yaw físico derecha
  actualizarYawDerecha();
  return true;
}

bool yawIzquierdaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;
  superiorArriba();
  inferiorIzquierda();        // 130° -> 0° = yaw físico izquierda
  actualizarYawIzquierda();
  return true;
}

void normalizarInferiorLibre() {
  if (posicionInferior == INF_DERECHA) {
    // 0° -> 130° deshace un yaw físico izquierda = yaw físico derecha.
    superiorArriba();
    inferiorCentro();
    actualizarYawIzquierda();
  }
  else if (posicionInferior == INF_IZQUIERDA) {
    // 260° -> 130° deshace un yaw físico derecha = yaw físico izquierda.
    superiorArriba();
    inferiorCentro();
    actualizarYawDerecha();
  }
}

bool orientarCaraAbajo(char objetivo) {
  normalizarInferiorLibre();
  char pos = posicionFisicaDeCara(objetivo);

  if (pos == 'D') return true;
  if (pos == 'F') volteoVertical();
  else if (pos == 'U') { volteoVertical(); volteoVertical(); }
  else if (pos == 'B') { volteoVertical(); volteoVertical(); volteoVertical(); }
  else if (pos == 'R') {
    if (!yawDerechaDesdeCentro()) return false;
    volteoVertical();
    normalizarInferiorLibre();
  }
  else if (pos == 'L') {
    if (!yawIzquierdaDesdeCentro()) return false;
    volteoVertical();
    normalizarInferiorLibre();
  }
  else return false;

  return posicionFisicaDeCara(objetivo) == 'D' && posicionInferior == INF_CENTRO;
}

void girarCaraAbajo90(int sentido) {
  normalizarInferiorLibre();
  superiorAbajo();
  if (sentido > 0) inferiorIzquierda();
  else inferiorDerecha();
  superiorArriba();
  normalizarInferiorLibre();
}

bool ejecutarCara(char objetivo, int cantidad) {
  if (!orientarCaraAbajo(objetivo)) return false;

  if (cantidad == 1) girarCaraAbajo90(+1);
  else if (cantidad == -1) girarCaraAbajo90(-1);
  else if (cantidad == 2) { girarCaraAbajo90(+1); girarCaraAbajo90(+1); }
  else return false;

  return true;
}

bool ejecutarMovimientoWCA(String mov) {
  mov.trim();
  if (mov.length() < 1) return false;

  char cara = mov.charAt(0);
  int cantidad = 1;

  if (mov.length() > 1) {
    if (mov.charAt(1) == '\'') cantidad = -1;
    else if (mov.charAt(1) == '2') cantidad = 2;
  }

  return ejecutarCara(cara, cantidad);
}


//// CAPTURA DE CAMARA Y ENVIO POR MQTT

void publicarEstadoCamara(const char* tipo, int numCaptura, const String& error = "") {
  StaticJsonDocument<192> doc;
  doc["tipo"] = tipo;
  doc["num_captura"] = numCaptura;
  if (error.length()) doc["error"] = error;

  String output;
  serializeJson(doc, output);
  mqttClient.publish(topicoEstado.c_str(), output.c_str());
}

bool inicializarCamara() {
  uint8_t vid = 0, pid = 0;
  uint8_t temp = 0;

  pinMode(CAM_CS, OUTPUT);
  digitalWrite(CAM_CS, HIGH);

  Wire.begin(CAM_SDA, CAM_SCL);
  SPI.begin(CAM_SCK, CAM_MISO, CAM_MOSI, CAM_CS);
  SPI.setFrequency(4000000);  // 4 MHz: conservador y estable

  // Verificar comunicación con el controlador FIFO ArduCAM
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55) {
    Serial.println("--> ERROR: no responde la interfaz SPI de ArduCAM");
    return false;
  }

  // Verificar sensor OV5642
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);

  Serial.printf("--> OV5642 Chip ID: 0x%02X 0x%02X\n", vid, pid);

  if (vid != 0x56 || pid != 0x42) {
    Serial.println("--> ERROR: no se detecta el sensor OV5642");
    return false;
  }

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5642_set_JPEG_size(OV5642_640x480);

  // Arrancar siempre con AWB, exposición y ganancia automáticos.
  // No usamos presets Manual_cwf/Manual_A/etc.
  myCAM.wrSensorReg16_8(0x3406, 0x00);
  myCAM.wrSensorReg16_8(0x3503, 0x00);
  parametrosCamaraBloqueados = false;
  Serial.println("--> OV5642: AWB + AEC + AGC automaticos");
  delay(800);

  myCAM.clear_fifo_flag();
  myCAM.write_reg(ARDUCHIP_FRAMES, 0x00);

  Serial.println("--> Arducam Mini 5MP Plus OV5642 inicializada correctamente");
  return true;
}

bool capturarYTransmitirFoto(int numCaptura) {
  if (!camaraOK) {
    publicarEstadoCamara("foto_error", numCaptura, "camara_no_inicializada");
    return false;
  }

  Serial.printf("--> Capturando foto %d...\n", numCaptura);

  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  unsigned long inicio = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    mqttClient.loop();

    if (procesoCancelado) {
      publicarEstadoCamara("foto_error", numCaptura, "proceso_cancelado");
      return false;
    }

    if (millis() - inicio > 8000) {
      publicarEstadoCamara("foto_error", numCaptura, "timeout_captura");
      return false;
    }

    delay(5);
  }

  const uint32_t fifoLen = myCAM.read_fifo_length();
  if (fifoLen == 0 || fifoLen >= MAX_FIFO_SIZE) {
    publicarEstadoCamara("foto_error", numCaptura, "tamano_fifo_invalido");
    return false;
  }

  Serial.printf("--> FIFO capturado: %lu bytes\n", (unsigned long)fifoLen);

  // ------------------------------------------------------------------
  // IMPORTANTE:
  // La FIFO de ArduCAM puede contener bytes extra antes del marcador
  // JPEG SOI (FF D8) y después del marcador EOI (FF D9).
  // Guardamos SOLO el JPEG real antes de publicarlo por MQTT.
  // ------------------------------------------------------------------

  uint8_t* jpeg = (uint8_t*)heap_caps_malloc(
      fifoLen,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );

  // Fallback por si PSRAM no estuviera disponible.
  if (jpeg == nullptr) {
    jpeg = (uint8_t*)malloc(fifoLen);
  }

  if (jpeg == nullptr) {
    publicarEstadoCamara("foto_error", numCaptura, "sin_memoria_jpeg");
    myCAM.clear_fifo_flag();
    return false;
  }

  bool jpegIniciado = false;
  bool jpegTerminado = false;
  uint8_t byteAnterior = 0x00;
  size_t jpegLen = 0;
  long posSOI = -1;
  long posEOI = -1;

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  for (uint32_t i = 0; i < fifoLen; i++) {
    uint8_t actual = SPI.transfer(0x00);

    // Buscar FF D8. Cuando encontramos D8, el FF fue el byte anterior.
    if (!jpegIniciado) {
      if (byteAnterior == 0xFF && actual == 0xD8) {
        jpegIniciado = true;
        posSOI = (long)i - 1;

        jpeg[jpegLen++] = 0xFF;
        jpeg[jpegLen++] = 0xD8;
      }
    }
    else {
      jpeg[jpegLen++] = actual;

      // Buscar FF D9 dentro del JPEG ya iniciado.
      if (jpegLen >= 2 &&
          jpeg[jpegLen - 2] == 0xFF &&
          jpeg[jpegLen - 1] == 0xD9) {
        jpegTerminado = true;
        posEOI = (long)i - 1;  // posición del FF del marcador FF D9
        break;
      }
    }

    byteAnterior = actual;
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  if (!jpegIniciado) {
    free(jpeg);
    publicarEstadoCamara("foto_error", numCaptura, "jpeg_sin_soi");
    Serial.println("--> ERROR: no se encontró marcador JPEG FF D8");
    return false;
  }

  if (!jpegTerminado) {
    free(jpeg);
    publicarEstadoCamara("foto_error", numCaptura, "jpeg_sin_eoi");
    Serial.println("--> ERROR: no se encontró marcador JPEG FF D9");
    return false;
  }

  Serial.printf(
      "--> JPEG limpio: %u bytes | SOI=%ld | EOI=%ld\n",
      (unsigned int)jpegLen,
      posSOI,
      posEOI
  );

  // PubSubClient beginPublish necesita conocer el tamaño exacto del payload.
  String topicoFoto = topicoImagen + "/" + String(numCaptura);

  if (!mqttClient.beginPublish(
        topicoFoto.c_str(),
        (unsigned int)jpegLen,
        false
      )) {
    free(jpeg);
    publicarEstadoCamara("foto_error", numCaptura, "mqtt_begin_publish");
    return false;
  }

  const size_t TAM_BLOQUE = 1024;
  size_t enviadosTotales = 0;

  while (enviadosTotales < jpegLen) {
    size_t restantes = jpegLen - enviadosTotales;
    size_t cantidad = restantes > TAM_BLOQUE ? TAM_BLOQUE : restantes;

    size_t enviados = mqttClient.write(
        jpeg + enviadosTotales,
        cantidad
    );

    if (enviados != cantidad) {
      mqttClient.endPublish();
      free(jpeg);
      publicarEstadoCamara("foto_error", numCaptura, "mqtt_write_incompleto");
      Serial.printf(
          "--> ERROR MQTT: enviados %u de %u bytes en bloque\n",
          (unsigned int)enviados,
          (unsigned int)cantidad
      );
      return false;
    }

    enviadosTotales += enviados;
    mqttClient.loop();
  }

  if (!mqttClient.endPublish()) {
    free(jpeg);
    publicarEstadoCamara("foto_error", numCaptura, "mqtt_end_publish");
    return false;
  }

  free(jpeg);

  publicarEstadoCamara("foto_ok", numCaptura);
  Serial.printf(
      "--> Foto %d enviada por MQTT: %u bytes JPEG válidos\n",
      numCaptura,
      (unsigned int)jpegLen
  );

  return true;
}


//// COMANDOS MQTT
void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) return;

  String accion = doc["accion"] | "";

  // Si llega un nuevo comando, reseteamos la bandera de cancelacion
  if (accion != "detener") {
    procesoCancelado = false;
  }

  // DETENER PROCESO (PARADA DE EMERGENCIA)
  if (accion == "detener") {
    procesoCancelado = true; // Activa corte inmediato en los bucles de movimiento
    superiorArriba();        // Eleva el brazo para liberar el cubo
    
    StaticJsonDocument<128> res;
    res["tipo"] = "detenido_ok";
    String output;
    serializeJson(res, output);
    mqttClient.publish(topicoEstado.c_str(), output.c_str());
    Serial.println("--> PARADA DE EMERGENCIA EJECUTADA EN ESP32");
    return;
  }
  
  // CONFIGURACION DE SERVOS (config_servos.json)
  else if (accion == "configurar_servos") {
    JsonObject config = doc["config"];
    if (config.containsKey("inf-der")) INF_DERECHA   = constrain((int)config["inf-der"], 0, 270);
    if (config.containsKey("inf-cen")) INF_CENTRO    = constrain((int)config["inf-cen"], 0, 270);
    if (config.containsKey("inf-izq")) INF_IZQUIERDA = constrain((int)config["inf-izq"], 0, 270);
    if (config.containsKey("sup-aba")) SUP_ABAJO     = config["sup-aba"];
    if (config.containsKey("sup-arr")) SUP_ARRIBA    = config["sup-arr"];
    if (config.containsKey("sup-vol")) SUP_VOLTEO    = config["sup-vol"];

    tiempoInicioProceso = millis(); // Reinicia temporizador de resumen
    Serial.println("Ángulos de servos calibrados correctamente desde Python.");
  }

  // MOVER SERVO INDIVIDUAL (Ajustes)
  else if (accion == "mover_servo") {
    String clave = doc["clave"] | "";
    int angulo = doc["angulo"] | 90;

    if (clave == "inf-der") { INF_DERECHA = constrain(angulo, 0, 270); inferiorDerecha(); }
    else if (clave == "inf-cen") { INF_CENTRO = constrain(angulo, 0, 270); inferiorCentro(); }
    else if (clave == "inf-izq") { INF_IZQUIERDA = constrain(angulo, 0, 270); inferiorIzquierda(); }
    else if (clave == "sup-aba") { SUP_ABAJO = angulo; superiorAbajo(); }
    else if (clave == "sup-arr") { SUP_ARRIBA = angulo; superiorArriba(); }
    else if (clave == "sup-vol") { SUP_VOLTEO = angulo; superiorArriba(); servoSuperior.write(SUP_VOLTEO); }
  }

  // EJECUTAR MOVIMIENTO DE CUBO 
  else if (accion == "mover_cubo") {
    String mov = doc["mov"] | "";
    bool ok = ejecutarMovimientoWCA(mov);

    if (ok) {
      StaticJsonDocument<128> res;
      res["tipo"] = "movimiento_ok";
      res["mov"] = mov;
      String output;
      serializeJson(res, output);
      mqttClient.publish(topicoEstado.c_str(), output.c_str());
    }
  }

  // CONGELAR COLOR/EXPOSICION PARA EL ESCANEO DE 6 CARAS
  else if (accion == "vision_bloquear_camara") {
    bool ok = bloquearParametrosCamaraActuales();

    StaticJsonDocument<160> res;
    res["tipo"] = ok ? "camara_bloqueada" : "camara_bloqueo_error";
    String output;
    serializeJson(res, output);
    mqttClient.publish(topicoEstado.c_str(), output.c_str());
  }

  // RESTAURAR AUTOMATICOS AL FINALIZAR/CANCELAR EL ESCANEO
  else if (accion == "vision_liberar_camara") {
    bool ok = liberarParametrosCamara();

    StaticJsonDocument<160> res;
    res["tipo"] = ok ? "camara_liberada" : "camara_liberar_error";
    String output;
    serializeJson(res, output);
    mqttClient.publish(topicoEstado.c_str(), output.c_str());
  }

  // CAPTURAR FOTO
  else if (accion == "capturar_foto") {
    int numCaptura = doc["num_captura"] | 1;
    capturarYTransmitirFoto(numCaptura);
  }

  // CAMBIAR POSICION PARA CAPTURAR U, B, D, F, L, R
  else if (accion == "siguiente_posicion") {
    int numeroCara = doc["numero_cara"] | 0;
    bool ok = true;

    // numeroCara = cara que ACABA de capturarse.
    // Ruta física verificada: U -> B -> D -> F -> L -> R.
    // La cámara siempre observa la cara física superior U.
    if (numeroCara == 1) {
      // Capturada U. Un volteo: B -> U.
      volteoVertical();
    }
    else if (numeroCara == 2) {
      // Capturada B. Un volteo: D -> U.
      volteoVertical();
    }
    else if (numeroCara == 3) {
      // Capturada D. Un volteo: F -> U.
      volteoVertical();
    }
    else if (numeroCara == 4) {
      // Capturada F. Primero un volteo devuelve la orientación vertical inicial.
      volteoVertical();

      // El movimiento físico actual lleva L a la cámara:
      // yaw físico izquierda -> R pasa a B; luego volteo -> B pasa a U.
      if (posicionInferior != INF_CENTRO) normalizarInferiorLibre();
      if (!yawIzquierdaDesdeCentro()) ok = false;
      if (ok) volteoVertical();
    }
    else if (numeroCara == 5) {
      // Capturada L. Su opuesta R está en D.
      // Dos volteos (180°) intercambian U y D -> R queda en U.
      volteoVertical();
      volteoVertical();
    }

    StaticJsonDocument<128> res;
    res["tipo"] = ok ? "posicion_ok" : "posicion_error";
    res["numero_cara"] = numeroCara;

    String output;
    serializeJson(res, output);
    mqttClient.publish(topicoEstado.c_str(), output.c_str());
  }

  // FINALIZAR Y ENVIAR RESUMEN
  else if (accion == "fin_proceso") {
    float tTotal = (millis() - tiempoInicioProceso) / 1000.0;
    StaticJsonDocument<256> res;
    res["tipo"] = "resumen";
    res["t_total"] = tTotal;
    String output;
    serializeJson(res, output);
    mqttClient.publish(topicoEstado.c_str(), output.c_str());
  }

  // RESETEAR ORIENTACION
  else if (accion == "resetear_orientacion") {
    caraU = 'U';
    caraF = 'F';
    caraR = 'R';
    caraD = 'D';
    caraB = 'B';
    caraL = 'L';

    // Asegurar que los servos regresen a la posición neutral inicial
    superiorArriba();
    inferiorCentro();
  }
}


//// CONEXION WIFICLIENT Y MQTT

void reconectarMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando al Broker MQTT...");
    if (mqttClient.connect(BOT_ID)) {
      Serial.println(" ¡Conectado!");
      mqttClient.subscribe(topicoComandos.c_str());
    } else {
      Serial.print(" Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en 2 segundos...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  servoInferior.setPeriodHertz(50);
  servoSuperior.setPeriodHertz(50);
  servoInferior.attach(PIN_SERVO_INFERIOR, INF_PULSO_MIN_US, INF_PULSO_MAX_US);
  servoSuperior.attach(PIN_SERVO_SUPERIOR, 500, 2400);

  Serial.println("--> Servo inferior POSICIONAL 270: DER=0, CENTRO=130, IZQ=260");
  Serial.println("--> Yaw calibrado: 130->0 = yaw IZQUIERDA; 130->260 = yaw DERECHA");
  Serial.println("--> Servo superior: ABAJO=88, ARRIBA=52, VOLTEO=0");

  escribirInferior270(INF_CENTRO);
  servoSuperior.write(SUP_ARRIBA);

  // Inicializar Arducam Mini 5MP Plus OV5642
  camaraOK = inicializarCamara();

  // Conexión Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Conectado. IP local: " + WiFi.localIP().toString());

  // Conexión MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setBufferSize(1024); // Amplía búfer para recibir JSONs completos
  mqttClient.setCallback(callbackMQTT);
}

void loop() {
  if (!mqttClient.connected()) {
    reconectarMQTT();
  }
  mqttClient.loop();
}