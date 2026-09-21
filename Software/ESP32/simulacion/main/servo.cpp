#include "servo.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Macros internas de configuracion
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CANAL_S1   LEDC_CHANNEL_0
#define LEDC_CANAL_S2   LEDC_CHANNEL_1
#define LEDC_RESOLUCION LEDC_TIMER_14_BIT
#define LEDC_FRECUENCIA 50
#define PULSO_MIN       500
#define PULSO_MAX       2400
#define GRADOS_MAX      180

// Definicion de la variable extern
int contador_movimientos = 0;

// Convierte los angulos en Duty Cycles de 14 bits de resolucion
static uint32_t angle_to_duty(int angle) {
    uint32_t pulsewidth = PULSO_MIN + ((PULSO_MAX - PULSO_MIN) * angle) / GRADOS_MAX;
    return (pulsewidth * 16384) / 20000;
}

// Movemos el servo y esperamos
static void mover_servo(ledc_channel_t canal, int grados) {
    ledc_set_duty(LEDC_MODE, canal, angle_to_duty(grados));
    ledc_update_duty(LEDC_MODE, canal);
    vTaskDelay(TIEMPO_MOV_MS / portTICK_PERIOD_MS);
}

// Inicializamos el servo
void servo_init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_RESOLUCION,
        .timer_num = LEDC_TIMER, 
        .freq_hz = LEDC_FRECUENCIA, 
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure      = false
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch1_conf = {
        .gpio_num = SERVO_1_PIN, 
        .speed_mode = LEDC_MODE, 
        .channel = LEDC_CANAL_S1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER, 
        .duty = angle_to_duty(POS_INICIAL),
        .hpoint = 0,
        .sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags          = { .output_invert = 0 },        
        .deconfigure    = false
    };
    ledc_channel_config(&ch1_conf);

    ledc_channel_config_t ch2_conf = {
        .gpio_num = SERVO_2_PIN, 
        .speed_mode = LEDC_MODE, 
        .channel = LEDC_CANAL_S2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER, 
        .duty = angle_to_duty(POS_INICIAL),
        .hpoint = 0,
        .sleep_mode     = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags          = { .output_invert = 0 },        
        .deconfigure    = false
    };
    ledc_channel_config(&ch2_conf);
}

void mover_servos_secuencia(void) {
    // Se mueve el servo 1 a 90 grados
    mover_servo(LEDC_CANAL_S1, POS_MEDIA);
    // Se mueve el servo 2 a 90 grados
    mover_servo(LEDC_CANAL_S2, POS_MEDIA);
    // Vuelve a posicion original el servo 2
    mover_servo(LEDC_CANAL_S2, POS_INICIAL);
    // Vuelve a posicion original el servo 1
    mover_servo(LEDC_CANAL_S1, POS_INICIAL);
    
    contador_movimientos += 4;
}