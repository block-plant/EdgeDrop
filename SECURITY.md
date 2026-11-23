# EdgeDrop Security Policy & Architecture

EdgeDrop is architected with a strict **Zero-Trust, Local-First, Defense-in-Depth** security model. This document outlines our threat model, offline guarantees, attack surface mitigations, and vulnerability reporting procedures.

---

## 1. Zero-Network Security Guarantee
- **0 Network Sockets:** EdgeDrop makes **zero** outbound network requests. There is no telemetry, no analytics, no crash reporting pingbacks, and no auto-updater connecting to the internet.
- **Air-Gapped Operation:** All compression math and file handling execute strictly on your local machine in pure C++20.
- **Local IPC Isolation:** Single-instance coordination uses `QLocalServer` with `QLocalServer::UserAccessOption` (POSIX socket permissions `0600` or Windows user token ACLs), requiring a cryptographically authenticated token (`EDGEDROP_WAKE_V1`) to wake the UI.

---

## 2. Zero-Trust File & Parsing Defenses

### Path & Filesystem Traversal
- **Null-Byte & Control Character Rejection:** Input paths containing null bytes (`\0`) or ASCII control characters (< 32) are immediately rejected.
- **Symlink Blocking:** File inputs are checked with `std::filesystem::symlink_status()`. Symlinks are strictly rejected to prevent arbitrary local file reads.
- **Input Size Hard Cap:** Input files larger than 2 GiB or 0-byte files are rejected at the router entry point.
- **Zip Slip Defense:** Archive entries in Office documents (DOCX, PPTX, XLSX) are inspected before extraction. Paths containing `..`, absolute paths (leading `/` or `\`), drive letters (`C:`), or directory traversals are dropped and fail the job.

### Resource Exhaustion & Bomb Defenses
- **Zip Bomb Mitigation:** Office archive inflation imposes a maximum entry count of 4,096 files, a per-file uncompressed size cap of 150 MB, a total uncompressed cap of 500 MB, and a compression ratio limit of 100:1 for entries over 1 MB.
- **Image Decompression Bomb Defense:** Raster images with dimensions exceeding 32,768 pixels or greater than 100,000,000 pixels (100 MP) are blocked before memory allocation.
- **Watchdog Subprocess Timeouts:** External worker processes (`gs`, `ffmpeg`, `ffprobe`) are bounded by strict watchdog timers (120s for PDF, 300s for media) and terminated if they stall.

---

## 3. Privacy & Metadata Scrubbing
- **EXIF GPS Coordinates:** Stripped from exported JPEG and PNG images.
- **Device & Serial Identifiers:** Camera serial numbers, lens info, and device profiles are sanitized from exported images.
- **User Comments & IPTC:** JPEG COM chunks (`0xFFFE`) and IPTC segments (`0xFFE5`) are stripped directly at the byte level.

---

## 4. Subprocess Sandboxing & Execution Policy
When executing helper binaries (`gs`, `ffmpeg`, `ffprobe`):
- **Immutable Path Allowlist:** Executables are only loaded from trusted system paths (`/opt/homebrew/bin`, `/usr/local/bin`, `/usr/bin`, `/bin`, Windows `Program Files`, or the signed application bundle directory). CWD (`.`) and relative user paths are strictly rejected.
- **World-Writable Verification:** POSIX binaries are verified not to have world-writable permissions before execution.
- **Environment Scrubbing:** Critical dynamic linker and configuration variables (`LD_PRELOAD`, `LD_LIBRARY_PATH`, `DYLD_INSERT_LIBRARIES`, `DYLD_LIBRARY_PATH`, `GS_OPTIONS`, `GS_LIB`, `FFMPEG_DATADIR`, `PYTHONPATH`) are removed from subprocess environments.
- **Command Injection Prevention:** Canonical file arguments are passed with `--` separators and `file:` protocols to eliminate command-line switch hijacking (e.g., `-sDEVICE=...`).

---

## 5. Secure Temporary Files (`SecureTempFile`)
- All intermediate outputs are created via the `SecureTempFile` RAII wrapper.
- Uses POSIX `O_CREAT | O_EXCL | O_NOFOLLOW` with file permissions `0600` (owner-read/write only) and 128-bit cryptographically secure random suffixes (`arc4random_buf` / `BCryptGenRandom` / `getrandom`).
- Guaranteed cleanup: If a job fails or is cancelled, the file is unlinked automatically upon scope exit.
- Magic byte validation: Files are inspected for valid format magic bytes (`JPEG`, `PNG`, `WebP`, `PDF`, `ZIP`) before being atomically committed to their final destination.

---

## 6. Binary Hardening & Compiler Flags
Every release build is compiled with strict modern defense-in-depth security flags:
- **Stack Protection:** `-fstack-protector-strong` / `/GS`
- **Buffer Hardening:** `-D_FORTIFY_SOURCE=2`
- **Position Independent:** `-fPIE` / `-pie` / `/DYNAMICBASE`
- **Control Flow Guard:** `/guard:cf` (Windows)
- **Data Execution Prevention:** `/NXCOMPAT`
- **High Entropy ASLR:** `/HIGHENTROPYVA`
- **Format String Protection:** `-Wformat=2 -Werror=format-security`
- **Path Stripping:** `-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.` to eliminate build machine paths from release binaries.

---

## 7. Reporting a Vulnerability

If you discover a security vulnerability in EdgeDrop, please report it privately:
- Email: **security@edgedrop.app** (or via GitHub Private Vulnerability Reporting).
- Please provide a detailed description of the issue, reproduction steps, and proof-of-concept payload if applicable.
- We commit to acknowledging receipt within 48 hours and issuing a patch release promptly.
