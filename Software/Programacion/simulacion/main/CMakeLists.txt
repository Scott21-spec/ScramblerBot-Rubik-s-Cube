idf_component_register(SRCS "main.cpp" "lcd.cpp" "servo.cpp"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "."
                       REQUIRES driver freertos esp_driver_gpio esp_driver_ledc)