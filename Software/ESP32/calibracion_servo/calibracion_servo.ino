#include <ESP32Servo.h>

// PINES SERVO
const int PIN_SERVO_BASE = 7;
const int PIN_SERVO_TAPA = 6;

Servo servoInferior;
Servo servoSuperior;

bool turno = true;

void setup() {
  servoInferior.attach(PIN_SERVO_BASE, 500, 2400);
  servoSuperior.attach(PIN_SERVO_TAPA, 500, 2400);
}

void loop() {
  if(turno){
    servoInferior.write(80);
    delay(1000);
    servoSuperior.write(164);
    delay(1000);
    servoInferior.write(80);
    delay(1000);
    servoInferior.write(0);
    delay(1000);
    servoInferior.write(80);
    delay(1000);
    servoInferior.write(180);
    delay(1000);
    servoInferior.write(80);
    delay(1000);
    servoSuperior.write(135);
    delay(1000);
    servoSuperior.write(75);
    delay(500);
    servoSuperior.write(135);
    
    turno = false;
  }
}

