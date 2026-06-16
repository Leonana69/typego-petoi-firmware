# esp32-xiao-cam-stream

MJPEG camera streaming firmware for the **AI-Thinker ESP32-CAM**. After
boot, the board connects to a known WiFi AP and serves an MJPEG stream at
`http://<board-ip>/`.

## Prerequisites

### 1. Install `arduino-cli`

**macOS (Homebrew):**
```sh
brew install arduino-cli
```

**Linux / macOS (official install script):**
```sh
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
# binary lands in ./bin/arduino-cli — move it somewhere on your PATH
```

**Windows:** download the zip from
<https://arduino.github.io/arduino-cli/latest/installation/> and add it to
`PATH`.

Verify:
```sh
arduino-cli version
```

### 2. Install the ESP32 core

Add Espressif's package index, update, and install the `esp32:esp32` core:
```sh
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Confirm the ESP32-CAM board is visible:
```sh
arduino-cli board listall | grep esp32cam
```

No extra libraries are required — `WiFi`, `esp_camera`, and `esp_http_server`
all ship with the ESP32 core.

## Build & flash

The ESP32-CAM has no onboard USB. Either seat it on an **ESP32-CAM-MB**
programmer (auto-reset, no jumpers) or wire a 3.3 V USB-to-serial adapter to
`U0T`/`U0R`/`GND` and pull **IO0 to GND** during reset to enter the bootloader
(release IO0 and reset to run).

From this directory (`esp32-xiao-cam-stream/`):

```sh
make build   # arduino-cli compile for esp32:esp32:esp32cam
make flash   # upload over the USB-serial adapter and open a 115200 serial monitor
```

Or call `arduino-cli` directly:
```sh
arduino-cli compile --fqbn esp32:esp32:esp32cam .
arduino-cli upload  --fqbn esp32:esp32:esp32cam -p /dev/cu.usbserial-XXXX .
```

PSRAM is enabled by the `esp32cam` board definition — the camera config
allocates two JPEG frame buffers in PSRAM.

## Configure WiFi

Credentials live in [`../shared/wifi_networks.h`](../shared/wifi_networks.h) at
the repo root — shared with the `OpenCatESP32/` sketch so you only edit them in
one place. `src/wifi_networks_shared.h` is a symlink that pulls the file in at
build time. See the [root README](../README.md#shared-wifi-configuration) for
the full mechanism.

At boot the board scans for any of the listed SSIDs and connects to the first
match (preference = declaration order).

## Layout

```
esp32-xiao-cam-stream.ino   # setup() / loop()
src/camera_pins.h           # AI-Thinker ESP32-CAM camera pinout
src/wifi_conn.h             # scan + connect
src/camera_stream.h         # esp_camera_init + MJPEG HTTP server
Makefile                    # build / flash / serial wrappers
```
