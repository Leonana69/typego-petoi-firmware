# Compile

Both subprojects build with `arduino-cli`.

- `OpenCatESP32/`
  ```
  arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app OpenCatESP32
  ```
  `huge_app` is required — default partition is 1.25 MB and the firmware is ~1.84 MB.

- `esp32-xiao-cam-stream/`
  ```
  cd esp32-xiao-cam-stream && make build
  ```
  Wraps `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi .`. PSRAM must be enabled. See `esp32-xiao-cam-stream/README.md` for arduino-cli + ESP32 core install steps.
