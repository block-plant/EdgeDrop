# EdgeDrop

> **Zero-trust offline file compression at your screen edge.**

EdgeDrop is a native, privacy-first desktop utility for **macOS**, **Windows**, and **Linux** built with pure **C++20** and **Qt 6 Quick**. It lives as a discreet, floating pill docked against your display edge. When you drag a file toward the border—or click the handle—a card glides open, compresses your file locally in milliseconds with exact target-size control, and lets you drag the finished file directly into your browser or workflow without touching the cloud.

---

## A quick note before you open EdgeDrop

EdgeDrop is built by an independent creator and currently distributed as an unsigned, open-source binary. Because it isn’t signed with a paid Apple Developer or Microsoft certificate ($99–$499/yr), your operating system will show a standard first-launch warning:

- **macOS:** You may see *“EdgeDrop cannot be opened because Apple cannot check it for malicious software”* or *“unidentified developer.”*  
  → Right-click (or Control-click) `EdgeDrop.app`, choose **Open**, and click **Open** again.  
  → Or run in Terminal: `xattr -cr /Applications/EdgeDrop.app`

- **Windows:** SmartScreen may display *“Windows protected your PC.”*  
  → Click **“More info”** and then select **“Run anyway.”**

- **Linux:** Ensure execution permission is set:  
  → `chmod +x EdgeDrop-*.AppImage`

Every release is 100% open-source, reproducible, built on transparent GitHub Actions runners, and SHA-256 checksummed below. EdgeDrop makes zero network calls and runs entirely offline on your device.

---

## Architecture

EdgeDrop is decoupled into two independent layers:

```text
edgedrop/
├── core/                      # Pure C++20 engine (libedgedrop_core) — ZERO Qt headers
│   ├── include/edgedrop/      # Public API, format detector, router, security guards
│   └── src/                   # Image, PDF, Office, Media compressors + C ABI
└── desktop/                   # Native desktop application shell
    ├── src/                   # Window management, wallpaper sampling, IPC, subprocess runner
    └── qml/                   # Apple/Airbnb fluid UI (Metal / DirectX / Vulkan)
```

1. **`libedgedrop_core` (Pure C++20 Static Library):**  
   Zero Qt dependencies. Reusable across desktop and mobile (Android NDK / iOS). Implements format detection, compression algorithms, Zip Slip defenses, decompression bomb mitigations, and EXIF privacy stripping.
2. **`desktop` (Qt Quick Shell):**  
   Lightweight desktop shell with frameless Metal/DirectX rendering, dynamic wallpaper color grading, native drag-and-drop (`QDrag`), user-isolated IPC, and sandboxed subprocess execution.

---

## Key Features

- **100% Offline & Zero-Trust:** Zero network calls, zero telemetry, zero analytics, zero external API keys.
- **Magnetic Screen-Edge Notch:** Tucks neatly against your left or right screen border. Press and hold to slide along the edge, or let it smoothly retract when not in use.
- **Adaptive Wallpaper Aesthetic:** Automatically samples your desktop background color tone for an ambient glassmorphism look, or toggle between 4 curated vibes:
  - **Adaptive:** Dynamically derives surface tint from your current desktop wallpaper.
  - **Obsidian:** Deep OLED black with cool teal accents.
  - **Porcelain:** Clean Apple ceramic light surface with contrast borders.
  - **Mist:** Subtle slate dark surface with balanced contrast.
- **Privacy Stripping:** Automatically removes EXIF GPS coordinates, camera serial numbers, and software comments from exported JPEG and PNG images.
- **Single-Pass Smart PDF Engine:** Strips redundant structural streams and downsamples high-resolution embedded photos without degrading text legibility.
- **Office OOXML Optimizer:** Safely recompresses embedded images inside DOCX, PPTX, and XLSX archives with Zip Slip and Zip Bomb defenses.
- **Direct Drag-Out:** Drag compressed files directly from the result card into any web upload form or folder.

---

## Supported Formats

| Category | Supported Extensions | Engine / Method |
|---|---|---|
| **Images** | `.jpg`, `.jpeg`, `.png`, `.webp`, `.heic`, `.tiff`, `.bmp` | OpenCV 4 / lodepng / Native Codec |
| **Documents** | `.pdf` | QPDF / Ghostscript (Sandboxed) |
| **Office** | `.docx`, `.pptx`, `.xlsx` | Zip-deflate stream recompressor |
| **Media** | `.mp4`, `.mov`, `.mkv`, `.webm`, `.mp3` | FFmpeg / FFprobe (Sandboxed) |

---

## Defense-in-Depth Security

EdgeDrop enforces strict security controls at compile time and runtime:
- **Compiler Hardening:** `-fstack-protector-strong`, `_FORTIFY_SOURCE=2`, `-fPIE`, `-pie`, `/guard:cf`, `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`.
- **Zero-Trust Input Validation:** Rejection of symlinks, null-byte paths, 0-byte files, and files larger than 2 GiB.
- **Archive & Bomb Guards:** Maximum 4,096 entries, 150 MB single-entry cap, 500 MB total uncompressed cap, and 100:1 ratio limit. Raster images capped at 32,768 px / 100 MP.
- **RAII Secure Temporary Files (`SecureTempFile`):** Owner-only permissions (`0600`), 128-bit cryptographically secure random suffixes, format magic byte validation, and guaranteed cleanup on job cancellation.
- **Subprocess Sandboxing:** Strict directory allowlist (`/opt/homebrew/bin`, `/usr/bin`, `Program Files`), dynamic linker environment scrubbing (`LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`), and watchdog timeouts (120s / 300s).
- **IPC Isolation:** `QLocalServer` restricted to current user (`UserAccessOption`) with `EDGEDROP_WAKE_V1` handshake verification.

See [SECURITY.md](SECURITY.md) for full threat model details.

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

# 4. Run all 10 CTests
ctest --test-dir build --output-on-failure

# 5. Launch application
./build/edgedrop.app/Contents/MacOS/edgedrop
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

---

Completely offline. Zero tracking. Zero telemetry.
