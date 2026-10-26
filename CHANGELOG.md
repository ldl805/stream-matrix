# Changelog

All notable changes to **StreamMatrix** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.1.2] - 2026-10-26

> **Release Summary:** Major under-the-hood RTSP streaming efficiency and concurrency overhaul for Raspberry Pi 5 and single-board computers (SBCs)—featuring zero-allocation packet moves, lazy decoder thread creation, demuxer-level muted audio discard (`AVDISCARD_ALL`), automatic Pi 5 DRM hardware HEVC decoding with zero-`swscale` NEON transfer, GUI frame coalescing, staggered grid startup, and fixes for the interactive **"Retry Now"** button and live HUD bitrate metrics.

### Changed — Under-the-Hood Improvements (RTSP & Raspberry Pi 5 / SBC Efficiency)
- **Zero-Allocation `AVSmartPtr` Moves & Direct Worker Dispatch**: Refactored `AVSmartPtr` (`AVPacketPtr` / `AVFramePtr`) move semantics to transfer `m_ref` ownership in $O(1)$ without heap-allocating/freeing `AVPacket` structs on every move, added a persistent `m_currentArgs` slot in `QmlAVWaitingQueue::takeFront()`, and replaced per-packet `std::tuple_cat` copies in `QmlAVTask` with direct `std::apply` dispatch.
- **Lazy Decoder Thread Creation & Muted Audio Stream Discard (`AVDISCARD_ALL`)**: Deferred worker thread creation in `QmlAVDecoder` until a stream is actively opened (eliminating idle threads for empty grid tiles and muted audio streams) and configured `AVStream::discard = AVDISCARD_ALL` on muted audio tracks so FFmpeg skips demuxing audio RTP packets at the parser level.
- **Removed Per-Packet Worker `std::this_thread::yield()`**: Eliminated unconditional `std::this_thread::yield()` calls after every task iteration in `QmlAVThread`, preventing hundreds of unnecessary voluntary context switches per second across multi-camera grids while retaining condition-variable sleep when queues are empty.
- **Raspberry Pi 5 Hardware HEVC Decoding (`AV_HWDEVICE_TYPE_DRM`) & `AV_CODEC_FLAG2_FAST`**:
  - Automatically selects `AV_HWDEVICE_TYPE_DRM` on ARM64 Linux when decoding HEVC (`AV_CODEC_ID_HEVC`) via the Pi 5 `rpi-hevc-dec` hardware block while keeping multi-threaded NEON software decode for H.264.
  - Updated `QmlAVVideoFrame::swPixelFormat()` to query `av_hwframe_transfer_get_formats()` when the hardware `sw_format` uses proprietary tiled layouts (`AV_PIX_FMT_RPI4_8` / `SAND128`), transferring directly into Qt-native `AV_PIX_FMT_YUV420P` via FFmpeg's NEON `unsand` routine with zero `swscale` conversion overhead.
  - Enabled `AV_CODEC_FLAG2_FAST` for real-time video streams and added graceful fallback to software decoding if hardware device initialization fails.
- **GUI Video Frame Coalescing**: Added `m_pendingVideoFrame` coalescing in `QmlAVPlayer` so at most one `presentPendingVideoFrame()` call is queued per viewport on the GUI thread at a time; if a burst of decoded frames arrives before the next VSync, intermediate frames are superseded before any `av_hwframe_transfer_data`, `sws_scale`, or OpenGL texture upload occurs.
- **Staggered Grid Startup, Low-Duty Reconnect Spinner & Corrupt Packet Discard**: Staggered `Player.qml` initial stream startup timers (`100 + (viewportIndex % 16) * 60` ms) to avoid simultaneous RTSP handshake spikes on layout switches, replaced dual 60 FPS continuous `RotationAnimator`s on offline tiles with a single 4 Hz stepped `Timer`, and enabled `AVFMT_FLAG_DISCARD_CORRUPT` on real-time streams.

