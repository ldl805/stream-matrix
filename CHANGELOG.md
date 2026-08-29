# Changelog

All notable changes to **StreamMatrix** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2026-08-29

### Fixed
- **Shutdown and Quit Execution**: Connected QML engine `quit` signal to `QGuiApplication::quit` and added window `onClosing` hook, resolving issues where the app ignored quit shortcuts and menu actions.
- **Worker Thread Deadlock Prevention**: Fixed `requestInterrupt` across decoder waiting queues to properly wake blocked producer threads during shutdown and feed cancellation.
- **Demuxer Loop Flood on Failure**: Corrected decode error propagation in the demuxer loop to prevent tight infinite loop spinning and event queue saturation.
- **Thread Concurrency in `wait()`**: Synchronized worker thread completion waiting on condition variables to prevent concurrent `join()` data races.
- **Video Buffer Plane Count**: Ensured planar video buffer returns accurate active plane counts to prevent out-of-bounds plane sampling in Qt Quick rendering.
- **Context Double-Free**: Added pointer safety in `Context` destructor.

## [1.0.0] - 2026-08-24

### Initial Release (Independent Modernized Fork of CCTV Viewer)

#### Added
- **Live Stream Diagnostics Overlay (HUD)**: Press <kbd>D</kbd> to inspect real-time FPS, Resolution, Codec, Hardware Acceleration indicator `[HW]`, and Bitrate.
- **Automated Reconnect Watchdog**: Background watchdog with exponential backoff on dropped feeds and interactive on-screen "Retry" action.
- **Dual-Profile Sub-Stream Switching**: Support for defining high-res `URL` and low-bandwidth `Sub-Stream URL` per camera. The grid automatically renders sub-streams in multi-view and elevates to high resolution upon entering full-screen.
- **Smart Wayland / X11 Auto-Detection**: Launcher script (`run.sh`) that dynamically selects `wayland-0` / `:0` and configures `QT_QPA_PLATFORM="wayland;xcb"`.
- **Quote-Aware Option Tokenizer**: Robust argument parsing supporting quoted strings and flags with complex parameters.
- **Camera Naming**: Ability to assign custom camera names to each viewport.

#### Changed
- **Cached `SwsContext` Scaling**: Replaced per-frame memory allocation/free cycles with persistent context caching using `SWS_FAST_BILINEAR`.
- **Thread-Safe Continuous Audio Playback**: Implemented mutex-protected audio frame queue with multi-frame loop feeding in `QmlAVAudioIODevice`, eliminating crackles and buffer starvation.
- **Producer-Consumer Deadlock Prevention**: Added interrupt signaling across worker threads and waiting queues.
- **User-Isolated Lockfile**: Upgraded `SingleApplication` lockfile from static `/tmp` to `$XDG_RUNTIME_DIR` with UID hashing.
- **Default FFmpeg Stream Config**: Pre-configured low-latency defaults (`-hwaccel drm -rtsp_transport tcp -fflags nobuffer -flags low_delay`).
- **Removed Hardcoded Fallback Streams**: Removed third-party Russian demo streams to provide a clean slate for user configurations.
