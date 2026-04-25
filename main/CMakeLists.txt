idf_component_register(SRCS "usb_printer_button_main.c"
                    PRIV_REQUIRES usb esp_driver_gpio
                    INCLUDE_DIRS "."
                    EMBED_FILES "1.pbm")
