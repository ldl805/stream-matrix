# Changelog

All notable changes to **StreamMatrix** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.1] - 2026-10-04

### Changed
- **Multi-Threaded Frame Decoding**: Ensured multi-threaded slice and frame decoding (`FF_THREAD_FRAME`) is actively utilized by filtering out conflicting `low_delay` flags during video decoder initialization, significantly improving multi-core CPU utilization on devices like Raspberry Pi 5.
- **Keyframe-Aware Overflow Protection**: Refactored packet waiting queue to use `std::deque` with keyframe-aware dropping (`AV_PKT_FLAG_KEY`), preventing video corruption and visual smearing when network packet buffers overflow.
- **Latency Backlog Reduction**: Reduced video packet buffer limit to ~2 seconds of backlog (64 packets), allowing streams that temporarily lag to rapidly catch up and resynchronize to live playback.
- **Optimized RTSP Startup & Probe Durations**: Set stream analysis duration to 1 second (`analyzeduration: 1000000`) and probe size to 500 KB (`probesize: 500000`), cutting initial connection latency and reconnect time from FFmpeg's 5-second default down to ~1 second.
- **Cleaned Obsolete Network Flags**: Removed deprecated `stimeout` and UDP-only `buffer_size` from default connection parameters.

## [1.1] - 2026-10-04

### Added
- **Interactive Reconnect Confirmation**: Instant tactile and visual feedback on the "Retry Now" button: hover highlights, pointing hand cursor, smooth scale compression, active `"Retrying..."` working state with animated spinner, and click debouncing.
- **Live Reconnection Status Updates**: Real-time status messaging during reconnection attempts, displaying active phases (`"Connecting (attempt N)..."`, `"Attempting connection to stream..."`) and live second-by-second countdowns (`"Retrying in Xs (attempt N)..."`) during exponential backoff.
- **Detailed Socket & FFmpeg Error Diagnostics**: Captured low-level socket, protocol, and timeout errors (e.g. `"Connection refused"`, `"Connection timed out"`, `"Server returned 404 Not Found"`, `"No route to host"`), displayed directly on the stream error overlay.
- **New Blue Icon Branding**: Redesigned application icon and branding with a sleek blue color palette matching the StreamMatrix interface styling.

### Fixed
- **Reconnection State Preservation**: Fixed an issue where manual retry destroyed the reconnection overlay prematurely and left a blank screen; retry attempts now cleanly preserve reconnect state until live video frames arrive.

## [1.0.4] - 2026-10-02

### Added
- **Flatpak & Flathub Packaging**: Added Flatpak manifest `org.streammatrix.StreamMatrix.yaml` targeting KDE Application Platform 5.15-25.08 with sandboxed network, Wayland/X11 display, PulseAudio/PipeWire audio, and DRI hardware acceleration.
- **AppStream 1.0 Metadata**: Added validated `org.streammatrix.StreamMatrix.metainfo.xml` for Linux software centers.
- **Freedesktop Desktop Entry**: Added `org.streammatrix.StreamMatrix.desktop` and scalable hicolor icon installation rules.

### Fixed
- **Sidebar GitHub Link**: Updated repository link on the sidebar logo from upstream cctv-viewer to `ldl805/stream-matrix`.

## [1.0.3] - 2026-10-01

### Fixed
- **RTSP Demuxer Queue Stalls & Choppiness**: Added `dropOnOverflow` ring buffer mode to `QmlAVWaitingQueue` so the demuxer thread drops the oldest unrendered packet rather than blocking when video processing experiences transient slowdowns. Increased default video packet queue capacity from 128 to 256 packets and frame buffer from 16 to 24 frames.
- **RTSP 404/453 Reconnection Failures**: Added immediate `avformat_close_input` in `QmlAVDemuxer` destructor to promptly send RTSP `TEARDOWN` and release server sockets before creating a new connection, preventing cameras from rejecting reconnect attempts due to session exhaustion.
- **Single-Packet Lockstep Decode Jitter**: Restructured the real-time decoding loop in `QmlAVDecoder` to eagerly drain all decoded frames immediately after sending a packet (`avcodec_receive_frame` loop) instead of waiting for the subsequent packet.
- **Multi-Threaded H.264 Software Decoding on Pi 5 / ARM**: Configured FFmpeg multi-threaded slice and frame decoding (`thread_count = 2`, `thread_type = FF_THREAD_SLICE | FF_THREAD_FRAME`) for software H.264 streams, utilizing Raspberry Pi 5 quad-core ARM NEON acceleration and preventing core saturation.
- **Frozen Stream Auto-Recovery**: Integrated a 7-second stalled stream watchdog in `QmlAVPlayer::updateMetrics()` that triggers an automatic soft reconnect if no new video frames arrive from an active stream.
- **Unbounded Audio Queue Memory Growth**: Capped `m_frames` queue in `QmlAVAudioIODevice` to 50 frames and avoided enqueuing audio buffers when muted (`volume <= 0.0`), preventing memory leaks on video-only monitoring feeds.
- **FFmpeg 7.x Channel Layout Memory Leak**: Added explicit `av_channel_layout_uninit` in `QmlAVResampler` destructor and prior to layout copies.
- **Stale Context Pointer Segfaults**: Cached stream properties (`timeBase`, `startTime`, `sampleAspectRatio`) in `QmlAVDecoder` so PTS calculations in `QmlAVFrame` do not dereference `AVStream` pointers after demuxer teardown.
- **Default FFmpeg Network Options**: Updated default RTSP network parameters with 5-second socket timeouts (`-stimeout 5000000 -timeout 5000000`) and a 1MB socket buffer (`-buffer_size 1048576`), removing invalid hardware accel flags for CPU-decoded H.264.

### Added
- **Automated GitHub Releases & Multi-Arch Packaging**: Added CPack support generating Debian `.deb` and `.tar.gz` distribution packages with architecture identification (`arm64`, `x86_64`), and added `.github/workflows/release.yml` for automated GitHub Releases on version tags.
- **CMake Test Integration**: Enabled CTest at repository root so all unit tests run via `ctest --test-dir build`.

## [1.0.2] - 2026-09-16

### Fixed
- **Global Key Event Interception for Quit Shortcuts**: Implemented `QuitShortcutFilter` installed at `QGuiApplication` level in C++ to intercept quit keystrokes (`Ctrl+Q`, `Ctrl+W`, `Ctrl+C`, `Alt+F4`, and `q`/`Q` outside text input controls) before any UI item can consume them, regardless of window focus state or Wayland/X11 surface hierarchy.
- **QML Key Propagation**: Fixed `Keys.onPressed` in `ViewportsLayout.qml` (viewport item level and layout root) and `SideBar.qml` which were implicitly accepting all key events (`event.accepted = true` by default) and blocking shortcuts from receiving events whenever viewports or sidebar had active focus.
- **Application-Scoped QML Shortcuts**: Upgraded shortcuts in `RootWindow.qml` to `Qt.ApplicationShortcut` context to ensure shortcuts trigger across the whole application lifecycle, and expanded quit sequences to include `Ctrl+Q`, `Ctrl+W`, `Ctrl+C`, `Alt+F4`, `q`, `Q`, `StandardKey.Quit`, and `StandardKey.Close`.
- **POSIX Signal Handling**: Added async-signal-safe Unix signal socketpair handling for `SIGINT` (terminal Ctrl+C) and `SIGTERM` ensuring clean event-loop shutdown and resource cleanup.

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
