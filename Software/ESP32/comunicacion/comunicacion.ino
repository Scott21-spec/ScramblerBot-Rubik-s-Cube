#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ======================================================
// SCRAMBLERBOT - HTTP + SERVOS
// Movimientos habilitados en esta etapa:
// D, D', D2, F, F', F2
// ======================================================

const char* WIFI_SSID = "ScramblerBot";
const char* WIFI_PASSWORD = "Scrambler123";

WebServer server(80);

Servo servoInferior;
Servo servoSuperior;

// Pines reales
const int PIN_SERVO_INFERIOR = 7;
const int PIN_SERVO_SUPERIOR = 6;

// Calibracion actual
int INF_CENTRO    = 90;
int INF_DERECHA   = 0;
int INF_IZQUIERDA = 180;

int SUP_ABAJO  = 164;
int SUP_ARRIBA = 138;
int SUP_VOLTEO = 90;

// Tiempos iniciales. Luego se optimizan experimentalmente.
const int T_INFERIOR = 500;
const int T_SUPERIOR = 350;
const int T_VOLTEO   = 500;

String estadoRobot = "LISTO";
int posicionInferior = INF_CENTRO;
int posicionSuperior = SUP_ARRIBA;

// ------------------------------------------------------
// Orientacion logica del cubo.
// Cada variable indica que cara logica esta en esa
// posicion fisica.
// Estado inicial:
// U=blanco, F=verde, R=rojo, D=amarillo, B=azul, L=naranja
// ------------------------------------------------------
char caraU = 'U';
char caraF = 'F';
char caraR = 'R';
char caraD = 'D';
char caraB = 'B';
char caraL = 'L';

void enviarJSON(int codigo, const String& contenido) {
  server.send(codigo, "application/json", contenido);
}

void mostrarOrientacion() {
  Serial.println();
  Serial.println("ORIENTACION ACTUAL");
  Serial.print("U: "); Serial.println(caraU);
  Serial.print("F: "); Serial.println(caraF);
  Serial.print("R: "); Serial.println(caraR);
  Serial.print("D: "); Serial.println(caraD);
  Serial.print("B: "); Serial.println(caraB);
  Serial.print("L: "); Serial.println(caraL);
  Serial.println();
}

// ======================================================
// ACTUALIZACION DE ORIENTACION
// ======================================================

// Rotacion horizontal hacia la derecha:
// R->F, F->L, L->B, B->R
void actualizarYawDerecha() {
  char oldF = caraF;
  char oldR = caraR;
  char oldB = caraB;
  char oldL = caraL;

  caraF = oldR;
  caraL = oldF;
  caraB = oldL;
  caraR = oldB;
}

// Rotacion horizontal hacia la izquierda:
// L->F, F->R, R->B, B->L
void actualizarYawIzquierda() {
  char oldF = caraF;
  char oldR = caraR;
  char oldB = caraB;
  char oldL = caraL;

  caraF = oldL;
  caraR = oldF;
  caraB = oldR;
  caraL = oldB;
}

