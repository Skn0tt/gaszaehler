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
│   ├── gaszaehler_main.cpp    Application entry point
│   ├── commodity_metering.h   Local CommodityMetering cluster helper (stopgap)
│   └── commodity_metering.cpp
├── partitions.csv             Custom partition table
├── sdkconfig.defaults         Default build configuration
├── sdkconfig                  Project configuration (generated, not committed)
├── matter-specs.md            Matter specification references (not committed, acquire links from https://csa-iot.org/developer-resource/specifications-download-request/)
└── README.md
```

## TODO

- **Upstream CommodityMetering support**: The gas counter uses the standard Matter [Commodity Metering](https://github.com/project-chip/connectedhomeip/issues/40140) cluster (0x0B07) via a local helper (`commodity_metering.h/cpp`). Once esp-matter ships a built-in `cluster::commodity_metering` wrapper, remove the local files and switch to the upstream API.
- **MeasurementTypeEnum for gas**: The `MeasurementTypeEnum` in the Matter spec currently only has electrical values — no gas/water/thermal. We set `kUnspecified` (0x00) for now. Update once the spec adds a proper commodity type.
- **Report CHIP bug: `GetClock_RealTime` gated by SNTP config**: In `connectedhomeip/src/platform/ESP32/SystemTimeSupport.cpp`, `SetClock_RealTime()` (used by `SetUTCTime`) unconditionally calls `settimeofday()`, but `GetClock_RealTime()` is gated behind `#ifdef CONFIG_ENABLE_SNTP_TIME_SYNC`. This means the Time Synchronization cluster can *set* time via `SetUTCTime` but can't *read it back* (UTCTime attribute returns null) unless SNTP is enabled — even though SNTP has nothing to do with it. Workaround: enable `CONFIG_ENABLE_SNTP_TIME_SYNC` in sdkconfig. Report upstream at [connectedhomeip](https://github.com/project-chip/connectedhomeip/issues).
