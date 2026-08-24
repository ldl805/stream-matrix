# Contributing to StreamMatrix

Thank you for your interest in contributing to **StreamMatrix**!

## Getting Started

1. **Fork the repository** on GitHub.
2. **Clone your fork** locally with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/your-username/stream-matrix.git
   cd stream-matrix
   ```
3. **Create a topic branch**:
   ```bash
   git checkout -b feature/my-feature-name
   ```

## Development & Building

- Ensure dependencies are installed (Qt5/Qt6 base, QML/declarative, multimedia, FFmpeg libs).
- Build and run unit tests:
  ```bash
  cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
  cmake --build build -j$(nproc)
  ctest --test-dir build --output-on-failure
  ```

## Code Guidelines

- C++17 standard conventions.
- Follow existing formatting and indent styles in QML and C++ files.
- Ensure all multi-threaded interactions (e.g. between demuxer, decoder, and audio threads) are properly guarded and interrupt-safe.
- When adding new QML types or C++ interfaces, ensure unit tests or QML mock validations pass.

## Submitting Pull Requests

1. Commit your changes with clear, descriptive commit messages.
2. Push your topic branch to your GitHub fork.
3. Open a Pull Request against the `master` branch.
4. Describe what the PR does, testing steps, and relevant issue links.
