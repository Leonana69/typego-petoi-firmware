# OpenCatESP32 — OpenCat Framework on ESP32/BiBoard

OpenCatESP32 runs the OpenCat quadruped robotics framework on [BiBoard](https://www.petoi.com/products/biboard-esp32-development-board-for-quadruped-robot?utm_source=github&utm_medium=code&utm_campaign=github-opencat) — an ESP32-based development board designed for multi-degree-of-freedom legged robots with up to 12 servos. Developed by [Petoi](https://www.petoi.com?utm_source=github&utm_medium=code&utm_campaign=github-opencat), the maker of futuristic programmable robotic pets.

This is the codebase for current-generation Petoi hardware. If you're on the older NyBoard (ATmega328P), see the main [OpenCat repo](https://github.com/PetoiCamp/OpenCat).


[![BittleESP32](https://github.com/PetoiCamp/NonCodeFiles/blob/master/gif/BiBoard.gif)](https://www.youtube.com/watch?v=GTgps_H990w)

[![BittleGap](https://github.com/PetoiCamp/NonCodeFiles/blob/master/gif/gap.gif)](https://youtu.be/1qhNRSQTcG4)

*Click either GIF to watch the demo.*

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
