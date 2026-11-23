#include "edgedrop/security_guards.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cstring>
#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif

namespace EdgeDrop {

std::string SecurityGuards::validateInputPath(const std::filesystem::path& path) {
    std::string str = path.string();
    if (str.empty()) {
        return "Input file path is empty.";
    }

    // Check for null bytes and ASCII control characters (< 0x20)
    for (char c : str) {
        auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7F) {
            return "File path contains invalid control characters.";
        }
    }

    std::error_code ec;
    auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::exists(status)) {
        return "File does not exist: " + path.filename().string();
    }

    // Strictly reject symlinks to prevent symlink traversal attacks
    if (std::filesystem::is_symlink(status)) {
        return "Symlinks are rejected for security: " + path.filename().string();
    }

    // Strictly accept regular files only (reject directories, FIFOs, sockets, character/block devices)
    if (!std::filesystem::is_regular_file(status)) {
        return "Path is not a regular file (directories, devices, and pipes are rejected).";
    }

    auto fsize = std::filesystem::file_size(path, ec);
    if (ec) {
        return "Unable to determine file size.";
    }
    if (fsize == 0) {
        return "File is empty (0 bytes).";
    }
    if (fsize > kMaxFileSize) {
        return "File exceeds 2 GiB limit.";
    }

    return "";
}

std::string SecurityGuards::validateOutputDir(const std::filesystem::path& outputDir,
                                             const std::filesystem::path& inputPath) {
    if (outputDir.empty()) {
        return "Output directory is empty.";
    }

    std::error_code ec;
    auto status = std::filesystem::symlink_status(outputDir, ec);
    if (ec || !std::filesystem::exists(status)) {
        return "Output directory does not exist: " + outputDir.string();
    }
    if (std::filesystem::is_symlink(status)) {
        return "Output directory cannot be a symbolic link.";
    }
    if (!std::filesystem::is_directory(status)) {
        return "Output path is not a directory.";
    }

    // Guard against output directory being the exact same as input file path
    if (!inputPath.empty()) {
        bool equiv = std::filesystem::equivalent(outputDir, inputPath, ec);
        if (!ec && equiv) {
            return "Output directory cannot be identical to input file.";
        }
    }

    return "";
}

std::string SecurityGuards::sanitizeFilename(const std::string& name) {
    std::string clean;
    clean.reserve(name.size());
    for (char c : name) {
        auto uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7F || c == '/' || c == '\\' || c == ':' || c == '\0') {
            clean += '_';
        } else {
            clean += c;
        }
    }
    if (clean.empty() || clean == "." || clean == "..") {
        clean = "compressed_file";
    }
    return clean;
}

