# esp32-xiao-cam-stream

MJPEG camera streaming firmware for the Seeed **XIAO ESP32S3 Sense**. After
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

Confirm the XIAO ESP32S3 board is visible:
```sh
arduino-cli board listall | grep XIAO_ESP32S3
```

No extra libraries are required — `WiFi`, `esp_camera`, and `esp_http_server`
all ship with the ESP32 core.

## Build & flash

From this directory (`esp32-xiao-cam-stream/`):

```sh
make build   # arduino-cli compile with PSRAM=opi
make flash   # upload to the first /dev/cu.usbmodem* and open a 115200 serial monitor
```

Or call `arduino-cli` directly:
```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi .
arduino-cli upload  --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi -p /dev/cu.usbmodem* .
```

PSRAM **must** be enabled (`PSRAM=opi`) — the camera config allocates two
JPEG frame buffers in PSRAM.

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
src/camera_pins.h           # XIAO ESP32S3 Sense camera pinout
src/wifi_conn.h             # scan + connect
src/camera_stream.h         # esp_camera_init + MJPEG HTTP server
Makefile                    # build / flash / serial wrappers
```
