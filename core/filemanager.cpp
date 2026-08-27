#include "filemanager.h"
#include "filemanager_glob.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <system_error>

namespace fsw {

// =========================================================================
// Construction / destruction
// =========================================================================

FileManager::FileManager() = default;
FileManager::~FileManager() = default;

// =========================================================================
// Configuration
// =========================================================================

void FileManager::setLogLevel(LogLevel level) noexcept { logLevel_ = level; }

void FileManager::setThrowOnError(bool enabled) noexcept {
    throwOnError_ = enabled;
}

void FileManager::setInfoStream(std::ostream* stream) noexcept {
    infoStream_ = stream;
}

void FileManager::setErrorStream(std::ostream* stream) noexcept {
    errorStream_ = stream;
}

// =========================================================================
// Last-error introspection
// =========================================================================

bool FileManager::lastOperationSucceeded() const noexcept {
    return lastCode_ == ErrorCode::None;
}

ErrorCode FileManager::lastError() const noexcept { return lastCode_; }

const std::string& FileManager::lastErrorMessage() const noexcept {
    return lastMessage_;
}

const std::string& FileManager::lastErrorPath() const noexcept {
    return lastPath_;
}

// =========================================================================
// Internal helpers
// =========================================================================

void FileManager::recordError(ErrorCode code,
                              const std::string& message,
                              const std::string& path) {
    lastCode_    = code;
    lastMessage_ = message;
    lastPath_    = path;
    logError(message);
    if (throwOnError_) {
        throw FileError(code, message, path);
    }
}

void FileManager::recordSuccess() noexcept {
    lastCode_    = ErrorCode::None;
    lastMessage_.clear();
    lastPath_.clear();
}

void FileManager::logInfo(const std::string& message) const {
    if (static_cast<int>(logLevel_) < static_cast<int>(LogLevel::Info)) {
        return;
    }
    std::ostream& out = infoStream_ ? *infoStream_ : std::cout;
    out << message << '\n';
}

void FileManager::logError(const std::string& message) const {
    if (static_cast<int>(logLevel_) < static_cast<int>(LogLevel::Error)) {
        return;
    }
    std::ostream& out = errorStream_ ? *errorStream_ : std::cerr;
    out << "[fsw error] " << message << '\n';
}

ErrorCode FileManager::mapStdError(const std::error_code& ec) noexcept {
    if (!ec) return ErrorCode::None;

    // Map by errc value, which is portable across platforms.
    const auto cond = ec.default_error_condition();
    using std::errc;
    switch (static_cast<std::errc>(cond.value())) {
        case errc::no_such_file_or_directory: return ErrorCode::FileNotFound;
        case errc::permission_denied:         return ErrorCode::PermissionDenied;
        case errc::file_exists:               return ErrorCode::AlreadyExists;
        case errc::not_a_directory:           return ErrorCode::NotADirectory;
        case errc::is_a_directory:            return ErrorCode::NotAFile;
        case errc::directory_not_empty:       return ErrorCode::DirectoryNotEmpty;
        case errc::invalid_argument:          return ErrorCode::InvalidArgument;
        case errc::filename_too_long:         return ErrorCode::PathTooLong;
        case errc::no_space_on_device:        return ErrorCode::OutOfSpace;
        case errc::io_error:                  return ErrorCode::IOError;
        case errc::operation_not_supported:   return ErrorCode::Unsupported;
        default:                              return ErrorCode::Unknown;
    }
}

// =========================================================================
// File creation / writing
// =========================================================================

bool FileManager::createFile(const std::string& path,
                             const std::string& content) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "createFile: path must not be empty", path);
        return false;
    }

    // Open in binary mode so we write exactly what we are given, then write
    // the content as a sequence of bytes. This avoids platform-specific
    // newline translation that would otherwise occur in text mode.
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        recordError(ErrorCode::IOError,
                    "createFile: failed to open '" + path + "' for writing",
                    path);
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        recordError(ErrorCode::IOError,
                    "createFile: write error on '" + path + "'", path);
        return false;
    }

    file.flush();
    recordSuccess();
    logInfo("File created successfully: " + path);
    return true;
}

bool FileManager::writeFile(const std::string& path,
                            const std::string& content) {
    // writeFile is semantically identical to createFile but signals intent.
    return createFile(path, content);
}

