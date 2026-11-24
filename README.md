# EdgeDrop

> **Zero-trust offline file compression at your screen edge.**

EdgeDrop is a native, privacy-first desktop utility for **macOS**, **Windows**, **Linux**, and **Android** built with pure **C++20** and **Qt 6 Quick**. It lives as a discreet, floating pill docked against your display edge. When you drag a file toward the border—or click the handle—a card glides open, compresses your file locally in milliseconds with exact target-size control, and lets you drag the finished file directly into your browser or workflow without touching the cloud.

---

## First-Launch Operating System Security Prompts

EdgeDrop is an independent open-source project and distributed without commercial code-signing certificates. Standard operating system warnings on first run are expected:

- **macOS (`.dmg`):**  
  If macOS displays *“EdgeDrop cannot be opened because Apple cannot check it for malicious software”*:  
  - Right-click (or Control-click) `EdgeDrop.app` in `/Applications`, choose **Open**, and click **Open** again.  
  - Or run in Terminal:  
    ```bash
    xattr -cr /Applications/EdgeDrop.app
    ```

- **Windows (`.exe`):**  
  If Windows SmartScreen displays *“Windows protected your PC”*:  
  - Click **“More info”** and then click **“Run anyway.”**

- **Linux (`.AppImage`):**  
  Mark the AppImage as executable before running:  
  ```bash
  chmod +x EdgeDrop-*.AppImage
  ./EdgeDrop-*.AppImage
  ```

- **Android (`.apk`):**  
  When installing the downloaded `.apk`:  
  - If prompted with *“Install unknown apps”*, tap **Settings** and toggle **Allow from this source**.

Every build is open-source, built via public GitHub Actions runners, and provided with SHA-256 checksums on GitHub Releases. EdgeDrop performs zero network calls and runs entirely offline on your device.

---

## Architecture

EdgeDrop is decoupled into modular layers:

```text
edgedrop/
├── core/                      # Pure C++20 engine (libedgedrop_core) — ZERO Qt headers
│   ├── include/edgedrop/      # Public API, format detector, router, security guards
│   └── src/                   # Image, PDF, Office, Media compressors + C ABI
├── desktop/                   # Native desktop application shell
│   ├── src/                   # Window management, wallpaper sampling, IPC, subprocess runner
│   └── qml/                   # Frameless fluid UI (Metal / DirectX / Vulkan)
├── android/                   # Native Android application (Kotlin + NDK/JNI)
├── docs/                      # Static release download website
└── tests/                     # Unit and integration test suite (CTest)
```

1. **`libedgedrop_core` (Pure C++20 Static Library):**  
   Zero Qt dependencies. Implements format detection, compression engines, Zip Slip defenses, decompression bomb mitigations, 64-bit file size tracking, and EXIF privacy stripping. Reused directly across desktop and mobile.
2. **`desktop` (Qt Quick Shell):**  
   Lightweight desktop shell with frameless rendering, dynamic wallpaper tone sampling, native drag-and-drop (`QDrag`), single-instance IPC, and sandboxed subprocess execution.
3. **`android` (Android App):**  
   Android application linking `libedgedrop_core` via JNI with edge gesture drawer integration.

---

## Supported Formats & System Dependencies

To keep installer sizes under 30 MB, EdgeDrop does not bundle heavy external transcoding binaries. Video and audio compression relies on local system FFmpeg.

| Category | Extensions | Engine / Dependency | Notes |
|---|---|---|---|
| **Images** | `.jpg`, `.jpeg`, `.png`, `.webp`, `.heic`, `.tiff`, `.bmp`, `.gif` | Built-in (OpenCV 4 / lodepng / libheif) | Bounded binary search, metadata stripping |
| **Documents** | `.pdf` | Built-in (libqpdf) + Optional Ghostscript | Structural cleanup, object streams, downsampling |
| **Office** | `.docx`, `.pptx`, `.xlsx` | Built-in (Zip-Deflate OOXML engine) | Media stream recompression with Zip Slip defense |
| **Video** | `.mp4`, `.mov`, `.mkv`, `.webm` | **System FFmpeg** (`ffmpeg` / `ffprobe`) | 2-pass rate control with strict container margins |
| **Audio** | `.mp3`, `.m4a`, `.aac`, `.ogg`, `.wav` | **System FFmpeg** (`ffmpeg`) | Sample rate & channel adaptive downmixing, WAV to M4A target transcoding |

### Installing FFmpeg (Required only for Video/Audio)

If you compress video or audio files, install FFmpeg on your system:

- **macOS:**  
  ```bash
  brew install ffmpeg
  ```
- **Windows:**  
  ```powershell
  winget install Gyan.FFmpeg
  ```
- **Linux (Debian/Ubuntu):**  
  ```bash
  sudo apt install ffmpeg
  ```

If FFmpeg is not found, EdgeDrop will allow compression of all images, PDFs, and Office files, and display a one-click copy command in the interface when a video or audio file is loaded.

---

## Defense-in-Depth Security

EdgeDrop enforces strict security controls at compile time and runtime:
- **Compiler Hardening:** `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, `-fPIE`, `-pie`, `/guard:cf`, `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`.
- **Zero-Trust Input Validation:** Rejection of symlinks, null-byte paths, 0-byte files, and files larger than 2 GiB. Full 64-bit (`uint64_t`, `qint64`) size accounting.
- **Archive & Bomb Guards:** Maximum 4,096 entries, 150 MB single-entry cap, 500 MB total uncompressed cap, and 100:1 ratio limit. Raster images capped at 32,768 px / 100 MP.
- **RAII Secure Temporary Files (`SecureTempFile`):** Owner-only permissions (`0600`), 128-bit cryptographically secure random suffixes, format magic byte validation, and guaranteed cleanup on job cancellation.
- **Subprocess Sandboxing:** Strict directory allowlist (`/opt/homebrew/bin`, `/usr/bin`, `Program Files`, WinGet), dynamic linker environment scrubbing (`LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`), and watchdog timeouts (120s / 300s).
- **IPC Isolation:** `QLocalServer` restricted to current user (`UserAccessOption`) with `EDGEDROP_WAKE_V1` handshake verification.

---

## Building from Source

### Prerequisites
- C++20 compiler: Clang 15+, GCC 12+, or MSVC 2022+
- CMake 3.22+
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Widgets`)
- OpenCV 4.8+
- QPDF 11+
- `nlohmann-json`

### macOS (Apple Silicon & Intel)
```bash
# 1. Install dependencies
brew install qt@6 opencv qpdf nlohmann-json cmake ninja

# 2. Configure build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DEDGEDROP_BUILD_TESTS=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix opencv);$(brew --prefix qpdf)"

# 3. Build application & test suite
cmake --build build --config Release

# 4. Run test suite
ctest --test-dir build --output-on-failure

# 5. Launch application
./build/desktop/edgedrop.app/Contents/MacOS/edgedrop
```

### Windows (MSVC 2022 + vcpkg)
```powershell
# 1. Install dependencies
vcpkg install opencv4 qpdf nlohmann-json --triplet x64-windows

# 2. Configure CMake
cmake -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DEDGEDROP_BUILD_TESTS=ON `
  -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

# 3. Build & test
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Linux (Ubuntu 22.04+)
```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build \
  qt6-base-dev qt6-declarative-dev qt6-quickcontrols2-5-dev \
  libopencv-dev libqpdf-dev nlohmann-json3-dev

# 2. Configure & build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DEDGEDROP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

### Android APK Build
```bash
cd android
./gradlew assembleRelease
```

---

Completely offline. Zero tracking. Zero telemetry.
Made with ❤️ by Ayush Kunwar.