// Volteo vertical puro:
// B->U, U->F, F->D, D->B
void actualizarVolteoVertical() {
  char oldU = caraU;
  char oldF = caraF;
  char oldD = caraD;
  char oldB = caraB;

  caraU = oldB;
  caraF = oldU;
  caraD = oldF;
  caraB = oldD;
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

// ======================================================
// PRIMITIVAS DE SERVOS
// ======================================================

void superiorAbajo() {
  servoSuperior.write(SUP_ABAJO);
  posicionSuperior = SUP_ABAJO;
  Serial.println("SUPERIOR -> ABAJO 164");
  delay(T_SUPERIOR);
}

void superiorArriba() {
  servoSuperior.write(SUP_ARRIBA);
  posicionSuperior = SUP_ARRIBA;
  Serial.println("SUPERIOR -> ARRIBA 138");
  delay(T_SUPERIOR);
}

void inferiorCentro() {
  servoInferior.write(INF_CENTRO);
  posicionInferior = INF_CENTRO;
  Serial.println("INFERIOR -> CENTRO 90");
  delay(T_INFERIOR);
}

void inferiorDerecha() {
  servoInferior.write(INF_DERECHA);
  posicionInferior = INF_DERECHA;
  Serial.println("INFERIOR -> DERECHA 0");
  delay(T_INFERIOR);
}

void inferiorIzquierda() {
  servoInferior.write(INF_IZQUIERDA);
  posicionInferior = INF_IZQUIERDA;
  Serial.println("INFERIOR -> IZQUIERDA 180");
  delay(T_INFERIOR);
}

// ======================================================
// REORIENTACIONES PURAS
// ======================================================

// VOLTEO = ARRIBA -> VOLTEO -> ARRIBA
void volteoVertical() {
  superiorArriba();

  servoSuperior.write(SUP_VOLTEO);
  posicionSuperior = SUP_VOLTEO;
  Serial.println("SUPERIOR -> VOLTEO 90");
  delay(T_VOLTEO);

  superiorArriba();
  actualizarVolteoVertical();
}

// Desde centro 90 hacia 0 con el cubo liberado.
// R pasa al frente.
bool yawDerechaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;

  superiorArriba();
  inferiorDerecha();
  actualizarYawDerecha();
  return true;
}

// Desde centro 90 hacia 180 con el cubo liberado.
// L pasa al frente.
bool yawIzquierdaDesdeCentro() {
  if (posicionInferior != INF_CENTRO) return false;

  superiorArriba();
  inferiorIzquierda();
  actualizarYawIzquierda();
  return true;
}

// Devuelve el servo inferior a 90 SIN girar una capa.
// Como el cubo esta liberado, esto es una rotacion del cubo completo.
void normalizarInferiorLibre() {
  if (posicionInferior == INF_DERECHA) {
    superiorArriba();
    inferiorCentro();
    actualizarYawIzquierda();   // 0 -> 90
  }
  else if (posicionInferior == INF_IZQUIERDA) {
    superiorArriba();
    inferiorCentro();
    actualizarYawDerecha();     // 180 -> 90
  }
}

// ======================================================
// LLEVAR UNA CARA LOGICA A LA POSICION FISICA D
// ======================================================

bool orientarCaraAbajo(char objetivo) {
  // Para simplificar cada movimiento empieza con el servo inferior centrado.
  normalizarInferiorLibre();

  char pos = posicionFisicaDeCara(objetivo);

  if (pos == 'D') {
    return true;
  }

  if (pos == 'F') {
    // F -> D
    volteoVertical();
  }
  else if (pos == 'U') {
    // U -> F -> D
    volteoVertical();
    volteoVertical();
  }
  else if (pos == 'B') {
    // B -> U -> F -> D
    volteoVertical();
    volteoVertical();
    volteoVertical();
  }
  else if (pos == 'R') {
    // R -> F
    if (!yawDerechaDesdeCentro()) return false;

    // F -> D
    volteoVertical();

    // Recuperar 90 sin sacar la cara objetivo de D.
    normalizarInferiorLibre();
  }
  else if (pos == 'L') {
    // L -> F
    if (!yawIzquierdaDesdeCentro()) return false;

    // F -> D
    volteoVertical();

    // Recuperar 90 sin sacar la cara objetivo de D.
    normalizarInferiorLibre();
  }
  else {
    return false;
  }

  return posicionFisicaDeCara(objetivo) == 'D' && posicionInferior == INF_CENTRO;
}

// ======================================================
// GIRAR LA CARA QUE ESTA FISICAMENTE ABAJO
// ======================================================

// sentido = +1 -> movimiento sin prima (D fisico)
// sentido = -1 -> movimiento prima (D' fisico)
//
// Validado experimentalmente:
// 90 -> 180 = D
// 90 ->   0 = D'
void girarCaraAbajo90(int sentido) {
  // Debe comenzar centrado.
  normalizarInferiorLibre();

  superiorAbajo();

  if (sentido > 0) {
    // D
    inferiorIzquierda();       // 90 -> 180
  } else {
    // D'
    inferiorDerecha();         // 90 -> 0
  }

  // Liberar antes de devolver el servo inferior.
  superiorArriba();

  // El retorno es una reorientacion del cubo completo,
  // no deshace el giro de la capa.
  normalizarInferiorLibre();
}