bool FileManager::appendFile(const std::string& path,
                             const std::string& content) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "appendFile: path must not be empty", path);
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
        recordError(ErrorCode::IOError,
                    "appendFile: failed to open '" + path + "' for appending",
                    path);
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) {
        recordError(ErrorCode::IOError,
                    "appendFile: write error on '" + path + "'", path);
        return false;
    }

    file.flush();
    recordSuccess();
    logInfo("Content appended to: " + path);
    return true;
}

bool FileManager::writeFileBinary(const std::string& path,
                                  const std::vector<std::uint8_t>& data) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "writeFileBinary: path must not be empty", path);
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        recordError(ErrorCode::IOError,
                    "writeFileBinary: failed to open '" + path + "'",
                    path);
        return false;
    }

    if (!data.empty()) {
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }
    if (!file) {
        recordError(ErrorCode::IOError,
                    "writeFileBinary: write error on '" + path + "'", path);
        return false;
    }

    file.flush();
    recordSuccess();
    logInfo("Binary file written: " + path);
    return true;
}

bool FileManager::clearFile(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "clearFile: path must not be empty", path);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        recordError(ErrorCode::FileNotFound,
                    "clearFile: '" + path + "' does not exist", path);
        return false;
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        recordError(ErrorCode::NotAFile,
                    "clearFile: '" + path + "' is not a regular file", path);
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        recordError(ErrorCode::IOError,
                    "clearFile: failed to truncate '" + path + "'", path);
        return false;
    }

    recordSuccess();
    logInfo("File cleared: " + path);
    return true;
}

// =========================================================================
// File deletion / movement
// =========================================================================

bool FileManager::deleteFile(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "deleteFile: path must not be empty", path);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        // Idempotent: deleting a non-existent file is considered a success.
        recordSuccess();
        logInfo("File already absent (treated as deleted): " + path);
        return true;
    }

    if (!std::filesystem::is_regular_file(path, ec)) {
        recordError(ErrorCode::NotAFile,
                    "deleteFile: '" + path + "' is not a regular file", path);
        return false;
    }

    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) {
        const ErrorCode code = mapStdError(ec);
        recordError(code,
                    "deleteFile: " + ec.message() + " ('" + path + "')", path);
        return false;
    }

    recordSuccess();
    logInfo("File deleted successfully: " + path);
    return true;
}

bool FileManager::renameFile(const std::string& oldPath,
                             const std::string& newPath) {
    if (oldPath.empty() || newPath.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "renameFile: paths must not be empty", oldPath);
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        recordError(code,
                    "renameFile: " + ec.message() + " ('" + oldPath +
                        "' -> '" + newPath + "')",
                    oldPath);
        return false;
    }

    recordSuccess();
    logInfo("File renamed successfully: " + oldPath + " -> " + newPath);
    return true;
}

bool FileManager::moveFile(const std::string& src, const std::string& dst) {
    return renameFile(src, dst);
}

bool FileManager::copyFile(const std::string& src,
                           const std::string& dst,
                           bool overwrite) {
    if (src.empty() || dst.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "copyFile: paths must not be empty", src);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(src, ec)) {
        recordError(ErrorCode::FileNotFound,
                    "copyFile: source '" + src + "' does not exist", src);
        return false;
    }
    if (!std::filesystem::is_regular_file(src, ec)) {
        recordError(ErrorCode::NotAFile,
                    "copyFile: source '" + src + "' is not a regular file", src);
        return false;
    }

    if (!overwrite && std::filesystem::exists(dst, ec)) {
        recordError(ErrorCode::AlreadyExists,
                    "copyFile: destination '" + dst + "' already exists", dst);
        return false;
    }

    const auto options = overwrite
        ? std::filesystem::copy_options::overwrite_existing
        : std::filesystem::copy_options::none;
    std::filesystem::copy_file(src, dst, options, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        recordError(code,
                    "copyFile: " + ec.message() + " ('" + src + "' -> '" +
                        dst + "')",
                    src);
        return false;
    }

    recordSuccess();
    logInfo("File copied successfully: " + src + " -> " + dst);
    return true;
}

// =========================================================================
// File reading
// =========================================================================