### Added
- **Live Real-Time RTSP Bitrate Measurement**: Added atomic packet byte-rate tracking in `QmlAVDemuxer` so the Diagnostics HUD (<kbd>D</kbd>) reports live kbps for RTSP streams where FFmpeg leaves container-level `avFormatCtx->bit_rate` at `0`.

### Fixed
- **Interactive "Retry Now" Button on Failed Streams**: Fixed `ViewportsLayout.qml` stacking its full-viewport `MouseArea` above `playerContainer` and swallowing mouse hover/click events intended for `Player.qml`'s **"Retry Now"** button; prevented fast double-clicks on **"Retry Now"** from toggling fullscreen and prevented `QmlAVPlayer::retry()` from double-incrementing `reconnectAttempt`.
- **Demuxer `avFormatCtx` Thread-Safety Race**: Fixed a data race / potential use-after-free where `QmlAVPlayer::updateMetrics()` read `m_context->avFormatCtx` on the GUI thread while `m_loaderThread` was concurrently inside `avformat_open_input` or `avformat_find_stream_info`.
- **Decoder Error-Path Resource Cleanup**: Fixed `QmlAVDecoder::open()` leaking `m_avCodecCtx` and leaving the decoder wedged if codec initialization failed mid-setup.
- **Stale HUD Metrics After Stream Stop**: Reset `bitrate`, `videoCodec`, `videoResolution`, `isHWAccelerated`, `framesDecoded`, and `framesDiscarded` in `QmlAVPlayer::cleanup()`.
- **Viewport Layout Model Leaks & Cell Merging**: Added the missing `count` Q_PROPERTY to `ViewportsLayoutModel` (fixing cell merging in `ViewportsLayout.qml::mergeCells()`), deleted removed `ViewportsLayoutItem` `QObject`s in `clear()`/`resize()`/`remove()`/`set()` to prevent memory leaks, and guarded `Context::m_config` with `QQmlEngine::CppOwnership` to prevent double-free on exit.
- **Event Filter & QML Binding Fixes**: Fixed `EventFilter::setEventTypes()` not storing `m_eventTypes`, fixed `SideBar.qml` missing `return` in `highlighted`, fixed `-1` vs `0` index truthiness in `ViewportsLayout.qml`, and fixed `CursorShape.qml` property references.

## [2.1.1] - 2026-10-25

