# Changelog

All notable changes to **StreamMatrix** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
