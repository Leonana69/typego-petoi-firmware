// XIAO ESP32S3 Sense — MJPEG camera streaming over HTTP.
// Build with arduino-cli (board: esp32:esp32:XIAO_ESP32S3). PSRAM must be
// enabled for the JPEG frame buffer.

#include "src/wifi_conn.h"
#include "src/camera_stream.h"

void setup() {
  Serial.begin(115200);
  // HWCDC blocks Serial.print waiting for the host to drain — drop the wait
  // so output never stalls if the monitor isn't attached yet.
  Serial.setTxTimeoutMs(0);
  // Give the host time to reattach after the upload/reset before we print.
  delay(2000);

  Serial.println();
  Serial.println("=== esp32-xiao-cam-stream boot ===");
  Serial.printf("PSRAM size: %u bytes\n", ESP.getPsramSize());
  Serial.printf("Free heap:  %u bytes\n", ESP.getFreeHeap());
  Serial.flush();

  wifiConnect();
  cameraInit();
  cameraStreamStart();
}

void loop() {
  delay(1000);
}