Result<std::string> FileManager::readFile(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "readFile: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg = "readFile: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        const std::string msg = "readFile: '" + path + "' is not a regular file";
        recordError(ErrorCode::NotAFile, msg, path);
        return {ErrorCode::NotAFile, msg};
    }

    // Open in binary mode and seek to end to learn the size, then read the
    // whole file in one shot. This is significantly faster than getline().
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        const std::string msg = "readFile: failed to open '" + path + "'";
        recordError(ErrorCode::IOError, msg, path);
        return {ErrorCode::IOError, msg};
    }

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size < 0) {
        const std::string msg = "readFile: tellg failed on '" + path + "'";
        recordError(ErrorCode::IOError, msg, path);
        return {ErrorCode::IOError, msg};
    }
    file.seekg(0, std::ios::beg);

    std::string content(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        file.read(&content[0], size);
        if (!file) {
            const std::string msg =
                "readFile: read error on '" + path + "'";
            recordError(ErrorCode::IOError, msg, path);
            return {ErrorCode::IOError, msg};
        }
    }

    recordSuccess();
    logInfo("File read successfully: " + path);
    return {std::move(content)};
}

Result<std::vector<std::string>> FileManager::readLines(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "readLines: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::ifstream file(path);
    if (!file) {
        const std::string msg = "readLines: unable to open '" + path + "'";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
    }

    recordSuccess();
    logInfo("Lines read from: " + path);
    return {std::move(lines)};
}

Result<std::vector<std::uint8_t>>
FileManager::readFileBinary(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "readFileBinary: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg =
            "readFileBinary: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        const std::string msg =
            "readFileBinary: '" + path + "' is not a regular file";
        recordError(ErrorCode::NotAFile, msg, path);
        return {ErrorCode::NotAFile, msg};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        const std::string msg =
            "readFileBinary: failed to open '" + path + "'";
        recordError(ErrorCode::IOError, msg, path);
        return {ErrorCode::IOError, msg};
    }

    file.seekg(0, std::ios::end);
    const auto size = file.tellg();
    if (size < 0) {
        const std::string msg =
            "readFileBinary: tellg failed on '" + path + "'";
        recordError(ErrorCode::IOError, msg, path);
        return {ErrorCode::IOError, msg};
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    if (size > 0) {
        file.read(reinterpret_cast<char*>(data.data()), size);
        if (!file) {
            const std::string msg =
                "readFileBinary: read error on '" + path + "'";
            recordError(ErrorCode::IOError, msg, path);
            return {ErrorCode::IOError, msg};
        }
    }

    recordSuccess();
    logInfo("Binary file read: " + path);
    return {std::move(data)};
}

bool FileManager::printFile(const std::string& path, std::ostream& out) {
    auto result = readFile(path);
    if (!result.ok()) {
        return false;
    }
    out << result.value();
    out.flush();
    return true;
}

bool FileManager::printFile(const std::string& path) {
    return printFile(path, std::cout);
}

// =========================================================================
// File metadata
// =========================================================================

bool FileManager::exists(const std::string& path) noexcept {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool FileManager::fileExists(const std::string& path) noexcept {
    return exists(path);
}

bool FileManager::isFile(const std::string& path) noexcept {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

bool FileManager::isDirectory(const std::string& path) noexcept {
    if (path.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

bool FileManager::isEmpty(const std::string& path) noexcept {
    if (path.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return false;
    return std::filesystem::is_empty(path, ec);
}

Result<std::uintmax_t> FileManager::fileSize(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "fileSize: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg = "fileSize: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        const std::string msg =
            "fileSize: '" + path + "' is not a regular file";
        recordError(ErrorCode::NotAFile, msg, path);
        return {ErrorCode::NotAFile, msg};
    }

    const auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "fileSize: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    recordSuccess();
    logInfo("Size of " + path + ": " + std::to_string(size) + " bytes");
    return {size};
}

Result<std::filesystem::file_time_type>
FileManager::lastModified(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "lastModified: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg =
            "lastModified: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "lastModified: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    recordSuccess();
    return {time};
}

bool FileManager::setPermissions(const std::string& path,
                                 std::filesystem::perms perms) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "setPermissions: path must not be empty", path);
        return false;
    }

    std::error_code ec;
    std::filesystem::permissions(path, perms,
                                 std::filesystem::perm_options::replace, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "setPermissions: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return false;
    }

    recordSuccess();
    logInfo("Permissions updated: " + path);
    return true;
}

// =========================================================================
// Directory operations
// =========================================================================

bool FileManager::createDirectory(const std::string& path, bool recursive) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "createDirectory: path must not be empty", path);
        return false;
    }

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (std::filesystem::is_directory(path, ec)) {
            recordSuccess();
            logInfo("Directory already exists: " + path);
            return true;
        }
        recordError(ErrorCode::AlreadyExists,
                    "createDirectory: '" + path +
                        "' exists but is not a directory",
                    path);
        return false;
    }

    const bool ok = recursive
        ? std::filesystem::create_directories(path, ec)
        : std::filesystem::create_directory(path, ec);
    if (ec || !ok) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "createDirectory: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return false;
    }

    recordSuccess();
    logInfo("Directory created: " + path);
    return true;
}

