# OpenCatESP32 — OpenCat Framework on ESP32/BiBoard

OpenCatESP32 runs the OpenCat quadruped robotics framework on [BiBoard](https://www.petoi.com/products/biboard-esp32-development-board-for-quadruped-robot?utm_source=github&utm_medium=code&utm_campaign=github-opencat) — an ESP32-based development board designed for multi-degree-of-freedom legged robots with up to 12 servos. Developed by [Petoi](https://www.petoi.com?utm_source=github&utm_medium=code&utm_campaign=github-opencat), the maker of futuristic programmable robotic pets.

This is the codebase for current-generation Petoi hardware. If you're on the older NyBoard (ATmega328P), see the main [OpenCat repo](https://github.com/PetoiCamp/OpenCat).


[![BittleESP32](https://github.com/PetoiCamp/NonCodeFiles/blob/master/gif/BiBoard.gif)](https://www.youtube.com/watch?v=GTgps_H990w)

[![BittleGap](https://github.com/PetoiCamp/NonCodeFiles/blob/master/gif/gap.gif)](https://youtu.be/1qhNRSQTcG4)

*Click either GIF to watch the demo.*

---

## HTTP Control API

This fork adds an HTTP control layer (port 80) alongside the existing WebSocket (:81) server. Once the board connects to WiFi (see [shared WiFi config](../README.md#shared-wifi-configuration)), you can drive the robot with plain `curl`.

### Quickstart

```sh
curl http://<robot-ip>/status                                   # board info
curl http://<robot-ip>/skills                                   # live skill list
curl -X POST http://<robot-ip>/up                               # stand up
curl -X POST http://<robot-ip>/skill -d '{"name":"sit"}'        # sit
curl -X POST http://<robot-ip>/skill -d '{"name":"wkF"}'        # walk forward
curl -X POST http://<robot-ip>/rest                             # power down
```

### Endpoints

| Method | Path | Body | What it does |
|---|---|---|---|
| `GET`  | `/status`  | — | `{model, ip, rssi, uptimeMs, busy, wsClients, freeHeap}` |
| `GET`  | `/skills`  | — | Live `{postures:[], gaits:[], behaviors:[]}` read from firmware |
| `POST` | `/skill`   | `{"name":"sit","arg":0}` | Run a named skill (`arg` optional, e.g. repeat count) |
| `POST` | `/cmd`     | `{"raw":"ksit"}` or `{"token":"t","args":"10 -5"}` | Raw passthrough for any OpenCat token |
| `POST` | `/move`    | `{"joints":[{"index":8,"angle":30}], "mode":"simultaneous"\|"sequential"}` | Move specific joints |
| `POST` | `/tilt`    | `{"roll":10,"pitch":-5}` | Tilt the body |
| `POST` | `/beep`    | `{"notes":[[60,4],[62,4]]}` | Play notes (MIDI number + duration) |
| `POST` | `/gyro`    | `{}` | Toggle IMU-based balancing |
| `POST` | `/speed`   | `{"delta":1}` or `{"delta":-2}` | Speed up / slow down |
| `POST` | `/stop`, `/rest`, `/balance`, `/up`, `/zero` | — | Convenience wrappers → `/skill` |

All POSTs return `{ok:true, token, cmd, durationMs, response}` on success, or `{ok:false, error:"busy"|"timeout"|"bad_request"}` with HTTP 4xx.

### Skill library (Bittle, ~93 skills)

Source of truth is `GET /skills` — it reads the firmware's `skillNameWithType[]` table at runtime, so if you rebuild for Nybble / Cub the list changes accordingly. The tables below are the Bittle set for quick reference.

**Postures** — static poses (`period = 1`)

| Name | What it does | Name | What it does |
|---|---|---|---|
| `balance`   | Neutral standing posture | `rest`    | Power servos down |
| `up`        | Stand up from rest       | `sit`     | Sit on haunches |
| `buttUp`    | Rear end up              | `str`     | Stretch |
| `calib`     | Calibration pose         | `zero`    | All joints to zero |
| `lifted`    | Pose when picked up      | `dropped` | Pose after drop |
| `lnd`       | Landing posture          |           |   |

**Gaits** — locomotion cycles (`period > 1`)

| Family | Forward | Left turn | Arm-variant F | Arm-variant L |
|---|---|---|---|---|
| Walk              | `wkF`     | `wkL`     | `wkArmF`  | `wkArmL`  |
| Trot              | `trF`     | `trL`     | `trArmF`  | `trArmL`  |
| Vertical trot     | `vtF`     | `vtL`     | `vtArmF`  | —         |
| Crawl             | `crF`     | `crL`     | `crArmF`  | `crArmL`  |
| Gallop            | `gpF`     | `gpL`     | —         | —         |
| Back-step / dash  | `bkF` `bk` `bdF` | `bkL` | `bkArmF`  | `bkArmL`  |
| High walk         | `hlw`     | —         | —         | —         |
| Jump gait         | `jpF`     | —         | —         | —         |
| Carpet-optimized  | `carpetF` | `carpetL` | —         | —         |
| Push              | `phF`     | `phL`     | —         | —         |
| Lift              | `lftF`    | `lftL`    | —         | —         |

**Behaviors** — one-shot expressions (`period < 0`)

| Group | Skills |
|---|---|
| Head / greeting | `nd` (nod), `hds` (head shake), `hg` (hug), `hi`, `hsk` (handshake), `fiv` (high five), `clap`, `ck` (check), `cmh` (come here), `gdb` (goodbye) |
| Emotes          | `ang` (angry), `hu` (hurt), `wh` (whine), `lucky`, `showOff`, `lpov`, `mw`, `snf` (sniff), `zz` (sleep), `chr` (chirp), `dg` (dog) |
| Acrobatic       | `flip`, `flipD`, `flipF`, `jmp` (jump), `launch`, `rl` (roll), `rc`, `bf`, `bx` (box), `kc` (kick), `pu` / `pu1` (pushup), `scrh` (scratch), `tbl` (tumble), `ts` (taunt shake), `ff` (fist fight) |
| Interaction     | `pick`, `pickD`, `pickF`, `put`, `putD`, `putF`, `toss`, `tossD`, `tossF`, `hunt`, `knock`, `pd`, `pee`, `dropRec` |

### Caveats

- **One command at a time.** HTTP and the built-in WebSocket share a single dispatch slot. A second request during motion returns `409 busy` — wait and retry.
- **Blocking semantics.** POST `/skill` doesn't return until the skill finishes (or 30 s timeout). Fine for scripting; don't open dozens of parallel connections.
- **Model-specific.** Names above are Bittle. Swap `#define BITTLE` for `NYBBLE` / `CUB` in `OpenCatESP32.ino` and rebuild — `GET /skills` will reflect the new set.

---

## Hardware

BiBoard is the control board for:

- 🐶 [Bittle X — robot dog with voice control](https://www.petoi.com/products/petoi-robot-dog-bittle-x-voice-controlled?utm_source=github&utm_medium=code&utm_campaign=github-opencat)
- 🐱 [Nybble Q — robot cat](https://www.petoi.com/products/petoi-nybble-q-robot-cat?utm_source=github&utm_medium=code&utm_campaign=github-opencat)

The older [Bittle](https://www.petoi.com/collections/robots/products/petoi-bittle-robot-dog?utm_source=github&utm_medium=code&utm_campaign=github-opencat) and [Nybble](https://www.petoi.com/collections/robots/products/petoi-nybble-robot-cat?utm_source=github&utm_medium=code&utm_campaign=github-opencat) (NyBoard) are discontinued. BiBoard is the current platform.

---

## Why BiBoard Over NyBoard?

The ATmega328P (NyBoard) gets the job done for locomotion. BiBoard is for when your **robotics programming** needs more headroom:

- **ESP32 dual-core @ 240 MHz** — handle real-time servo coordination and a perception pipeline simultaneously
- **Wi-Fi + Bluetooth built in** — wireless control and data streaming without a dongle
- **Up to 12 servos** — full 12-DOF configurations
- **Arduino IDE compatible** — same workflow, more horsepower
- **Open source** — hardware and software both forkable

If you're building an open source robot dog for research, running ROS, deploying a vision model, or just want room to experiment — BiBoard is the right foundation.

---

## Board Configuration

**Arduino IDE settings (ESP32 Dev Module):**

| Setting | Value |
|---|---|
| Upload Speed | 921600 |
| CPU Frequency | 240 MHz (WiFi/BT) |
| Flash Frequency | 80 MHz |
| Flash Mode | QIO |
| Flash Size | 4 MB |
| Partition Scheme | Default 4MB with spiffs |
| Core Debug Level | None |
| PSRAM | Disabled |

Full setup: [Upload Sketch for BiBoard](https://docs.petoi.com/arduino-ide/upload-sketch-for-biboard)

---

## What People Have Built

- [AI and computer vision applications](https://www.petoi.com/blogs/blog/tagged/showcase+ai?utm_source=github&utm_medium=code&utm_campaign=github-opencat)
- [Raspberry Pi robotics projects](https://www.petoi.com/blogs/blog/tagged/raspberry-pi?utm_source=github&utm_medium=code&utm_campaign=github-opencat)
- [NVIDIA Isaac simulations and reinforcement learning](https://www.youtube.com/playlist?list=PLHMFXft_rV6MWNGyofDzRhpatxZuUZMdg)
- [SLAM with ROS using Bittle and Raspberry Pi](https://www.youtube.com/watch?v=uXpQUIF_Jyk&list=PLHMFXft_rV6MWNGyofDzRhpatxZuUZMdg&index=6)

Academic and research use: [Research Spotlight](https://www.petoi.com/pages/research-spotlight?utm_source=github&utm_medium=code&utm_campaign=github-opencat)

---

## Community & Discussion

- [r/OpenCat](https://www.reddit.com/r/OpenCat/) — firmware code, framework hacking, extending and porting OpenCat
- [r/Petoi](https://www.reddit.com/r/Petoi/) — hardware Q&A, builds, quadruped coding, curriculum, 3D-printed parts, general discussion

---

## Resources

- [Petoi Doc Center](https://docs.petoi.com)
- [User showcases](https://www.petoi.com/pages/petoi-open-source-extensions-user-demos-and-hacks?utm_source=github&utm_medium=code&utm_campaign=github-opencat)
- [Advanced tutorials by the community](https://www.youtube.com/playlist?list=PLHMFXft_rV6MWNGyofDzRhpatxZuUZMdg)
- [All kits and accessories](https://www.petoi.com/store?utm_source=github&utm_medium=code&utm_campaign=github-opencat)
- [FAQ](https://www.petoi.com/pages/faq?utm_source=github&utm_medium=code&utm_campaign=github-opencat)

Follow the project: [YouTube](https://www.youtube.com/@petoicamp) · [Twitter](https://twitter.com/petoicamp) · [Instagram](https://www.instagram.com/petoicamp/) · [Facebook](https://www.facebook.com/PetoiCamp/) · [LinkedIn](https://www.linkedin.com/company/33449768/admin/dashboard/)