// Ejecuta una cara logica del cubo sin importar donde se encuentre fisicamente.
// cantidad:
//   1  = X
//  -1  = X'
//   2  = X2
bool ejecutarCara(char objetivo, int cantidad) {
  if (!orientarCaraAbajo(objetivo)) {
    Serial.println("ERROR: no se pudo llevar la cara objetivo a D");
    return false;
  }

  if (cantidad == 1) {
    girarCaraAbajo90(+1);
  }
  else if (cantidad == -1) {
    girarCaraAbajo90(-1);
  }
  else if (cantidad == 2) {
    girarCaraAbajo90(+1);
    girarCaraAbajo90(+1);
  }
  else {
    return false;
  }

  return true;
}

// ======================================================
// INTERPRETE WCA - ETAPA ACTUAL
// ======================================================

bool ejecutarMovimiento(String movimiento) {
  movimiento.trim();

  if (movimiento == "D")  return ejecutarCara('D',  1);
  if (movimiento == "D'") return ejecutarCara('D', -1);
  if (movimiento == "D2") return ejecutarCara('D',  2);

  if (movimiento == "F")  return ejecutarCara('F',  1);
  if (movimiento == "F'") return ejecutarCara('F', -1);
  if (movimiento == "F2") return ejecutarCara('F',  2);

  return false;
}

// ======================================================
// HTTP
// ======================================================

void handleInicio() {
  server.send(
    200,
    "text/html",
    "<h1>ScramblerBot</h1>"
    "<p>HTTP funcionando</p>"
    "<p>Movimientos activos: D, D', D2, F, F', F2</p>"
  );
}

void handlePing() {
  Serial.println("PING recibido");
  enviarJSON(200, "{\"ok\":true,\"dispositivo\":\"ScramblerBot\",\"estado\":\"conectado\"}");
}

void handleEstado() {
  String respuesta = "{";
  respuesta += "\"ok\":true,";
  respuesta += "\"estado\":\"" + estadoRobot + "\",";
  respuesta += "\"inferior\":" + String(posicionInferior) + ",";
  respuesta += "\"superior\":" + String(posicionSuperior) + ",";
  respuesta += "\"orientacion\":{";
  respuesta += "\"U\":\"" + String(caraU) + "\",";
  respuesta += "\"F\":\"" + String(caraF) + "\",";
  respuesta += "\"R\":\"" + String(caraR) + "\",";
  respuesta += "\"D\":\"" + String(caraD) + "\",";
  respuesta += "\"B\":\"" + String(caraB) + "\",";
  respuesta += "\"L\":\"" + String(caraL) + "\"";
  respuesta += "}}";

  enviarJSON(200, respuesta);
}

void handleMezclar() {
  if (!server.hasArg("plain")) {
    enviarJSON(400, "{\"ok\":false,\"error\":\"sin_movimiento\"}");
    return;
  }

  String movimiento = server.arg("plain");
  movimiento.trim();

  Serial.println();
  Serial.println("============================");
  Serial.print("MOVIMIENTO RECIBIDO: ");
  Serial.println(movimiento);
  Serial.println("============================");

  bool permitido =
    movimiento == "D"  || movimiento == "D'" || movimiento == "D2" ||
    movimiento == "F"  || movimiento == "F'" || movimiento == "F2";

  if (!permitido) {
    enviarJSON(
      422,
      "{\"ok\":false,\"error\":\"movimiento_no_implementado\",\"movimiento\":\"" + movimiento + "\"}"
    );
    return;
  }

  estadoRobot = "EJECUTANDO";

  bool ok = ejecutarMovimiento(movimiento);

  if (!ok) {
    estadoRobot = "ERROR";
    enviarJSON(500, "{\"ok\":false,\"error\":\"error_ejecucion\"}");
    return;
  }

  estadoRobot = "LISTO";

  mostrarOrientacion();

  enviarJSON(
    200,
    "{\"ok\":true,\"estado\":\"completed\",\"movimiento\":\"" + movimiento + "\"}"
  );
}