bool FileManager::removeDirectory(const std::string& path, bool recursive) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "removeDirectory: path must not be empty", path);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        recordSuccess();
        logInfo("Directory already absent: " + path);
        return true;
    }
    if (!std::filesystem::is_directory(path, ec)) {
        recordError(ErrorCode::NotADirectory,
                    "removeDirectory: '" + path + "' is not a directory", path);
        return false;
    }

    if (recursive) {
        const auto removed = std::filesystem::remove_all(path, ec);
        if (ec) {
            const ErrorCode code = mapStdError(ec);
            const std::string msg =
                "removeDirectory: " + ec.message() + " ('" + path + "')";
            recordError(code, msg, path);
            return false;
        }
        recordSuccess();
        logInfo("Removed " + std::to_string(removed) + " entries under: " + path);
        return true;
    }

    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) {
        const ErrorCode code = ec ? mapStdError(ec) : ErrorCode::DirectoryNotEmpty;
        const std::string msg =
            ec ? ("removeDirectory: " + ec.message() + " ('" + path + "')")
               : ("removeDirectory: '" + path + "' is not empty");
        recordError(code, msg, path);
        return false;
    }

    recordSuccess();
    logInfo("Directory removed: " + path);
    return true;
}

Result<std::vector<std::string>>
FileManager::listDirectory(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "listDirectory: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg =
            "listDirectory: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }
    if (!std::filesystem::is_directory(path, ec)) {
        const std::string msg =
            "listDirectory: '" + path + "' is not a directory";
        recordError(ErrorCode::NotADirectory, msg, path);
        return {ErrorCode::NotADirectory, msg};
    }

    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
        entries.push_back(entry.path().filename().string());
    }
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "listDirectory: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    std::sort(entries.begin(), entries.end());
    recordSuccess();
    logInfo("Listed " + std::to_string(entries.size()) +
            " entries in: " + path);
    return {std::move(entries)};
}

Result<std::vector<std::string>>
FileManager::listDirectoryRecursive(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "listDirectoryRecursive: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) {
        const std::string msg =
            "listDirectoryRecursive: '" + path + "' is not a directory";
        recordError(ErrorCode::NotADirectory, msg, path);
        return {ErrorCode::NotADirectory, msg};
    }

    std::vector<std::string> entries;
    const auto base = std::filesystem::path(path);
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(path, ec)) {
        auto rel = std::filesystem::relative(entry.path(), base, ec);
        if (ec) {
            rel = entry.path();
            ec.clear();
        }
        entries.push_back(rel.string());
    }
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "listDirectoryRecursive: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    std::sort(entries.begin(), entries.end());
    recordSuccess();
    return {std::move(entries)};
}

// =========================================================================
// File statistics (text)
// =========================================================================

Result<std::size_t> FileManager::countLines(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "countLines: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::ifstream file(path);
    if (!file) {
        const std::string msg = "countLines: failed to open '" + path + "'";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    std::size_t total = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++total;
    }

    recordSuccess();
    logInfo("Total lines in " + path + ": " + std::to_string(total));
    return {total};
}

