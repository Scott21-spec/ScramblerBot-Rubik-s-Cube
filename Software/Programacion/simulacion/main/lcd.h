#ifndef LCD_H
#define LCD_H
#include <stdint.h>
#include "driver/gpio.h"

// Macros publicas de la pantalla lcd
#define SDA_PIN     GPIO_NUM_1
#define SCL_PIN     GPIO_NUM_2
#define I2C_PORT    I2C_NUM_0
#define LCD_ADDR    0x20

// Funciones publicas a utilizar en el main.cpp
void lcd_init(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_print(const char *str);
void lcd_clear(void);

#endif