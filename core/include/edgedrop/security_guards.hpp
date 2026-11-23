#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <system_error>

namespace EdgeDrop {

class SecurityGuards {
public:
    static constexpr uint64_t kMaxFileSize = 2ULL * 1024 * 1024 * 1024; // 2 GiB

    // Validates input file path: exists, not empty, no nulls/controls,
    // strictly a regular file (never a symlink, FIFO, socket, device), and <= 2 GiB.
    // Returns empty string if valid, otherwise a user-friendly error message.
    static std::string validateInputPath(const std::filesystem::path& path);

    // Validates destination directory (exists, is regular dir, not a symlink).
    static std::string validateOutputDir(const std::filesystem::path& outputDir,
                                         const std::filesystem::path& inputPath);

    // Returns a safe sanitized filename without directory traversal or control characters.
    static std::string sanitizeFilename(const std::string& name);

    // Cryptographically secure random 128-bit hex string (32 hex characters).
    static std::string generateRandomHex128();

    // Verifies output file has non-zero size and matches expected magic bytes.
    static bool verifyOutputMagic(const std::filesystem::path& path, std::string_view expectedFormat);

    // Strips EXIF GPS coordinates, camera serial numbers, IPTC, and comment chunks from JPEG data.
    static void stripJpegPrivacyMetadata(std::vector<uint8_t>& jpegData);
};

// RAII Secure Temporary File
// Created with owner-only permissions (0600 on POSIX) and cryptographically random suffix.
// Destructor unconditionally unlinks the file if it was not successfully committed.
class SecureTempFile {
public:
    explicit SecureTempFile(const std::filesystem::path& destDir, const std::string& extension = "");
    ~SecureTempFile();

    SecureTempFile(const SecureTempFile&) = delete;
    SecureTempFile& operator=(const SecureTempFile&) = delete;

    SecureTempFile(SecureTempFile&& other) noexcept;
    SecureTempFile& operator=(SecureTempFile&& other) noexcept;

    bool isValid() const { return m_valid; }
    const std::filesystem::path& path() const { return m_path; }
    const std::string& errorMessage() const { return m_error; }

    // Validates file size > 0 and expected magic bytes, then atomically renames to finalPath.
    bool commit(const std::filesystem::path& finalPath, std::string_view expectedFormat = "");

    // Discards and deletes the temporary file immediately.
    void discard();

private:
    std::filesystem::path m_path;
    bool m_valid = false;
    bool m_committed = false;
    std::string m_error;
};

} // namespace EdgeDrop
