#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "driver/gpio.h"

// Macros publicas del Servomotor
#define SERVO_1_PIN     GPIO_NUM_7
#define SERVO_2_PIN     GPIO_NUM_8
#define POS_INICIAL     0
#define POS_MEDIA       90
#define TIEMPO_MOV_MS   500  // Tiempo para que el servo llegue a su posicion

// Variable que puede usarse en el main.cpp
extern int contador_movimientos;

// Funciones publicas
void servo_init(void);
void mover_servos_secuencia(void);

#endif