std::string SecurityGuards::generateRandomHex128() {
    uint8_t bytes[16];
    bool gotRandom = false;

#if defined(__APPLE__)
    arc4random_buf(bytes, sizeof(bytes));
    gotRandom = true;
#elif defined(_WIN32)
    if (BCryptGenRandom(NULL, bytes, sizeof(bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        gotRandom = true;
    }
#elif defined(__linux__)
    ssize_t ret = getrandom(bytes, sizeof(bytes), 0);
    if (ret == sizeof(bytes)) {
        gotRandom = true;
    }
#endif

    if (!gotRandom) {
#ifndef _WIN32
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            ssize_t r = read(fd, bytes, sizeof(bytes));
            close(fd);
            if (r == sizeof(bytes)) {
                gotRandom = true;
            }
        }
#endif
    }

    if (!gotRandom) {
        // Fallback to std::random_device
        std::random_device rd;
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            bytes[i] = static_cast<uint8_t>(rd());
        }
    }

    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

bool SecurityGuards::verifyOutputMagic(const std::filesystem::path& path, std::string_view expectedFormat) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) == 0) {
        return false;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    std::array<uint8_t, 16> buf{};
    f.read(reinterpret_cast<char*>(buf.data()), buf.size());
    size_t n = static_cast<size_t>(f.gcount());
    if (n < 3) return false;

    std::string fmt;
    for (char c : expectedFormat) {
        fmt += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (fmt == "jpeg" || fmt == "jpg") {
        return (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF);
    }
    if (fmt == "png") {
        return (n >= 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G' &&
                buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A);
    }
    if (fmt == "webp") {
        return (n >= 12 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
                buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P');
    }
    if (fmt == "pdf") {
        return (n >= 5 && buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' && buf[3] == 'F' && buf[4] == '-');
    }
    if (fmt == "docx" || fmt == "pptx" || fmt == "xlsx" || fmt == "zip") {
        return (buf[0] == 0x50 && buf[1] == 0x4B && buf[2] == 0x03 && buf[3] == 0x04);
    }
    if (fmt == "gif") {
        return (n >= 6 && buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == '8' &&
                (buf[4] == '7' || buf[4] == '9') && buf[5] == 'a');
    }

    // Default: allow non-empty file if format is unconstrained
    return true;
}

void SecurityGuards::stripJpegPrivacyMetadata(std::vector<uint8_t>& data) {
    if (data.size() < 4 || data[0] != 0xFF || data[1] != 0xD8) return;
    std::vector<uint8_t> clean;
    clean.reserve(data.size());
    clean.push_back(0xFF);
    clean.push_back(0xD8);

    size_t i = 2;
    while (i + 3 < data.size()) {
        if (data[i] != 0xFF) {
            clean.insert(clean.end(), data.begin() + i, data.end());
            break;
        }
        uint8_t marker = data[i + 1];
        if (marker == 0xD9 || marker == 0xDA) { // EOI or SOS (start of scan data)
            clean.insert(clean.end(), data.begin() + i, data.end());
            break;
        }
        if (marker == 0x00 || (marker >= 0xD0 && marker <= 0xD7)) {
            clean.push_back(data[i]);
            clean.push_back(data[i + 1]);
            i += 2;
            continue;
        }
        uint16_t len = (static_cast<uint16_t>(data[i + 2]) << 8) | static_cast<uint16_t>(data[i + 3]);
        if (i + 2 + len > data.size() || len < 2) {
            clean.insert(clean.end(), data.begin() + i, data.end());
            break;
        }

        // Strip APP1 (EXIF/XMP: 0xE1), APP13 (Photoshop/IPTC: 0xED), COM (Comment: 0xFE)
        if (marker == 0xE1 || marker == 0xED || marker == 0xFE) {
            i += 2 + len;
        } else {
            clean.insert(clean.end(), data.begin() + i, data.begin() + i + 2 + len);
            i += 2 + len;
        }
    }
    data = std::move(clean);
}

// ── SecureTempFile Implementation ─────────────────────────────────────────────

SecureTempFile::SecureTempFile(const std::filesystem::path& destDir, const std::string& extension) {
    std::string hex = SecurityGuards::generateRandomHex128();
    std::string ext = extension;
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    std::string filename = ".edgedrop.tmp." + hex + ext;
    m_path = destDir / filename;

#ifdef _WIN32
    HANDLE h = CreateFileA(m_path.string().c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL,
                           CREATE_NEW,
                           FILE_ATTRIBUTE_TEMPORARY,
                           NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        m_valid = true;
    } else {
        m_valid = false;
        m_error = "Failed to create secure temporary file on Windows.";
    }
#else
    // Owner-only read/write permissions (0600) + O_NOFOLLOW
    int fd = open(m_path.c_str(), O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW, 0600);
    if (fd >= 0) {
        close(fd);
        m_valid = true;
    } else {
        m_valid = false;
        m_error = "Failed to create secure temporary file: " + std::string(strerror(errno));
    }
#endif
}

SecureTempFile::~SecureTempFile() {
    if (!m_committed && !m_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }
}

SecureTempFile::SecureTempFile(SecureTempFile&& other) noexcept
    : m_path(std::move(other.m_path)),
      m_valid(other.m_valid),
      m_committed(other.m_committed),
      m_error(std::move(other.m_error)) {
    other.m_valid = false;
    other.m_committed = true; // prevent destruction of moved path
}

SecureTempFile& SecureTempFile::operator=(SecureTempFile&& other) noexcept {
    if (this != &other) {
        if (!m_committed && !m_path.empty()) {
            std::error_code ec;
            std::filesystem::remove(m_path, ec);
        }
        m_path = std::move(other.m_path);
        m_valid = other.m_valid;
        m_committed = other.m_committed;
        m_error = std::move(other.m_error);
        other.m_valid = false;
        other.m_committed = true;
    }
    return *this;
}

bool SecureTempFile::commit(const std::filesystem::path& finalPath, std::string_view expectedFormat) {
    if (!m_valid || m_path.empty()) {
        m_error = "Temporary file is invalid.";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(m_path, ec) || std::filesystem::file_size(m_path, ec) == 0) {
        m_error = "Temporary output is empty or missing.";
        discard();
        return false;
    }

    if (!expectedFormat.empty()) {
        if (!SecurityGuards::verifyOutputMagic(m_path, expectedFormat)) {
            m_error = "Temporary output does not match expected format magic bytes.";
            discard();
            return false;
        }
    }

    // Atomic rename
    std::filesystem::rename(m_path, finalPath, ec);
    if (ec) {
        m_error = "Atomic commit rename failed: " + ec.message();
        discard();
        return false;
    }

    m_committed = true;
    return true;
}

void SecureTempFile::discard() {
    if (!m_committed && !m_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
        m_path.clear();
        m_valid = false;
    }
}

} // namespace EdgeDrop
