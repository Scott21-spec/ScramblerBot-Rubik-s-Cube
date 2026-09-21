#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd.h"
#include "servo.h"

// Macros de tiempos 
#define TIEMPO_SALUDO_MS      5000   // 5 segundos
#define TIEMPO_RESOLVIENDO_MS 10000  // 10 segundos
#define TIEMPO_PAUSA_MS       2000   // 2 segundos (¡Faltaba este!)

// El 'extern "C"' para que reconozca la funcion principal
extern "C" void app_main(void) 
{
    // Inicializacion del hardware
    lcd_init();
    servo_init();

    // Mensaje inicial
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Hola soy ");
    lcd_set_cursor(0, 1);
    lcd_print("ScramblerBot!!");
    vTaskDelay(TIEMPO_SALUDO_MS / portTICK_PERIOD_MS);

    // Imprimimos la base del mensaje en pantalla 
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Resolviendo...");
    lcd_set_cursor(0, 1);
    lcd_print("Movimientos: ");

    // Tomamos la marca de tiempo actual en milisegundos
    TickType_t tiempo_inicio = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Mientras no hayan pasado 10 segundos, repetimos la secuencia
    while (((xTaskGetTickCount() * portTICK_PERIOD_MS) - tiempo_inicio) < TIEMPO_RESOLVIENDO_MS) {
        
        // Convertimos el numero entero a texto para que la LCD lo entienda
        char buffer_texto[16]; // Creamos un espacio temporal en memoria
        snprintf(buffer_texto, sizeof(buffer_texto), "%d", contador_movimientos);
        
        // Imprimimos solo el numero actualizado en la posicion correcta
        lcd_set_cursor(13, 1);
        lcd_print(buffer_texto);
        
        mover_servos_secuencia();
    }

    // Pausa
    vTaskDelay(TIEMPO_PAUSA_MS / portTICK_PERIOD_MS);

    // Mensaje final
    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_print("Cubo Resuelto!!");
}