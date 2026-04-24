// XIAO ESP32S3 Sense — MJPEG camera streaming over HTTP.
// Build with arduino-cli (board: esp32:esp32:XIAO_ESP32S3). PSRAM must be
// enabled for the JPEG frame buffer.

#include "src/wifi_conn.h"
#include "src/camera_stream.h"

void setup() {
  Serial.begin(115200);
  delay(100);
  wifiConnect();
  cameraInit();
  cameraStreamStart();
}

void loop() {
  delay(1000);
}
