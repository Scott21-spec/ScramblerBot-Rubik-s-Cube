#include "lcd.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Enviar 4 bits a la pantalla lcd
static void lcd_send_nibble(uint8_t data, uint8_t rs) {
    uint8_t d = (data & 0xF0) | 0x08 | rs; 
    uint8_t d_en = d | 0x04; 
    i2c_master_write_to_device(I2C_PORT, LCD_ADDR, &d_en, 1, 100);
    vTaskDelay(1 / portTICK_PERIOD_MS); 
    uint8_t d_dis = d & ~0x04;
    i2c_master_write_to_device(I2C_PORT, LCD_ADDR, &d_dis, 1, 100);
    vTaskDelay(1 / portTICK_PERIOD_MS);
}

// Enviar byte entero a la pantalla lcd
static void lcd_send_byte(uint8_t data, uint8_t rs) {
    lcd_send_nibble(data, rs); 
    lcd_send_nibble(data << 4, rs); 
}

// Comando (rs=0) y texto (rs=1)
static void lcd_cmd(uint8_t cmd) {
    lcd_send_byte(cmd, 0); 
}
static void lcd_data(uint8_t data) {
    lcd_send_byte(data, 1); 
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_cmd(0x80 | (col + row_offsets[row]));
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_data((uint8_t)(*str));
        str++;
    }
}

void lcd_clear(void) {
    lcd_cmd(0x01);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

// Funcion de inicializacion
void lcd_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER, 
        .sda_io_num = SDA_PIN, 
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, 
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 100000},
        .clk_flags = 0
    };
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);

    vTaskDelay(50 / portTICK_PERIOD_MS); 
    lcd_send_nibble(0x30, 0); 
    vTaskDelay(5 / portTICK_PERIOD_MS);
    lcd_send_nibble(0x30, 0); 
    vTaskDelay(1 / portTICK_PERIOD_MS);
    lcd_send_nibble(0x30, 0); 
    vTaskDelay(1 / portTICK_PERIOD_MS);
    lcd_send_nibble(0x20, 0); 
    vTaskDelay(1 / portTICK_PERIOD_MS); 
    
    lcd_cmd(0x28); 
    lcd_cmd(0x0C); // Display ON
    lcd_clear();
}