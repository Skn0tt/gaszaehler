# Gaszähler

ESP32-C6-based gas meter data collection firmware, built with [ESP-IDF](https://github.com/espressif/esp-idf) v6.0.

## Development

```bash
# Install system dependencies (macOS)
brew install libgcrypt glib pixman sdl2 libslirp dfu-util cmake python

# Install Espressif Installation Manager
brew tap espressif/eim
brew install eim

# Install ESP-IDF
eim install
```

For other operating systems or installation methods, see the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/index.html).


```bash
# Useful scripts:
source ~/.espressif/tools/activate_idf_v6.0.sh # run in every new terminal session
idf.py build
idf.py -p PORT flash # Replace `PORT` with your serial port (e.g. `/dev/cu.usbserial-X` on macOS)
idf.py -p PORT monitor # monitor serial output
idf.py -p PORT flash monitor # build, flash, and monitor in one step

idf.py menuconfig # Configure project settings (Wi-Fi, peripherals, etc.)
```

### Project structure

```
├── CMakeLists.txt             Root build configuration
├── main/
│   ├── CMakeLists.txt         Main component build configuration
│   └── gaszaehler_main.c      Application entry point
├── sdkconfig                  Project configuration (generated, not committed)
└── README.md
```
