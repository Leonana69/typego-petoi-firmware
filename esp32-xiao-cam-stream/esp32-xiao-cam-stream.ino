// AI-Thinker ESP32-CAM — MJPEG camera streaming over HTTP.
// Build with arduino-cli (board: esp32:esp32:esp32cam). PSRAM is enabled by
// the board definition and holds the JPEG frame buffer.

#include "src/wifi_conn.h"
#include "src/camera_stream.h"

void setup() {
  Serial.begin(115200);
  // Give the serial monitor a moment to attach after the upload/reset before
  // we print.
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
  // Print the WiFi signal strength at 0.5 Hz (every 2 s).
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("RSSI: not connected");
  }
  delay(2000);
}
