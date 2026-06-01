// Incluimos las librerias principales
# include <stdio.h>
# include "freertos/FreeRTOS.h"
# include "freertos/task.h"
# include "driver/gpio.h"

# define BLINK_GPIO 38 // LED de la placa -> GPIO 38

static uint8_t estado = 0; // Estado actual del led

void app_main ( void )
{
    // Resetear y configurar el GPIO 38 como salida
    gpio_reset_pin ( BLINK_GPIO ) ;
    gpio_set_direction ( BLINK_GPIO , GPIO_MODE_OUTPUT ) ;

    while (1) {
        gpio_set_level ( BLINK_GPIO , estado ) ;
        estado = ! estado ; // Cambia entre 0 y 1
        // Delay de 500 ms usando los ticks del RTOS
        vTaskDelay (500 / portTICK_PERIOD_MS );
    }
}