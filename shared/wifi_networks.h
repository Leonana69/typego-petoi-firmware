#ifndef SHARED_WIFI_NETWORKS_H
#define SHARED_WIFI_NETWORKS_H

// Single source of truth for WiFi credentials, shared by both subprojects:
//   - OpenCatESP32           (Arduino, included via src/wifiNetworks.h)
//   - esp32-xiao-cam-stream  (ESP-IDF, included via src/wifi.c)
//
// Connection preference = declaration order. Each site scans nearby APs and
// picks the first entry whose SSID is in range.
//
// Consider adding this file to .gitignore if you don't want credentials in VCS.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *ssid;
    const char *password;
} wifi_network_t;

static const wifi_network_t WIFI_NETWORKS[] = {
    {"YourSSID", "YourPassword"},
};

#define NUM_WIFI_NETWORKS (sizeof(WIFI_NETWORKS) / sizeof(WIFI_NETWORKS[0]))

#ifdef __cplusplus
}
#endif

#endif  // SHARED_WIFI_NETWORKS_H
