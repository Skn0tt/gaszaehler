# Gaszähler

Battery-powered gas meter pulse counter that reports consumption via **Matter** over **Thread**,
with **1 year battery life** on a 2000 mAh battery.

Parts:

- DFrobot Firebeetle ESP32-C6
- Reed switch, e.g. KY-021
- lithium battery with JST connector

## Development

Built with [ESP-IDF](https://github.com/espressif/esp-idf) v5.4.1 and [ESP-Matter](https://github.com/espressif/esp-matter).

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
source ~/.espressif/tools/activate_idf_v5.4.1.sh # run in every new terminal session
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
│   ├── idf_component.yml      Managed component dependencies (esp-matter, etc.)
│   └── gaszaehler_main.cpp     Application entry point
├── partitions.csv             Custom partition table
├── sdkconfig.defaults         Default build configuration
├── sdkconfig                  Project configuration (generated, not committed)
├── matter-specs.md            Matter specification references (not committed, acquire links from https://csa-iot.org/developer-resource/specifications-download-request/)
└── README.md
```

## TODO

- **Migrate to Commodity Metering cluster**: The gas counter currently uses a custom vendor-specific cluster (`0xFFF10000`). The Matter spec (1.5+) introduced a standard [Commodity Metering](https://github.com/project-chip/connectedhomeip/issues/40140) cluster that covers gas/water/energy metering. Once esp-matter picks up connectedhomeip support for it, replace the custom cluster with the standard one for proper controller interoperability.
