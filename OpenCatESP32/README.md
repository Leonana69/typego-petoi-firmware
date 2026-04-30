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
curl http://<robot-ip>/version                                  # firmware build time
curl http://<robot-ip>/skills                                   # live skill list
curl -X POST http://<robot-ip>/up                               # stand up
curl -X POST http://<robot-ip>/skill -d '{"name":"sit"}'        # sit
curl -X POST http://<robot-ip>/nav -d '{"vx":0.1}'              # walk forward
curl -X POST http://<robot-ip>/euler -d '{"pitch":-0.15}'       # squat rear, head up
curl -X POST http://<robot-ip>/rest                             # power down

# Unified high-rate channel — one endpoint, switches on `command`
curl -X POST http://<robot-ip>/control -d '{"command":"nav","vx":0.2,"vyaw":0.3}'
curl -X POST http://<robot-ip>/control -d '{"command":"euler","pitch":0.2,"yaw":0.5}'
curl -X POST http://<robot-ip>/control -d '{"command":"stand_up"}'
```

### Endpoints

| Method | Path | Body | What it does |
|---|---|---|---|
| `GET`  | `/status`  | — | `{model, ip, rssi, uptimeMs, busy, wsClients, freeHeap}` |
| `GET`  | `/version` | — | `{buildDate, buildTime, model}` — confirm what's flashed |
| `GET`  | `/skills`  | — | Live `{postures:[], gaits:[], behaviors:[]}` read from firmware |
| `GET`  | `/pid`     | — | `{kp, ki, kd, active, targetDeg, currentDeg}` — pitch PID state |
| `POST` | `/skill`   | `{"name":"sit","arg":0}` | Run a named skill (`arg` optional, e.g. repeat count) |
| `POST` | `/cmd`     | `{"raw":"ksit"}` or `{"token":"t","args":"10 -5"}` | Raw passthrough for any OpenCat token |
| `POST` | `/move`    | `{"joints":[{"index":8,"angle":30}], "mode":"simultaneous"\|"sequential"}` | Move specific joints |
| `POST` | `/euler`   | `{"roll":<rad>, "pitch":<rad>, "yaw":<rad>}` | Body orientation. Pitch ∈ [-0.2, 0.5], roll ∈ [-0.4, 0.4], yaw ∈ [-1.0, 1.0]. Roll uses IMU loop; pitch uses direct knee+hip fold + closed-loop PID (5 Hz, IMU-feedback); yaw drives head pan (joint 0, +=left). |
| `POST` | `/nav`     | `{"vx":<m/s>, "vy":<m/s>, "vyaw":<rad/s>}` | Body-frame velocity → closest gait. `vy` is ignored (no native lateral). Magnitudes only act as dead-zones; use `/speed` to change gait speed. |
| `POST` | `/pid`     | `{"kp":1.0,"ki":0.2,"kd":0.5}` (any subset) | Live-tune the pitch PID gains |
| `POST` | `/beep`    | `{"notes":[[60,4],[62,4]]}` | Play notes (MIDI number + duration) |
| `POST` | `/gyro`    | `{}` | Toggle IMU-based balancing |
| `POST` | `/speed`   | `{"delta":1}` or `{"delta":-2}` | Speed up / slow down |
| `POST` | `/tilt`    | `{"roll":10,"pitch":-5}` | **Buggy — use `/euler` instead.** Misparses inputs against the firmware's `t axis angle` format. |
| `POST` | `/stop`, `/rest`, `/balance`, `/up`, `/zero` | — | Convenience wrappers → `/skill`. Also stop the pitch PID if active. |
| `POST` | `/control` | `{"command":"<name>", ...}` | Unified high-rate channel — see [`/control`](#control--unified-high-rate-channel) below. |

The RESTful POSTs return `{ok:true, token, cmd, durationMs, response}` on success, or `{ok:false, error:"busy"|"timeout"|"bad_request"|"out_of_range", detail:"…"}` with HTTP 4xx. `/control` returns the smaller `{ok:true}` / `{ok:false, error:"…"}` shape — see its section.

### `/euler` — body pose with closed-loop pitch

- **Roll** is open-loop through the firmware's IMU balance loop (sets `yprTilt[2]`).
- **Pitch** sets a setpoint that a layered PID drives by folding rear knees + hips (negative pitch / head up) or front knees + hips (positive pitch / head down). The IMU's pitch correction is suppressed while `/euler` holds a non-zero pitch; the PID re-issues the joint frame at 5 Hz to track the setpoint and refresh servos against thermal droop.
- **Yaw** drives joint 0 (head pan) directly. Positive yaw = head left.
- Calling `/balance`, `/up`, `/rest`, `/stop`, `/zero`, `/skill`, or `/nav` cleanly stops the PID and re-enables the firmware's pitch IMU loop.
- **Hardware caveat**: at large negative pitch, the front legs carry the body weight and may thermally throttle after 5–10 s, causing the chassis to drop. Stay below `pitch=-0.15` for sustained holds.

### `/nav` — discrete-gait velocity command

The robot only supports a fixed set of gait skills (`wkF`, `wkL`, `wkR`, `bk`, `bkL`, `bkR`, `balance`), so `/nav` maps `(vx, vyaw)` to the closest one rather than producing a continuous velocity. `vy` (lateral) has no native equivalent on these quadrupeds and is dropped. Magnitudes only matter as a dead-zone. Use `/speed {"delta":±N}` to adjust gait speed and `/euler` to lean while walking.

### `/control` — unified high-rate channel

One endpoint that switches on a `command` field and dispatches to the same logic as `/nav`, `/euler`, and the skill-load wrappers. Useful for clients that prefer a single route and a higher command rate (e.g. teleop loops at ~10 Hz). The RESTful endpoints above are still available — `/control` doesn't replace them.

Two semantic differences from the RESTful endpoints:

- **Non-blocking.** Returns `{ok:true}` the moment the command is dispatched; does not wait for the gait/skill to finish. This matches a 0.5 s client timeout in tight follow loops, but it means a long-running skill (e.g. `up` from `rest` takes ~1–2 s) holds the dispatch slot — calls arriving during its tail get HTTP `409 {"ok":false,"error":"busy"}`.
- **Smaller reply.** Just `{ok:true}` on success or `{ok:false, error:"bad_json"|"unknown_command"|"out_of_range"|"busy", detail:"…"}` on failure. No `token`/`durationMs`/`response` (since those need waiting for completion).

Accepted commands and their dispatch:

| `command` | What it dispatches |
|---|---|
| `stop`, `sit_down`, `rest`     | Skill `rest` |
| `stand_up`, `up`               | Skill `up` |
| `stretch`                       | Skill `str` |
| `balance`                       | Skill `balance` |
| `nav`                           | Same as POST `/nav`. JSON keys: `vx` (m/s), `vy` (m/s, ignored), `vyaw` (rad/s). |
| `euler`                         | Same as POST `/euler`. JSON keys: `roll` (rad), `pitch` (rad), `yaw` (rad). Same range checks. |

```sh
curl -X POST http://<robot-ip>/control -d '{"command":"nav","vx":0.2,"vy":0.0,"vyaw":0.0}'
curl -X POST http://<robot-ip>/control -d '{"command":"euler","roll":0.0,"pitch":0.2,"yaw":0.5}'
curl -X POST http://<robot-ip>/control -d '{"command":"stop"}'
```

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
- **Blocking semantics.** RESTful POSTs (`/skill`, `/nav`, `/euler`, …) don't return until the command finishes (or 30 s timeout). Fine for scripting; don't open dozens of parallel connections. Use `/control` for high-rate teleop where waiting for completion would drop frames.
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
