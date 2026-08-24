# StreamMatrix

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt Framework](https://img.shields.io/badge/Qt-5.15%20%7C%206.x-41CD52?logo=qt)](https://www.qt.io/)
[![Platform: Linux & Raspberry Pi](https://img.shields.io/badge/Platform-Linux%20%7C%20Raspberry%20Pi-C51A4A?logo=raspberrypi)](https://www.raspberrypi.com/)

**StreamMatrix** is a high-performance, low-latency video matrix application designed for monitoring multiple live RTSP, RTMP, HTTP, and local video feeds simultaneously. Built with Qt Quick (QML) and FFmpeg, it is specifically tuned for optimal resource efficiency on desktop Linux and embedded systems like the **Raspberry Pi 4 and Raspberry Pi 5**.

---

## 🌟 Acknowledgement & Origins

StreamMatrix is an independent, modernized fork of the wonderful [**CCTV Viewer**](https://github.com/iEvgeny/cctv-viewer) project originally created by **Evgeny S. Maksimov ([@iEvgeny](https://github.com/iEvgeny))**. 

We extend our sincere gratitude to Evgeny for establishing the foundational architecture and QML AV pipeline. As upstream development slowed down, this fork was created to:
- Actively maintain and modernize the codebase for newer Linux environments (Debian 12/13 Trixie, Raspberry Pi OS).
- Deliver native support for modern Wayland compositors (`labwc`, `wayfire`) and X11.
- Optimize multi-stream hardware acceleration (DRM, VA-API) to allow smooth, low-power playback of high-resolution IP camera grids on single-board computers like the Raspberry Pi 5.
- Introduce critical reliability features including stream health diagnostics, automated reconnection watchdog, and dual-profile sub-stream management.

---

## 🚀 Key Features & Enhancements

### ⚡ Performance & Hardware Acceleration
- **Raspberry Pi 5 & 4 Tuned**: Seamless out-of-the-box hardware decoding (`-hwaccel drm`) and smooth Wayland/Labwc rendering.
- **Cached `SwsContext` Scaling**: Eliminates per-frame memory allocation/deallocation churn and scaling bottleneck, reducing CPU load.
- **Zero-Copy Video Path**: Improved GLX and DRM texture handling with dynamic resolution change support.
- **Thread-Safe Glitch-Free Audio**: Overhauled audio I/O pipeline with continuous frame buffering and thread synchronization, eliminating audio stuttering and buffer underruns.
- **Deadlock-Free Shutdown**: Interrupt-aware producer/consumer queues prevent deadlocks and hung processes on exit.

### 📊 Diagnostics & Stream Health
- **Live Stream Diagnostics HUD**: Press <kbd>D</kbd> to toggle an on-screen HUD displaying real-time **FPS**, **Resolution**, **Video Codec**, **Hardware Acceleration status `[HW]`**, and **Bitrate**.
- **Auto-Reconnect Watchdog**: Automatically detects dropped feeds or network stalls and reconnects using exponential backoff without crashing or freezing. Includes an interactive on-screen "Retry" action.
- **Dual-Profile Resolution Switching**: Configure a high-resolution main stream (`URL`) and low-bandwidth sub-stream (`Sub-Stream URL`) per camera. The grid automatically renders lightweight sub-streams and seamlessly elevates to full resolution upon double-clicking into full-screen.

### 🖥️ Modernized UI & System Integration
- **Quote-Aware FFmpeg Parser**: Tokenizer supports complex arguments and quoted options (e.g. `-rtsp_transport tcp -fflags nobuffer`).
- **Multi-User Safe**: Instance locking uses user-isolated runtime paths (`$XDG_RUNTIME_DIR`), allowing multiple users to run isolated instances concurrently.
- **Smart Wayland/X11 Launcher**: Automatically detects active display sockets when launched from desktop shortcuts, systemd services, or SSH.
- **Customizable Layouts & Presets**: Flexible $M \times N$ grid layouts with span adjustments, digital pan/zoom, preset carousel cycling, and kiosk mode.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| <kbd>F11</kbd> / <kbd>Double Click</kbd> | Toggle Full-Screen Mode |
| <kbd>D</kbd> | Toggle Stream Diagnostics HUD (FPS, Bitrate, Codec, Resolution) |
| <kbd>M</kbd> | Toggle Mute/Unmute for Selected Viewport |
| <kbd>Alt</kbd> + <kbd>1</kbd> .. <kbd>9</kbd> | Switch Directly to Preset 1 through 9 |
| <kbd>Alt</kbd> + <kbd>←</kbd> / <kbd>→</kbd> | Switch to Previous / Next Preset |
| <kbd>Space</kbd> | Pause / Resume Preset Carousel |
| <kbd>Esc</kbd> | Exit Full-Screen or Deselect |
| <kbd>Ctrl</kbd> + <kbd>Q</kbd> | Quit Application |

---

## 🛠️ Building & Installing

### Prerequisites (Debian / Ubuntu / Raspberry Pi OS)

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    qtbase5-dev \
    qtdeclarative5-dev \
    qtmultimedia5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    qml-module-qtquick2 \
    qml-module-qtquick-layouts \
    qml-module-qtquick-controls2 \
    qml-module-qtquick-window2 \
    qml-module-qtmultimedia \
    qml-module-qtgraphicaleffects \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev \
    libavdevice-dev \
    libva-dev
```

### Clone & Build

Clone the repository recursively to fetch all dependencies:

```bash
git clone --recurse-submodules https://github.com/ldl805/stream-matrix.git
cd stream-matrix

# Configure and build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Install System-Wide

```bash
sudo cmake --install build
```

Or run directly from the build directory using the included launcher:
```bash
./run.sh
```

---

## ⚙️ Configuration & Hardware Acceleration

### Recommended Raspberry Pi 5 & 4 FFmpeg Options
In **Settings → Viewport → Default FFmpeg options**, the following defaults are pre-configured for low latency:
```text
-hwaccel drm -rtsp_transport tcp -fflags nobuffer -flags low_delay
```

### Common Camera RTSP Formats
- **Main Stream**: `rtsp://username:password@192.168.1.50:554/stream1`
- **Sub Stream**: `rtsp://username:password@192.168.1.50:554/stream2`

---

## 📜 License & Credits

- **StreamMatrix** is licensed under the [GNU General Public License v3.0](LICENSE).
- Based on [cctv-viewer](https://github.com/iEvgeny/cctv-viewer) &copy; Evgeny S. Maksimov.
- Uses [FFmpeg](https://ffmpeg.org/) (LGPL/GPL) and the [Qt Framework](https://www.qt.io/) (LGPLv3/GPLv3).