> **Release Summary:** Resolves source-build and QML runtime dependency issues on fresh Debian 13 (Trixie) and Ubuntu installations ([#1](https://github.com/ldl805/stream-matrix/issues/1)), makes Google Test optional during CMake configuration, embeds QML runtime package dependencies into `.deb` installers, and streamlines project documentation and release notes.

### Added
- **Optional Unit Test Compilation (`BUILD_TESTING`)**: Added the standard `BUILD_TESTING` CMake option (enabled by default, disable via `-DBUILD_TESTING=OFF`) in `src/qmlav/CMakeLists.txt` to allow building StreamMatrix without compiling the unit test suite.
- **Graceful Google Test Detection**: Updated `src/qmlav/src/tests/CMakeLists.txt` to probe for Google Test using `find_package(GTest QUIET)` instead of `REQUIRED`, automatically skipping unit test compilation with a status message when `libgtest-dev` is not installed ([#1](https://github.com/ldl805/stream-matrix/issues/1)).
- **Debian `.deb` Runtime QML Dependencies**: Added explicit Qt5 QML runtime module dependencies (`qml-module-qtquick2`, `qml-module-qtquick-layouts`, `qml-module-qtquick-controls`, `qml-module-qtquick-controls2`, `qml-module-qtquick-dialogs`, `qml-module-qtquick-window2`, `qml-module-qt-labs-settings`, `qml-module-qtmultimedia`, `qml-module-qtgraphicaleffects`) to `CPACK_DEBIAN_PACKAGE_DEPENDS` so `apt install ./stream-matrix_*.deb` pulls in all required QML plugins automatically.
- **Automated Release Changelog Extraction**: Updated `.github/workflows/release.yml` to automatically populate GitHub Release notes directly from `CHANGELOG.md` whenever a version tag is pushed.

### Fixed
- **Missing Debian 13 (Trixie) Build & Runtime Dependencies**: Added `libgtest-dev`, `qml-module-qtquick-controls`, `qml-module-qtquick-dialogs`, and `qml-module-qt-labs-settings` to the Debian/Ubuntu installation instructions in `README.md`, fixing `QQmlApplicationEngine failed to load component` (`Qt.labs.settings`, `QtQuick.Dialogs`, and `QtQuick.Controls 1.2` `MessageDialog`) startup failures on fresh Debian 13 systems ([#1](https://github.com/ldl805/stream-matrix/issues/1)).

### Changed
- **Documentation & Changelog Cleanup**: Decluttered `README.md` with a quick-install package table, concise feature summaries, and collapsible platform build sections; expanded `CHANGELOG.md` with release summaries, complete version histories, and comparison links.

## [2.1.0] - 2026-10-20

> **Release Summary:** Introduces native Windows (`x86_64`) support with D3D11VA/DXVA2 hardware decoding, automated portable `.zip` and NSIS `.exe` installer packaging, and multi-platform CI/CD release publishing.

### Added
- **Native Windows Support (`x86_64`)**: Full native Windows build and runtime support using Qt 5 and FFmpeg (`d3d11va` / `dxva2` GPU hardware acceleration and multi-threaded software decoding).
- **Windows Portable & Installer Packages**: Added automated `windeployqt` and MinGW/FFmpeg DLL runtime dependency bundling in CMake/CPack, producing standalone portable `.zip` archives (`stream-matrix-<version>-Windows-x86_64.zip`) and NSIS `.exe` installers (`stream-matrix-<version>-Windows-x86_64.exe`).
- **Windows CI & Automated Release Publishing**: Extended GitHub Actions CI and Release workflows to automatically compile, test, and publish Windows `x86_64` packages alongside Linux `x86_64` and `arm64` packages.
- **Hardware Acceleration Auto-Selection (`-hwaccel auto`)**: Added automatic hardware device selection (`d3d11va`/`dxva2` on Windows, `vaapi`/`drm` on Linux) and HUD `[HW]` detection for `hw_device_ctx`-backed decoders.

### Fixed
- **Windows Local File Path Handling & Playback Pacing**: Fixed `QUrl` local path resolution for Windows drive letters (`C:/...`), trimmed trailing slashes on RTSP stream paths (`/live2/` -> `/live2`) to prevent strict RTSP `404 Stream Not Found` errors, and disabled real-time packet overflow dropping for local media files.
- **Cross-Platform Signal, INI Settings & Lockfile Handling**: Added Windows console control handler (`SetConsoleCtrlHandler`), parent terminal console attachment for CLI flags (`--help`, `--version`), `QSettings::IniFormat` default config storage (`%APPDATA%\StreamMatrix\StreamMatrix.ini`), and `USERNAME` fallback for single-instance lockfiles.

## [2.0.0] - 2026-10-20

> **Release Summary:** Major RTSP pipeline and resource efficiency overhaul—featuring immediate socket teardown on stalled feeds, a 6-second reconnect cooldown for embedded RTSP servers, zero-overhead muted audio skipping, and leaner frame queues for Raspberry Pi 5 multi-camera grids.

### Changed
- **Tuned RTSP AVDictionary & Probe Defaults**: Increased default RTSP socket I/O `timeout` to 20s (`20000000`), `analyzeduration` to 5s (`5000000`), and `probesize` to 5 MB (`5000000`) to reliably capture keyframes and handle transient camera or base-station stalls.
- **6-Second Reconnect Cooldown**: Raised initial reconnect backoff from 2s to 6s (max 30s) to give embedded RTSP servers (such as Eufy HomeBase Pro and NVR appliances) sufficient time to tear down stale TCP sockets and avoid `404 Not Found` session locks.
- **Zero-Overhead Muted Audio Bypass**: Skipped audio packet decoding, resampling (`swr_convert`), and cross-thread signal dispatch at the demuxer level whenever a viewport is muted (`volume == 0`), substantially reducing CPU overhead on multi-camera grids.
- **Leaner Raspberry Pi 5 Queue Limits**: Reduced decoded video frame queue limit (`VIDEO_FRAMES_LIMIT`) from 24 to 8 and packet queue limit (`VIDEO_PACKETS_LIMIT`) from 64 to 32 to lower RAM usage and glass-to-glass latency.
- **FFmpeg 7 Option Compatibility & Sanitization**: Automatically mapped deprecated `-stimeout` options to `-timeout` and filtered out CLI-only flags (`-thread_queue_size`, `-i`) before calling `avformat_open_input`.

### Fixed
- **Stalled-Stream Watchdog Socket Teardown**: Fixed `QmlAVPlayer::updateMetrics()` so stalled streams immediately tear down the dead RTSP socket (`cleanup()`) *before* starting the backoff timer, rather than holding the broken socket open until reconnect execution.
- **Hidden Preset Background Reconnection Leak**: Cancelled active reconnect timers on `stop()` and `setAutoPlay(false)` and gated reconnection on `m_autoPlay`, preventing inactive layout presets from reconnecting in the background and colliding with active camera sessions.
- **Per-Viewport Option Mutation**: Fixed `Player.qml` and `SideBar.qml` mutating `avOptions` in place when merging `defaultAVFormatOptions`.

## [1.2.2] - 2026-10-15

> **Release Summary:** Restores the automated stream reconnection loop and 7-second stalled-stream watchdog (reverting the manual-only experiment from `v1.2.0`) while retaining full interactive retry controls and FFmpeg error diagnostics.

### Changed
- **Automatic Reconnection Loop Restored**: Re-enabled automated exponential backoff reconnection (`scheduleReconnect()`) whenever a live stream drops or encounters an EOF/network error, ensuring unattended wall-mounted monitors recover without manual intervention.
- **Stalled-Stream Watchdog Recovery**: Restored the 7-second frame-starvation watchdog timer in `QmlAVPlayer::updateMetrics()` so silently frozen RTSP sockets automatically cycle and recover.
- **Unified Auto & Manual Retry Overlay**: Preserved the interactive **"Retry Now"** button, live backoff countdown timer, and detailed FFmpeg socket error reporting alongside automatic background recovery.

## [1.2.0] - 2026-10-12

> **Release Summary:** Experimental release that transitioned stream recovery to manual-only retry mode and updated default RTSP FFmpeg documentation (subsequently reverted in `v1.2.2` in favor of automatic watchdog recovery).

### Changed
- **Manual-Only Stream Reconnection (Reverted in 1.2.2)**: Temporarily disabled automatic background reconnect timers in `QmlAVPlayer` so failed streams waited for explicit user interaction via the on-screen **"Retry Now"** button.
- **RTSP FFmpeg Option Guidance**: Updated `README.md` recommended FFmpeg options to highlight hardware decoding caveats on Raspberry Pi 5 (where H.264 uses multi-threaded CPU NEON decoding while hardware decode is reserved for HEVC/H.265).

## [1.1.1] - 2026-10-04

> **Release Summary:** Major RTSP decoding performance and latency update for Raspberry Pi 5 and multi-core systems—enabling active `FF_THREAD_FRAME` multi-threaded decoding and keyframe-aware packet queue dropping.

### Changed
- **Multi-Threaded Frame Decoding**: Ensured multi-threaded slice and frame decoding (`FF_THREAD_FRAME`) is actively utilized by filtering out conflicting `low_delay` flags during video decoder initialization, significantly improving multi-core CPU utilization on devices like Raspberry Pi 5.
- **Keyframe-Aware Overflow Protection**: Refactored packet waiting queue to use `std::deque` with keyframe-aware dropping (`AV_PKT_FLAG_KEY`), jumping cleanly to the next keyframe sync point when network buffers overflow and preventing gray frames or visual smearing.
- **Latency Backlog Reduction**: Reduced video packet buffer limit to ~2 seconds of backlog (64 packets), allowing streams that temporarily lag to rapidly catch up and resynchronize to live playback.
- **Optimized RTSP Startup & Probe Durations**: Tuned stream analysis duration (`analyzeduration`) and probe size (`probesize`) for faster initial camera startup and reconnect times.
- **Cleaned Obsolete Network Flags**: Removed deprecated `stimeout` and UDP-only `buffer_size` from default TCP connection parameters.

## [1.1] - 2026-10-04

> **Release Summary:** Introduces an interactive reconnection overlay with tactile button feedback, real-time attempt & countdown status messages, low-level FFmpeg error diagnostics, and refreshed blue application branding.

### Added
- **Interactive Reconnect Confirmation**: Instant tactile and visual feedback on the **"Retry Now"** button: hover highlights, pointing hand cursor, smooth scale compression, active `"Retrying..."` working state with animated spinner, and click debouncing.
- **Live Reconnection Status Updates**: Real-time status messaging during reconnection attempts, displaying active connection phases (`"Connecting (attempt N)..."`, `"Attempting connection to stream..."`) and live second-by-second countdowns (`"Retrying in Xs (attempt N)..."`) during exponential backoff.
- **Detailed Socket & FFmpeg Error Diagnostics**: Captured low-level socket, protocol, and timeout errors (e.g. `"Connection refused"`, `"Connection timed out"`, `"Server returned 404 Not Found"`, `"No route to host"`), displayed directly on the stream error overlay.
- **New Blue Icon Branding**: Redesigned application icon and branding with a sleek blue color palette matching the StreamMatrix dark interface styling.

### Fixed
- **Reconnection State Preservation**: Fixed an issue where manual retry destroyed the reconnection overlay prematurely and left a blank screen; retry attempts now cleanly preserve reconnect state until live video frames arrive.

## [1.0.4] - 2026-10-02

> **Release Summary:** Adds universal Linux packaging support with a Flathub-ready Flatpak manifest, validated AppStream 1.0 metadata, and freedesktop.org desktop entries.

### Added
- **Flatpak & Flathub Packaging**: Added Flatpak manifest `org.streammatrix.StreamMatrix.yaml` targeting KDE Application Platform `5.15-25.08` with sandboxed network access, Wayland/X11 display sockets, PulseAudio/PipeWire audio, and DRI hardware acceleration.
- **AppStream 1.0 Metadata**: Added validated `org.streammatrix.StreamMatrix.metainfo.xml` with release history and feature descriptions for Linux software centers (GNOME Software, KDE Discover).
- **Freedesktop Desktop Entry & Icons**: Added `org.streammatrix.StreamMatrix.desktop` and scalable `hicolor` SVG icon installation rules in `CMakeLists.txt`.

### Fixed
- **Sidebar GitHub Link**: Updated the repository URL on the sidebar logo from upstream `cctv-viewer` to `ldl805/stream-matrix`.

## [1.0.3] - 2026-10-01

> **Release Summary:** Comprehensive RTSP streaming stability update resolving demuxer queue stalls, RTSP 404/453 session exhaustion, single-packet decode jitter, and FFmpeg 7 memory leaks, alongside automated multi-arch GitHub Releases.

### Added
- **Automated GitHub Releases & Multi-Arch Packaging**: Added CPack support generating Debian `.deb` and `.tar.gz` distribution packages for `x86_64` (`amd64`) and `aarch64` (`arm64`), plus `.github/workflows/release.yml` for automated GitHub Releases on version tags.
- **CMake Test Integration**: Enabled CTest at repository root so all unit tests run via `ctest --test-dir build`.

### Fixed
- **RTSP Demuxer Queue Stalls & Choppiness**: Added `dropOnOverflow` ring buffer mode to `QmlAVWaitingQueue` so the demuxer thread drops stale unrendered packets rather than blocking when video decoding experiences transient slowdowns.
- **RTSP 404/453 Reconnection Failures**: Added immediate `avformat_close_input` in `QmlAVDemuxer` destructor to promptly send RTSP `TEARDOWN` and release server sockets before creating a new connection, preventing cameras from rejecting reconnect attempts due to session exhaustion.
- **Single-Packet Lockstep Decode Jitter**: Restructured the real-time decoding loop in `QmlAVDecoder` to eagerly drain all available decoded frames after sending each packet (`avcodec_receive_frame` loop).
- **Multi-Threaded H.264 Software Decoding on Pi 5 / ARM**: Configured FFmpeg multi-threaded slice and frame decoding (`thread_count = 2`, `thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME`) for software H.264 streams.
- **Frozen Stream Auto-Recovery**: Integrated a 7-second stalled-stream watchdog in `QmlAVPlayer::updateMetrics()` that triggers an automatic soft reconnect if no new video frames arrive from an active stream.
- **Unbounded Audio Queue Memory Growth**: Capped `m_frames` queue in `QmlAVAudioIODevice` to 50 frames and avoided enqueuing audio buffers when muted (`volume <= 0.0`), preventing memory leaks on video-only monitoring feeds.
- **FFmpeg 7.x Channel Layout Memory Leak**: Added explicit `av_channel_layout_uninit` in `QmlAVResampler` destructor and prior to layout copies.
- **Stale Context Pointer Segfaults**: Cached stream properties (`timeBase`, `startTime`, `sampleAspectRatio`) in `QmlAVDecoder` so PTS calculations in `QmlAVFrame` do not dereference freed `AVStream` pointers after demuxer teardown.
- **Default FFmpeg Network Options**: Updated default RTSP network parameters with 5-second socket timeouts and a 1 MB socket buffer, removing invalid hardware accel flags for CPU-decoded H.264.

## [1.0.2] - 2026-09-16

> **Release Summary:** Fixes keyboard shortcut interception and clean application shutdown across Wayland, X11, and terminal sessions.

### Fixed
- **Global Key Event Interception for Quit Shortcuts**: Implemented `QuitShortcutFilter` installed at the `QGuiApplication` level in C++ to intercept quit keystrokes (`Ctrl+Q`, `Ctrl+W`, `Ctrl+C`, `Alt+F4`, and `q`/`Q` outside text input controls) before child QML items can consume them.
- **QML Key Propagation**: Fixed `Keys.onPressed` handlers in `ViewportsLayout.qml` and `SideBar.qml` that were implicitly accepting all key events (`event.accepted = true` by default) and blocking global shortcuts when viewports or the sidebar held focus.
- **Application-Scoped QML Shortcuts**: Upgraded shortcuts in `RootWindow.qml` to `Qt.ApplicationShortcut` context and expanded quit sequences to include `StandardKey.Quit` and `StandardKey.Close`.
- **POSIX Signal Handling**: Added async-signal-safe Unix socketpair handling for `SIGINT` (terminal `Ctrl+C`) and `SIGTERM`, ensuring clean Qt event-loop shutdown and RTSP socket teardown.

## [1.0.1] - 2026-08-29

> **Release Summary:** Resolves worker thread deadlocks during stream teardown, fixes QML window close hooks, and hardens planar video buffer handling.

### Fixed
- **Shutdown and Quit Execution**: Connected the QML engine `quit` signal to `QGuiApplication::quit` and added a window `onClosing` hook so window-manager close events cleanly stop all active streams.
- **Worker Thread Deadlock Prevention**: Fixed `requestInterrupt` across decoder waiting queues to properly wake blocked producer threads during shutdown and feed cancellation.
- **Demuxer Loop Flood on Failure**: Corrected decode error propagation in the demuxer loop to prevent tight infinite loop spinning and Qt event queue saturation on broken streams.
- **Thread Concurrency in `wait()`**: Synchronized worker thread completion waiting on condition variables to prevent concurrent `join()` data races.
- **Video Buffer Plane Count & Context Safety**: Ensured planar video buffers return accurate active plane counts for Qt Quick rendering and guarded against double-free in the `Context` destructor.

## [1.0.0] - 2026-08-24

> **Release Summary:** Initial release of **StreamMatrix** as an independent, modernized fork of `cctv-viewer`, introducing the live diagnostics HUD, automated reconnect watchdog, dual-profile sub-stream switching, and Wayland/Pi 5 optimizations.

### Added
- **Live Stream Diagnostics Overlay (HUD)**: Press <kbd>D</kbd> to inspect real-time FPS, Resolution, Codec, Hardware Acceleration indicator `[HW]`, and Bitrate.
- **Automated Reconnect Watchdog**: Background watchdog with exponential backoff on dropped feeds and interactive on-screen **"Retry"** action.
- **Dual-Profile Sub-Stream Switching**: Support for defining high-resolution `URL` and low-bandwidth `Sub-Stream URL` per camera, automatically rendering sub-streams in grid layouts and switching to full resolution in full-screen mode.
- **Smart Wayland / X11 Auto-Detection**: Launcher script (`run.sh`) that dynamically detects `wayland-0` / `:0` and configures `QT_QPA_PLATFORM="wayland;xcb"`.
- **Quote-Aware Option Tokenizer**: Robust FFmpeg argument parser supporting quoted strings and complex option flags.
- **Custom Camera Naming**: Ability to assign custom display titles to each camera viewport.

### Changed
- **Cached `SwsContext` Scaling**: Replaced per-frame `SwsContext` allocation/free cycles with persistent context caching using `SWS_FAST_BILINEAR`.
- **Thread-Safe Continuous Audio Playback**: Implemented a mutex-protected audio frame queue with multi-frame loop feeding in `QmlAVAudioIODevice`, eliminating audio crackles and buffer starvation.
- **Producer-Consumer Deadlock Prevention**: Added interrupt signaling across worker threads and waiting queues.
- **User-Isolated Lockfile**: Upgraded `SingleApplication` lockfile path from static `/tmp` to `$XDG_RUNTIME_DIR` with UID hashing for multi-user safety.
- **Clean Default Configuration**: Removed legacy third-party demo streams and pre-configured low-latency RTSP defaults.

[2.1.2]: https://github.com/ldl805/stream-matrix/compare/v2.1.1...v2.1.2
[2.1.1]: https://github.com/ldl805/stream-matrix/compare/v2.1.0...v2.1.1
[2.1.0]: https://github.com/ldl805/stream-matrix/compare/v2.0.0...v2.1.0
[2.0.0]: https://github.com/ldl805/stream-matrix/compare/v1.2.2...v2.0.0
[1.2.2]: https://github.com/ldl805/stream-matrix/compare/v1.2.0...v1.2.2
[1.2.0]: https://github.com/ldl805/stream-matrix/compare/v1.1.1...v1.2.0
[1.1.1]: https://github.com/ldl805/stream-matrix/compare/v1.1...v1.1.1
[1.1]: https://github.com/ldl805/stream-matrix/compare/v1.0.4...v1.1
[1.0.4]: https://github.com/ldl805/stream-matrix/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/ldl805/stream-matrix/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/ldl805/stream-matrix/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/ldl805/stream-matrix/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/ldl805/stream-matrix/releases/tag/v1.0.0
