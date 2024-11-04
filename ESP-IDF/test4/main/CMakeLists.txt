# Register the components
idf_component_register(SRCS "button_manager.c" "button.c" "sniffer.c" "main.c" "i2c_oled.c" "wifi_connect.c" # List your source files here
                       INCLUDE_DIRS "."  # Include directories if needed
)  # Specify required components                                 