// ======================================================
// CONTROL MANUAL DESDE AJUSTES
//
// La GUI envia, por ejemplo:
// BOTTOM_CENTER:92
// TOP_UP:140
//
// El valor recibido se aplica al servo y tambien se guarda
// como nueva calibracion para los movimientos D/F posteriores.
// ======================================================
void handleManual() {
  if (!server.hasArg("plain")) {
    enviarJSON(400, "{\"ok\":false,\"error\":\"sin_comando_manual\"}");
    return;
  }

  String body = server.arg("plain");
  body.trim();

  int sep = body.indexOf(':');
  if (sep < 0) {
    enviarJSON(400, "{\"ok\":false,\"error\":\"formato_manual_invalido\"}");
    return;
  }

  String comando = body.substring(0, sep);
  comando.trim();
  comando.toUpperCase();

  int angulo = body.substring(sep + 1).toInt();
  if (angulo < 0 || angulo > 180) {
    enviarJSON(400, "{\"ok\":false,\"error\":\"angulo_invalido\"}");
    return;
  }

  Serial.print("MANUAL: ");
  Serial.print(comando);
  Serial.print(" -> ");
  Serial.print(angulo);
  Serial.println(" grados");

  if (comando == "BOTTOM_CENTER") {
    INF_CENTRO = angulo;
    servoInferior.write(angulo);
    posicionInferior = angulo;
  }
  else if (comando == "BOTTOM_RIGHT") {
    INF_DERECHA = angulo;
    servoInferior.write(angulo);
    posicionInferior = angulo;
  }
  else if (comando == "BOTTOM_LEFT") {
    INF_IZQUIERDA = angulo;
    servoInferior.write(angulo);
    posicionInferior = angulo;
  }
  else if (comando == "TOP_DOWN") {
    SUP_ABAJO = angulo;
    servoSuperior.write(angulo);
    posicionSuperior = angulo;
  }
  else if (comando == "TOP_UP") {
    SUP_ARRIBA = angulo;
    servoSuperior.write(angulo);
    posicionSuperior = angulo;
  }
  else if (comando == "TOP_FLIP") {
    SUP_VOLTEO = angulo;
    servoSuperior.write(angulo);
    posicionSuperior = angulo;
  }
  else {
    enviarJSON(400, "{\"ok\":false,\"error\":\"comando_manual_desconocido\"}");
    return;
  }

  delay(650);

  String respuesta = "{\"ok\":true,\"estado\":\"completed\",\"angulo\":" + String(angulo) + "}";
  enviarJSON(200, respuesta);
}

void handleDetener() {
  estadoRobot = "DETENIDO";
  servoSuperior.write(SUP_ARRIBA);
  posicionSuperior = SUP_ARRIBA;

  Serial.println("SISTEMA DETENIDO");
  enviarJSON(200, "{\"ok\":true,\"estado\":\"DETENIDO\"}");
}

// ======================================================
// SETUP / LOOP
// ======================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  servoInferior.setPeriodHertz(50);
  servoSuperior.setPeriodHertz(50);

  servoInferior.attach(PIN_SERVO_INFERIOR, 500, 2400);
  servoSuperior.attach(PIN_SERVO_SUPERIOR, 500, 2400);

  servoInferior.write(INF_CENTRO);
  servoSuperior.write(SUP_ARRIBA);

  posicionInferior = INF_CENTRO;
  posicionSuperior = SUP_ARRIBA;

  delay(700);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();
  Serial.println("============================");
  Serial.println("SCRAMBLERBOT HTTP - D + F");
  Serial.println("============================");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleInicio);
  server.on("/ping", HTTP_GET, handlePing);
  server.on("/estado", HTTP_GET, handleEstado);
  server.on("/mezclar", HTTP_POST, handleMezclar);
  server.on("/manual", HTTP_POST, handleManual);
  server.on("/servo", HTTP_POST, handleManual);
  server.on("/mover", HTTP_POST, handleManual);
  server.on("/detener", HTTP_POST, handleDetener);

  server.begin();

  Serial.println("Servidor HTTP iniciado");
  Serial.println("Activos: D D' D2 F F' F2");

  mostrarOrientacion();
}

void loop() {
  server.handleClient();
}