#ifndef PETOI_WIFI_NETWORKS_H
#define PETOI_WIFI_NETWORKS_H

// Network list lives in the repo-root shared/ directory so both subprojects
// (OpenCatESP32 and esp32-xiao-cam-stream) stay in sync. Edit credentials
// there, not here. This include resolves through the src/wifi_networks_shared.h
// symlink — arduino-cli dereferences it when copying the sketch into build/.
#include "wifi_networks_shared.h"

// Aliases so existing code in webServer.h keeps compiling unchanged.
typedef wifi_network_t WifiCredential;
#define KNOWN_NETWORKS     WIFI_NETWORKS
#define NUM_KNOWN_NETWORKS NUM_WIFI_NETWORKS

#endif  // PETOI_WIFI_NETWORKS_H
