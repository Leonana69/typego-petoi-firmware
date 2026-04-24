#ifndef WIFI_CONN_H
#define WIFI_CONN_H

#include <Arduino.h>
#include <WiFi.h>

// Credentials live in the repo-root shared/ directory so both subprojects
// (esp32-xiao-cam-stream and OpenCatESP32) stay in sync. Edit them there.
// This resolves through the src/wifi_networks_shared.h symlink so arduino-cli
// picks it up when copying the sketch into build/.
#include "wifi_networks_shared.h"

static int wifiScanForKnownAp() {
  int n = WiFi.scanNetworks();
  for (size_t j = 0; j < NUM_WIFI_NETWORKS; j++) {
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == WIFI_NETWORKS[j].ssid) {
        return (int)j;
      }
    }
  }
  return -1;
}

inline void wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  int idx = wifiScanForKnownAp();
  if (idx < 0) {
    Serial.println("No known AP found");
    return;
  }

  Serial.printf("Connecting to %s\n", WIFI_NETWORKS[idx].ssid);
  WiFi.begin(WIFI_NETWORKS[idx].ssid, WIFI_NETWORKS[idx].password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 60) {
    delay(500);
    Serial.print('.');
    retry++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected, IP=");
    Serial.println(WiFi.localIP());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.printf("Failed to connect to %s\n", WIFI_NETWORKS[idx].ssid);
  }
}

#endif
