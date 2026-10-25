# StreamMatrix

[![Latest Release](https://img.shields.io/github/v/release/ldl805/stream-matrix?color=blue&logo=github)](https://github.com/ldl805/stream-matrix/releases/latest)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt Framework](https://img.shields.io/badge/Qt-5.15%20%7C%206.x-41CD52?logo=qt)](https://www.qt.io/)
[![Platform: Linux, Windows & Raspberry Pi](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20Raspberry%20Pi-C51A4A?logo=raspberrypi)](https://www.raspberrypi.com/)

**StreamMatrix** is a high-performance, low-latency video matrix application for monitoring multiple live RTSP, RTMP, HTTP, and local video feeds simultaneously. Built with Qt Quick (QML) and FFmpeg, it runs natively on **Linux (`x86_64` & `arm64`)**, **Windows (`x86_64`)**, and single-board computers like the **Raspberry Pi 4 & 5**.

---

## 📦 Installation

Download pre-built packages from the [**Latest GitHub Release**](https://github.com/ldl805/stream-matrix/releases/latest):

| Platform | Architecture | Package Format | Quick Install / Usage |
| :--- | :--- | :--- | :--- |
| **Windows** | `x86_64` | `.exe` Installer / `.zip` Portable | Run `stream-matrix-<ver>-Windows-x86_64.exe` or extract the `.zip` archive |
| **Debian / Ubuntu / Raspberry Pi OS** | `amd64` / `arm64` | `.deb` Package | `sudo apt install ./stream-matrix_<ver>_<arch>.deb` |
| **Generic Linux** | `x86_64` / `aarch64` | `.tar.gz` Archive | Extract archive and run `bin/stream-matrix` |

---

## ✨ Key Features

- **Low-Latency Multi-Stream Engine**: Hardware-accelerated decoding (`vaapi`, `drm`, `d3d11va`, `dxva2`, or `auto`) paired with multi-threaded ARM NEON / x86 software decoding, keyframe-aware queue management, and zero-overhead muted audio bypass.
- **Dual-Profile Sub-Stream Switching**: Configure both a primary `URL` and a lightweight `Sub-Stream URL` per camera—automatically displaying low-bandwidth sub-streams in grid view and switching to full resolution in full-screen.
- **Automated Watchdog & Reconnection UI**: Detects dropped connections or stalled feeds (7s frame watchdog) with immediate socket teardown, exponential backoff countdowns, FFmpeg root-cause diagnostics, and a debounced **Retry Now** control.
- **Live Stream Diagnostics HUD**: Toggle an on-screen overlay (<kbd>D</kbd>) showing real-time **FPS**, **Resolution**, **Codec**, **Hardware Acceleration `[HW]`**, and **Bitrate** per viewport.
- **Flexible Layouts & Presets**: Customizable $M \times N$ grids with viewport spanning, digital pan/zoom, preset carousel cycling, kiosk mode, and native **Wayland** & **X11** support.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| <kbd>F11</kbd> / <kbd>Double Click</kbd> | Toggle Full-Screen Mode |
| <kbd>D</kbd> | Toggle Stream Diagnostics HUD (FPS, Bitrate, Codec, Resolution) |
| <kbd>M</kbd> | Toggle Mute / Unmute for Selected Viewport |
| <kbd>Alt</kbd> + <kbd>1</kbd> .. <kbd>9</kbd> | Switch Directly to Preset 1–9 |
| <kbd>Alt</kbd> + <kbd>←</kbd> / <kbd>→</kbd> | Switch to Previous / Next Preset |
| <kbd>Space</kbd> | Pause / Resume Preset Carousel |
| <kbd>Esc</kbd> | Exit Full-Screen or Deselect Viewport |
| <kbd>Ctrl</kbd> + <kbd>Q</kbd> / <kbd>Q</kbd> | Quit Application |

---

## ⚙️ Recommended RTSP Configuration

In **Settings → Viewport → Default FFmpeg options**, the following defaults are pre-configured for reliable, low-latency RTSP streaming:

```text
-rtsp_transport tcp -fflags nobuffer -timeout 20000000 -probesize 5000000 -analyzeduration 5000000
```

> **Raspberry Pi 5 Note:** The BCM2712 CPU decodes H.264 streams in software using multi-threaded ARM NEON instructions (hardware decode on Pi 5 is dedicated to HEVC/H.265). Leave `-hwaccel drm` off for H.264 feeds.

---

## 🛠️ Building from Source

### Debian / Ubuntu / Raspberry Pi OS

```bash
# 1. Install build & runtime dependencies (compatible with Debian 12/13 & Ubuntu 22.04/24.04)
sudo apt update
sudo apt install -y \
    build-essential cmake pkg-config libgtest-dev \
    qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev qttools5-dev qttools5-dev-tools \
    qml-module-qtquick2 qml-module-qtquick-layouts qml-module-qtquick-controls \
    qml-module-qtquick-controls2 qml-module-qtquick-dialogs qml-module-qtquick-window2 \
    qml-module-qt-labs-settings qml-module-qtmultimedia qml-module-qtgraphicaleffects \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev \
    libavdevice-dev libva-dev

# 2. Clone, configure, and compile (pass -DBUILD_TESTING=OFF to skip unit tests)
git clone https://github.com/ldl805/stream-matrix.git
cd stream-matrix
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 3. Install system-wide (or launch locally via ./run.sh)
sudo cmake --install build
```

<details>
<summary><strong>Windows Build & Packaging (MSYS2 / MinGW-w64)</strong></summary>

```bash
# 1. Install toolchain, Qt 5, FFmpeg, GTest, and NSIS in MSYS2 MINGW64
pacman -S --needed \
    mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
    mingw-w64-x86_64-pkgconf mingw-w64-x86_64-gtest mingw-w64-x86_64-nsis \
    mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-declarative \
    mingw-w64-x86_64-qt5-multimedia mingw-w64-x86_64-qt5-quickcontrols \
    mingw-w64-x86_64-qt5-quickcontrols2 mingw-w64-x86_64-qt5-graphicaleffects \
    mingw-w64-x86_64-qt5-svg mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-ffmpeg

# 2. Configure, build, and run unit tests
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# 3. Package standalone ZIP archive and NSIS installer
cpack --config build/CPackConfig.cmake -B build -G "ZIP;NSIS"
```
</details>

<details>
<summary><strong>Flatpak Build & Run</strong></summary>

```bash
sudo apt install -y flatpak flatpak-builder
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install -y flathub org.kde.Platform//5.15-25.08 org.kde.Sdk//5.15-25.08

flatpak-builder --user --install --force-clean build-flatpak org.streammatrix.StreamMatrix.yaml
flatpak run org.streammatrix.StreamMatrix
```
</details>

---

## 📜 Origins & License

- **StreamMatrix** is an independent, modernized fork of [**CCTV Viewer**](https://github.com/iEvgeny/cctv-viewer), originally created by **Evgeny S. Maksimov ([@iEvgeny](https://github.com/iEvgeny))**. We thank Evgeny for establishing the foundational architecture and QML AV pipeline.
- Licensed under the [GNU General Public License v3.0](LICENSE).
- Powered by [FFmpeg](https://ffmpeg.org/) (LGPL/GPL) and the [Qt Framework](https://www.qt.io/) (LGPLv3/GPLv3).
