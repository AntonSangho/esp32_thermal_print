# ESP32-P4-Nano Thermal Printer Control

**MVP (Minimum Viable Product)**: Thermal printer control via USB, with text and image printing capabilities.

## Overview

This project implements thermal receipt printer support on the Waveshare ESP32-P4-Nano board using ESP-IDF v5.4. The board connects to a Bixolon SRP-330II thermal printer via USB and supports both text and bitmap image printing using ESC/POS commands.

**Board**: Waveshare ESP32-P4-Nano (ESP32-P4 RISC-V dual-core, 16MB flash)  
**Printer**: Bixolon SRP-330II (VID=0x1504, PID=0x006e, USB CDC ACM class)  
**Interface**: USB OTG (UTMI HS port on second USB connector)

---

## MVP Features

### Phase 1: GPIO LED Control ✅
- **Status**: Complete
- **Source**: `main/led_blink_main.c`
- **Test**: GPIO 20 LED blink at 500ms interval
- **Purpose**: Verify build system and basic ESP-IDF functionality

### Phase 2: USB CDC Text Printing ✅
- **Status**: Complete  
- **Source**: `main/usb_printer_main.c`
- **Features**:
  - Auto-detect Bixolon SRP-330II via VID:PID
  - ESC/POS text commands:
    - `0x1B 0x40` - Printer initialization
    - `0x1B 0x61 0x01` - Center align
    - `0x0A` - Line feed
    - `0x1D 0x56 0x41 0x00` - Paper auto-cut
  - Successfully prints test messages
- **Verified Output**: 
  - ✅ "Hello ESP32!"
  - ✅ "USB Printer Test"

### Phase 3: Bitmap Image Printing ✅
- **Status**: Complete
- **Source**: `main/usb_printer_image_main.c`
- **Features**:
  - PBM (Portable Bitmap) image embedding: `main/logo.pbm`
  - Image dimensions: 384×799 pixels (58mm paper width @ 203 DPI)
  - P4 (raw 1-bit) PBM header parsing
  - ESC/POS raster image command: `0x1D 0x76 0x30` (GS v 0)
  - Automatic paper feed (3 lines) and cut after image
- **Image Conversion**:
  ```bash
  convert input.png -resize 384x -threshold 50% -depth 1 logo.pbm
  ```

---

## Hardware Requirements

- **ESP32-P4-Nano** evaluation board (Waveshare)
- **Bixolon SRP-330II** thermal receipt printer (or compatible CDC ACM thermal printer)
- USB-A to micro-USB/USB-C cable for printer connection
- External LED + 330Ω resistor (GPIO 20 to GND) for Phase 1 test

### Printer Compatibility

Any thermal printer supporting:
- USB CDC ACM class (virtual serial port)
- ESC/POS command set

To find your printer's VID:PID:
```bash
lsusb | grep -i printer
# Update CONFIG_PRINTER_VID and CONFIG_PRINTER_PID in Kconfig
```

---

## Build & Flash

### Environment Setup
```bash
export IDF_PATH=~/projects/esp-idf-v5.4
. $IDF_PATH/export.sh
```

### Build
```bash
idf.py build
```

### Flash
```bash
idf.py -p /dev/ttyACM0 flash
```

### Monitor (Optional - not required since printer output confirms success)
```bash
stty -F /dev/ttyACM0 115200 raw -echo && timeout 10 cat /dev/ttyACM0
```

---

## Configuration

Edit `main/Kconfig.projbuild` to customize:

- **Printer VID/PID**: Default Bixolon (0x1504/0x006e)
- **Printer width**: 384 pixels (58mm), or 576 (80mm)

### Switch Between Phases

Edit `main/CMakeLists.txt` to change active source:
```cmake
idf_component_register(SRCS "usb_printer_image_main.c"  # Change this
                    PRIV_REQUIRES usb esp_driver_gpio
                    INCLUDE_DIRS "."
                    EMBED_FILES "logo.pbm")
```

Options:
- `led_blink_main.c` - Phase 1
- `usb_printer_main.c` - Phase 2
- `usb_printer_image_main.c` - Phase 3 (current)

---

## Binary Size

| Phase | Source File | Size | Partition Headroom |
|-------|-------------|------|-------------------|
| 1 | led_blink_main.c | 427 KB | 80% |
| 2 | usb_printer_main.c | 482 KB | 77% |
| 3 | usb_printer_image_main.c | 523 KB | 75% |

All fit comfortably in 2MB factory partition. Phase 4 (WiFi) may require optimization or partition expansion.

---

## Verified Output

### Phase 2 - Text Printing
```
[Printer Output]
        Hello ESP32!
      USB Printer Test
─────────────────────── (cut line)
```

### Phase 3 - Image Printing
```
[Printer Output]
   Image Printing Test
   [384x799 bitmap image]
─────────────────────── (cut line)
```

---

## Architecture

### USB CDC ACM Integration
- **Component**: `espressif/usb_host_cdc_acm:2.*` (managed component)
- **Hardware**: UTMI HS port (`USB_DWC_HS`)
- **Port**: Second USB OTG connector on board (not JTAG port)
- **Features**: Auto device detection, blocking transfers, configurable buffers

### FreeRTOS Task Structure
1. **usb_lib_task** (priority 20): USB host event loop
2. **printer_task** (priority 5): CDC device polling, print job execution

### Embedded Resources
- **logo.pbm**: 38 KB PBM image embedded in firmware via `EMBED_FILES`
- **Device drivers**: esp_driver_gpio, esp_driver_usb_host

---

## Roadmap

### Completed ✅
- [x] Phase 1: GPIO LED control
- [x] Phase 2: USB CDC text printing
- [x] Phase 3: Bitmap image printing

### In Progress 🚧
- [ ] Phase 4: WiFi SoftAP + HTTP web server

### Future Phases
- [ ] Multi-image support (SPIFFS partition)
- [ ] Print job queue and history
- [ ] Mobile app integration
- [ ] Thermal paper width auto-detection
- [ ] Print preview on ESP32-P4 display (if connected)

---

## Known Limitations

1. **Single image per build**: Change `logo.pbm` for different images
2. **No persistent storage**: Print jobs not saved across reboot
3. **SoftAP only** (Phase 4): No existing WiFi AP support
4. **Fixed baud rate**: CDC ACM parameters hardcoded (no runtime config)

---

## Testing Checklist

- [x] Build firmware without errors
- [x] Flash to board successfully
- [x] Printer auto-detected on USB connection
- [x] Text output prints correctly
- [x] Image output renders correctly
- [x] Paper feed and cut commands execute
- [x] Multiple prints work (no lockup)

---

## References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/)
- [ESC/POS Command Set](https://www.epson.com.tw/pos)
- [USB CDC ACM Component](https://components.espressif.com/components/espressif/usb_host_cdc_acm)
- [Bixolon SRP-330II Manual](https://www.bixolon.com/en/product/mobile-printers/srp-330ii)

---

## License

This project uses ESP-IDF and Espressif components under Apache 2.0 license.

---

**Last Updated**: 2026-04-24  
**Version**: MVP (v0.1)
