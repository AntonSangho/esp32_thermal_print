idf_component_register(SRCS "usb_printer_image_main.c"
                    PRIV_REQUIRES usb esp_driver_gpio
                    INCLUDE_DIRS "."
                    EMBED_FILES "logo.pbm")
