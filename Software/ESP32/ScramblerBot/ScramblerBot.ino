#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
//#include "esp_camera.h"



//// CONFIGURACION DE RED Y MQTT
const char* WIFI_SSID     = "FIUNA";               // Nombre del Wi-Fi
const char* WIFI_PASSWORD = "fiuna#2024";          // Contraseña del Wi-Fi
const char* MQTT_BROKER   = "172.16.240.137";      // Dirección IP de PC
const int   MQTT_PORT     = 1883;

const char* BOT_ID        = "bot1";
String topicoComandos     = "scramblerbot/" + String(BOT_ID) + "/comandos";
String topicoEstado       = "scramblerbot/" + String(BOT_ID) + "/estado";
String topicoImagen       = "scramblerbot/" + String(BOT_ID) + "/imagen";

WiFiClient espClient;
PubSubClient mqttClient(espClient);


//// CONFIGURACION DE SERVOS Y PINOUT
Servo servoInferior;
Servo servoSuperior;

const int PIN_SERVO_INFERIOR = 7;
const int PIN_SERVO_SUPERIOR = 6;

// Variables de calibracion 
int INF_CENTRO    = 90;
int INF_DERECHA   = 0;
int INF_IZQUIERDA = 180;

int SUP_ABAJO  = 88;
int SUP_ARRIBA = 52;
int SUP_VOLTEO = 0;

// Tiempos de movimiento
const int T_INFERIOR = 500;
const int T_SUPERIOR = 350;
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

volatile bool procesoCancelado = false;


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
  servoInferior.write(INF_CENTRO);
  posicionInferior = INF_CENTRO;
  esperarMQTT(T_INFERIOR);
}

void inferiorDerecha() {
  if (procesoCancelado) return;
  servoInferior.write(INF_DERECHA);
  posicionInferior = INF_DERECHA;
  esperarMQTT(T_INFERIOR);
}

void inferiorIzquierda() {
  if (procesoCancelado) return;
  servoInferior.write(INF_IZQUIERDA);
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

bool yawDerechaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;
  superiorArriba();
  inferiorDerecha();
  actualizarYawDerecha();
  return true;
}

bool yawIzquierdaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;
  superiorArriba();
  inferiorIzquierda();
  actualizarYawIzquierda();
  return true;
}

void normalizarInferiorLibre() {
  if (posicionInferior == INF_DERECHA) {
    superiorArriba();
    inferiorCentro();
    actualizarYawIzquierda();
  }
  else if (posicionInferior == INF_IZQUIERDA) {
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

/*
//// CAPTURA DE CAMARA Y ENVIO POR MQTT
void inicializarCamara() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5; config.pin_d1 = 18; config.pin_d2 = 19; config.pin_d3 = 21;
  config.pin_d4 = 36; config.pin_d5 = 39; config.pin_d6 = 34; config.pin_d7 = 35;
  config.pin_xclk = 0; config.pin_pclk = 22; config.pin_vsync = 25; config.pin_href = 23;
  config.pin_sscb_sda = 26; config.pin_sscb_scl = 27; config.pin_pwdn = 32; config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("--> Error crítico inicializando la cámara OV5642");
  }
}

void capturarYTransmitirFoto(int numCaptura) {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("--> Fallo en captura de cámara");
    return;
  }

  // Transmisión streaming eficiente en bloques sin saturar búfer
  mqttClient.beginPublish(topicoImagen.c_str(), fb->len, false);
  mqttClient.write(fb->buf, fb->len);
  mqttClient.endPublish();

  esp_camera_fb_return(fb);

  // Confirmar a Python que la foto fue tomada y enviada
  StaticJsonDocument<128> doc;
  doc["tipo"] = "foto_ok";
  doc["num_captura"] = numCaptura;
  String output;
  serializeJson(doc, output);
  mqttClient.publish(topicoEstado.c_str(), output.c_str());
}
*/

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
    if (config.containsKey("inf-der")) INF_DERECHA   = config["inf-der"];
    if (config.containsKey("inf-cen")) INF_CENTRO    = config["inf-cen"];
    if (config.containsKey("inf-izq")) INF_IZQUIERDA = config["inf-izq"];
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

    if (clave == "inf-der") { INF_DERECHA = angulo; inferiorDerecha(); }
    else if (clave == "inf-cen") { INF_CENTRO = angulo; inferiorCentro(); }
    else if (clave == "inf-izq") { INF_IZQUIERDA = angulo; inferiorIzquierda(); }
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
/*
  // CAPTURAR FOTO
  else if (accion == "capturar_foto") {
    int numCaptura = doc["num_captura"] | 1;
    capturarYTransmitirFoto(numCaptura);
  }
*/
  // CAMBIAR POSICION
  else if (accion == "siguiente_posicion") {
    volteoVertical(); // Rota físicamente el cubo a la siguiente cara
    StaticJsonDocument<128> res;
    res["tipo"] = "posicion_ok";
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
  servoInferior.attach(PIN_SERVO_INFERIOR, 500, 2400);
  servoSuperior.attach(PIN_SERVO_SUPERIOR, 500, 2400);

  servoInferior.write(INF_CENTRO);
  servoSuperior.write(SUP_ARRIBA);

  // Inicializar cámara OV5642
  //inicializarCamara();

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