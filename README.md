# typego-petoi-firmware

Two ESP32 Arduino sketches that share a single WiFi credentials file:

- **`OpenCatESP32/`** — Petoi BiBoard firmware (OpenCat framework) with an added HTTP control server. See [`OpenCatESP32/README.md`](OpenCatESP32/README.md).
- **`esp32-xiao-cam-stream/`** — AI-Thinker ESP32-CAM MJPEG camera streamer. See [`esp32-xiao-cam-stream/README.md`](esp32-xiao-cam-stream/README.md).

Both sketches build with `arduino-cli` — run `make build` (or `make flash`) inside either subdirectory.

## Shared WiFi configuration

Both sketches scan for a known AP at boot and connect to the first match. The credentials list is a single file at the repo root so you only edit it in one place:

```
shared/wifi_networks.h          ← single source of truth — edit this
```

Each sketch picks it up via a symlink inside its own `src/`:

```
OpenCatESP32/src/wifi_networks_shared.h         →  ../../shared/wifi_networks.h
esp32-xiao-cam-stream/src/wifi_networks_shared.h →  ../../shared/wifi_networks.h
```

The symlink is needed because `arduino-cli` copies each sketch's `src/` into `build/sketch/src/` before compiling — direct `../../shared/…` includes don't resolve from the build copy, but symlinks are dereferenced during the copy so the real file lands next to the sketch.

### Editing credentials

Open `shared/wifi_networks.h` and add/edit entries (preference = declaration order; the first SSID found in a scan wins):

```c
static const wifi_network_t WIFI_NETWORKS[] = {
    {"YourSSID",      "yourpassword"},
    {"BackupNetwork", "otherpassword"},
};
```

Rebuild both sketches to pick up the change.

### Caveats

- **Windows** — symlinks require developer mode (or `git config core.symlinks true` on clone). If you build on Windows and don't want to enable symlinks, replace each symlink with a `cp` step in the sketch's `Makefile`.
- **Credentials in git** — `shared/wifi_networks.h` holds plaintext passwords. Consider `echo 'shared/wifi_networks.h' >> .gitignore` and committing a `shared/wifi_networks.example.h` instead.