Result<std::size_t> FileManager::countWords(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "countWords: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::ifstream file(path);
    if (!file) {
        const std::string msg = "countWords: failed to open '" + path + "'";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    std::size_t total = 0;
    std::string word;
    while (file >> word) {
        ++total;
    }

    recordSuccess();
    logInfo("Total words in " + path + ": " + std::to_string(total));
    return {total};
}

Result<std::size_t> FileManager::countChars(const std::string& path,
                                            bool includeWhitespace) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "countChars: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::ifstream file(path);
    if (!file) {
        const std::string msg = "countChars: failed to open '" + path + "'";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    std::size_t total = 0;
    char ch;
    while (file.get(ch)) {
        if (includeWhitespace || !std::isspace(static_cast<unsigned char>(ch))) {
            ++total;
        }
    }

    recordSuccess();
    logInfo("Total chars in " + path + ": " + std::to_string(total));
    return {total};
}

// =========================================================================
// Search
// =========================================================================

Result<std::vector<std::string>>
FileManager::findFiles(const std::string& path, const std::string& pattern) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "findFiles: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec)) {
        const std::string msg =
            "findFiles: '" + path + "' is not a directory";
        recordError(ErrorCode::NotADirectory, msg, path);
        return {ErrorCode::NotADirectory, msg};
    }

    std::vector<std::string> matches;
    const auto base = std::filesystem::path(path);

    // Treat empty pattern as "*".
    const std::string& glob = pattern.empty()
        ? std::string{"*"}
        : pattern;

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(path, ec)) {
        const std::string name = entry.path().filename().string();
        if (globMatch(glob, name)) {
            auto rel = std::filesystem::relative(entry.path(), base, ec);
            if (ec) {
                rel = entry.path();
                ec.clear();
            }
            matches.push_back(rel.string());
        }
    }
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "findFiles: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    std::sort(matches.begin(), matches.end());
    recordSuccess();
    logInfo("Found " + std::to_string(matches.size()) +
            " matching entries under: " + path);
    return {std::move(matches)};
}

// =========================================================================
// Disk information
// =========================================================================

Result<std::uintmax_t> FileManager::diskUsage(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "diskUsage: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        const std::string msg = "diskUsage: '" + path + "' does not exist";
        recordError(ErrorCode::FileNotFound, msg, path);
        return {ErrorCode::FileNotFound, msg};
    }

    std::uintmax_t total = 0;
    if (std::filesystem::is_regular_file(path, ec)) {
        total = std::filesystem::file_size(path, ec);
        if (ec) {
            const ErrorCode code = mapStdError(ec);
            recordError(code, ec.message(), path);
            return {code, ec.message()};
        }
    } else if (std::filesystem::is_directory(path, ec)) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(path, ec)) {
            if (ec) break;
            if (entry.is_regular_file(ec)) {
                total += entry.file_size(ec);
            }
        }
        if (ec) {
            const ErrorCode code = mapStdError(ec);
            const std::string msg =
                "diskUsage: " + ec.message() + " ('" + path + "')";
            recordError(code, msg, path);
            return {code, msg};
        }
    } else {
        recordError(ErrorCode::Unsupported,
                    "diskUsage: '" + path + "' is neither file nor directory",
                    path);
        return {ErrorCode::Unsupported,
                "neither file nor directory"};
    }

    recordSuccess();
    logInfo("Disk usage of " + path + ": " + std::to_string(total) + " bytes");
    return {total};
}

Result<std::uintmax_t> FileManager::freeSpace(const std::string& path) {
    if (path.empty()) {
        recordError(ErrorCode::InvalidArgument,
                    "freeSpace: path must not be empty", path);
        return {ErrorCode::InvalidArgument, "path must not be empty"};
    }

    std::error_code ec;
    const auto info = std::filesystem::space(path, ec);
    if (ec) {
        const ErrorCode code = mapStdError(ec);
        const std::string msg =
            "freeSpace: " + ec.message() + " ('" + path + "')";
        recordError(code, msg, path);
        return {code, msg};
    }

    recordSuccess();
    logInfo("Free space on " + path + ": " +
            std::to_string(info.available) + " bytes");
    return {info.available};
}

// =========================================================================
// Path utilities
// =========================================================================

std::string FileManager::getFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string FileManager::getStem(const std::string& path) {
    return std::filesystem::path(path).stem().string();
}

std::string FileManager::getExtension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

std::string FileManager::getParentPath(const std::string& path) {
    return std::filesystem::path(path).parent_path().string();
}

std::string FileManager::joinPaths(const std::string& a, const std::string& b) {
    return (std::filesystem::path(a) / std::filesystem::path(b)).string();
}

std::string FileManager::getAbsolutePath(const std::string& path) {
    std::error_code ec;
    const auto result = std::filesystem::absolute(path, ec);
    if (ec) return path;
    return result.string();
}

std::string FileManager::normalizePath(const std::string& path) {
    return std::filesystem::path(path).lexically_normal().string();
}

// =========================================================================
// Backward-compatible aliases — definitions live inline in filemanager.h.
// =========================================================================

} // namespace fsw
