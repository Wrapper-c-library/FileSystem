#ifndef LIBRARY_CPP_WRAPPER_FILEMANAGER_H
#define LIBRARY_CPP_WRAPPER_FILEMANAGER_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iosfwd>
#include <string>
#include <vector>

#include "filemanager_errors.h"

namespace fsw {

/**
 * @brief Severity level for the messages emitted by FileManager.
 *
 * The level controls which messages are written to the configured output
 * streams. Errors are always reported when the level is at least Error.
 */
enum class LogLevel {
    /// Suppress all log output. Use this in libraries or non-interactive code.
    Silent = 0,
    /// Report only failures (default).
    Error = 1,
    /// Report both failures and successful operations.
    Info = 2
};

/**
 * @brief A small wrapper around the C++17 <filesystem> facilities that adds
 *        consistent error handling, configurable logging, and a richer set of
 *        operations than the raw standard library.
 *
 * # Design
 *
 * - Operations that produce a value return a Result<T>. Callers can use
 *   `if (result)` to test for success and `result.value()` to access the value.
 * - Operations that do not produce a value return a bool (true on success).
 * - Every operation updates an internal last-error slot that can be queried
 *   via lastError() / lastErrorMessage() / lastOperationSucceeded().
 * - If setThrowOnError(true) has been called, failed operations throw a
 *   FileError instead of returning false / a failed Result.
 * - Logging output goes to a configurable stream (std::cerr for errors,
 *   std::cout for info messages) and is filtered by the current LogLevel.
 *
 * # Thread safety
 *
 * A single FileManager instance is NOT thread-safe — the last-error slot and
 * configuration state are mutated without locking. Distinct instances may be
 * used concurrently from different threads.
 */
class FileManager {
public:
    /// Construct a FileManager with default settings (LogLevel::Error, no throw).
    FileManager();

    /// Destructor. Does not perform any filesystem operations.
    ~FileManager();

    FileManager(const FileManager&) = default;
    FileManager& operator=(const FileManager&) = default;
    FileManager(FileManager&&) noexcept = default;
    FileManager& operator=(FileManager&&) noexcept = default;

    // ----------------------------------------------------------------------
    // Configuration
    // ----------------------------------------------------------------------

    /**
     * @brief Set the verbosity of log output.
     * @param level The new log level.
     */
    void setLogLevel(LogLevel level) noexcept;

    /**
     * @brief Control whether failed operations throw FileError.
     *
     * When enabled, every failed operation raises a FileError instead of
     * returning false or a failed Result. The last-error slot is still
     * updated before the throw, so a catch handler can also read it.
     *
     * @param enabled true to throw, false to return error values (default).
     */
    void setThrowOnError(bool enabled) noexcept;

    /**
     * @brief Redirect the stream used for informational messages.
     *
     * Pass nullptr to restore the default (std::cout).
     */
    void setInfoStream(std::ostream* stream) noexcept;

    /**
     * @brief Redirect the stream used for error messages.
     *
     * Pass nullptr to restore the default (std::cerr).
     */
    void setErrorStream(std::ostream* stream) noexcept;

    // ----------------------------------------------------------------------
    // Last-error introspection
    // ----------------------------------------------------------------------

    /// @return true if the most recent operation completed successfully.
    bool lastOperationSucceeded() const noexcept;

    /// @return The error code produced by the most recent operation.
    ErrorCode lastError() const noexcept;

    /// @return The human-readable message produced by the most recent operation.
    const std::string& lastErrorMessage() const noexcept;

    /// @return The path associated with the most recent operation, if any.
    const std::string& lastErrorPath() const noexcept;

    // ----------------------------------------------------------------------
    // File creation / writing
    // ----------------------------------------------------------------------

    /**
     * @brief Create a new file and write @p content into it.
     *
     * If the file already exists its previous contents are replaced.
     * Intermediate directories are NOT created — use createDirectory() first
     * if the parent path does not yet exist.
     *
     * @param path    Path of the file to create.
     * @param content Text to write. May be empty to create an empty file.
     * @return true on success, false on failure (consult lastError()).
     */
    bool createFile(const std::string& path, const std::string& content = "");

    /**
     * @brief Overwrite the entire contents of @p path with @p content.
     *
     * Equivalent to createFile() but semantically clearer when the caller
     * knows the file already exists.
     */
    bool writeFile(const std::string& path, const std::string& content);

    /**
     * @brief Append @p content to the end of @p path, creating it if absent.
     *
     * @param path    Path of the file to append to.
     * @param content Text to append.
     * @return true on success, false on failure.
     */
    bool appendFile(const std::string& path, const std::string& content);

    /**
     * @brief Write @p data as binary into @p path, replacing any prior content.
     *
     * Use this overload for non-text data (images, serialized structures, …).
     *
     * @return true on success, false on failure.
     */
    bool writeFileBinary(const std::string& path,
                         const std::vector<std::uint8_t>& data);

    /**
     * @brief Truncate @p path to zero length without deleting it.
     *
     * Useful for clearing log files in-place, keeping any existing
     * permissions / ownership.
     *
     * @return true on success, false on failure.
     */
    bool clearFile(const std::string& path);

    // ----------------------------------------------------------------------
    // File deletion / movement
    // ----------------------------------------------------------------------

    /**
     * @brief Remove the file at @p path.
     *
     * Returns true if the file was removed OR if it did not exist in the first
     * place (idempotent). Returns false only on genuine errors such as
     * permission denied or path refers to a directory.
     *
     * @return true on success (or already-absent), false on failure.
     */
    bool deleteFile(const std::string& path);

    /**
     * @brief Rename or move a file or directory.
     *
     * If @p newPath is on a different filesystem than @p oldPath the file is
     * copied and the original deleted; this is handled transparently by
     * std::filesystem::rename.
     *
     * @return true on success, false on failure.
     */
    bool renameFile(const std::string& oldPath, const std::string& newPath);

    /**
     * @brief Move a file or directory.
     *
     * Semantically identical to renameFile() but mirrors the standard library
     * naming and is provided for readability.
     */
    bool moveFile(const std::string& src, const std::string& dst);

    /**
     * @brief Copy a file to a new location.
     *
     * @param src       Source file path.
     * @param dst       Destination file path.
     * @param overwrite If false and @p dst already exists the operation fails
     *                  with ErrorCode::AlreadyExists. If true (default) the
     *                  destination is silently replaced.
     * @return true on success, false on failure.
     */
    bool copyFile(const std::string& src,
                  const std::string& dst,
                  bool overwrite = true);

    // ----------------------------------------------------------------------
    // File reading
    // ----------------------------------------------------------------------

    /**
     * @brief Read the entire content of @p path as a UTF-8 / ASCII string.
     *
     * @return A Result holding the file content on success, or an error.
     */
    Result<std::string> readFile(const std::string& path);

    /**
     * @brief Read @p path line by line into a vector.
     *
     * The trailing newline of each line is NOT included. The final line is
     * included even if it does not end with a newline character.
     *
     * @return A Result holding the lines on success, or an error.
     */
    Result<std::vector<std::string>> readLines(const std::string& path);

    /**
     * @brief Read @p path as a raw byte buffer.
     *
     * Use this overload for non-text data.
     *
     * @return A Result holding the byte vector on success, or an error.
     */
    Result<std::vector<std::uint8_t>> readFileBinary(const std::string& path);

    /**
     * @brief Print the contents of @p path to an output stream.
     *
     * Equivalent to `cat` on the shell. The default output stream is
     * std::cout.
     *
     * @param path File to print.
     * @param out  Stream to write to (defaults to std::cout).
     * @return true on success, false on failure.
     */
    bool printFile(const std::string& path, std::ostream& out);

    /// @overload
    bool printFile(const std::string& path);

    // ----------------------------------------------------------------------
    // File metadata
    // ----------------------------------------------------------------------

    /**
     * @brief Check whether @p path refers to an existing filesystem object.
     *
     * @return true if the path exists, false otherwise. Returns false (and
     *         records no error) if the path is absent.
     */
    bool exists(const std::string& path) noexcept;

    /// Convenience alias for exists().
    bool fileExists(const std::string& path) noexcept;

    /// @return true if @p path exists and is a regular file.
    bool isFile(const std::string& path) noexcept;

    /// @return true if @p path exists and is a directory.
    bool isDirectory(const std::string& path) noexcept;

    /**
     * @brief Check whether @p path is empty.
     *
     * For a regular file, empty means zero bytes. For a directory, empty
     * means it contains no entries.
     *
     * @return true if the path exists and is empty, false otherwise.
     */
    bool isEmpty(const std::string& path) noexcept;

    /**
     * @brief Get the size of @p path in bytes.
     * @return A Result holding the size on success, or an error.
     */
    Result<std::uintmax_t> fileSize(const std::string& path);

    /**
     * @brief Get the last modification time of @p path.
     * @return A Result holding the time point on success, or an error.
     */
    Result<std::filesystem::file_time_type> lastModified(const std::string& path);

    /**
     * @brief Set the permissions of @p path.
     *
     * @param perms The new permission bits, e.g.
     *              `std::filesystem::perms::owner_read |
     *               std::filesystem::perms::owner_write`.
     * @return true on success, false on failure.
     */
    bool setPermissions(const std::string& path, std::filesystem::perms perms);

    // ----------------------------------------------------------------------
    // Directory operations
    // ----------------------------------------------------------------------

    /**
     * @brief Create a directory.
     *
     * @param path      Directory to create.
     * @param recursive If true (default), intermediate directories are created
     *                  as needed (equivalent to `mkdir -p`). If false, the
     *                  call fails when the parent does not exist.
     * @return true on success or when the directory already exists,
     *         false on failure.
     */
    bool createDirectory(const std::string& path, bool recursive = true);

    /**
     * @brief Remove a directory.
     *
     * @param path      Directory to remove.
     * @param recursive If true, the directory and ALL its contents are
     *                  removed (equivalent to `rm -rf`). Use with care.
     *                  If false (default), the directory must be empty.
     * @return true on success, false on failure.
     */
    bool removeDirectory(const std::string& path, bool recursive = false);

    /**
     * @brief List the immediate children of a directory.
     *
     * @return A Result holding a vector of entry names (not full paths).
     *         Returns an empty vector for an empty directory.
     */
    Result<std::vector<std::string>> listDirectory(const std::string& path);

    /**
     * @brief List a directory recursively (depth-first).
     *
     * Returned entries are relative to @p path and use the platform's native
     * path separator.
     *
     * @return A Result holding the relative paths of every descendant.
     */
    Result<std::vector<std::string>> listDirectoryRecursive(const std::string& path);

    // ----------------------------------------------------------------------
    // File statistics (text)
    // ----------------------------------------------------------------------

    /// @return A Result holding the number of lines in @p path.
    Result<std::size_t> countLines(const std::string& path);

    /**
     * @brief Count the number of whitespace-separated words in @p path.
     * @return A Result holding the word count.
     */
    Result<std::size_t> countWords(const std::string& path);

    /**
     * @brief Count the number of characters in @p path.
     *
     * @param includeWhitespace If true (default) whitespace is counted;
     *                          if false only non-whitespace characters are.
     * @return A Result holding the character count.
     */
    Result<std::size_t> countChars(const std::string& path,
                                   bool includeWhitespace = true);

    // ----------------------------------------------------------------------
    // Search
    // ----------------------------------------------------------------------

    /**
     * @brief Recursively find files whose name matches @p pattern.
     *
     * @param path    Root directory to search.
     * @param pattern Glob pattern (e.g. "*.cpp", "test_*.txt"). An empty
     *                pattern matches every entry.
     * @return A Result holding the list of matching relative paths.
     */
    Result<std::vector<std::string>> findFiles(const std::string& path,
                                               const std::string& pattern = "*");

    // ----------------------------------------------------------------------
    // Disk information
    // ----------------------------------------------------------------------

    /**
     * @brief Compute the total disk usage of @p path (recursive).
     *
     * For a file this is the file size. For a directory this is the sum of
     * the sizes of every regular file inside it, recursively.
     *
     * @return A Result holding the size in bytes.
     */
    Result<std::uintmax_t> diskUsage(const std::string& path);

    /**
     * @brief Get the free space available on the filesystem containing @p path.
     * @return A Result holding the number of free bytes.
     */
    Result<std::uintmax_t> freeSpace(const std::string& path);

    // ----------------------------------------------------------------------
    // Path utilities (static)
    // ----------------------------------------------------------------------

    /// @return The filename component of @p path (e.g. "a.txt" for "/x/a.txt").
    static std::string getFileName(const std::string& path);

    /// @return The stem component of @p path (e.g. "a" for "/x/a.txt").
    static std::string getStem(const std::string& path);

    /// @return The extension of @p path including the leading dot, if any.
    static std::string getExtension(const std::string& path);

    /// @return The parent directory of @p path, or "" if there is none.
    static std::string getParentPath(const std::string& path);

    /// @return @p a and @p b joined with the platform's native separator.
    static std::string joinPaths(const std::string& a, const std::string& b);

    /// @return The absolute (canonical) form of @p path.
    static std::string getAbsolutePath(const std::string& path);

    /// @return The lexically-normalized form of @p path (no trailing slash).
    static std::string normalizePath(const std::string& path);

    // ----------------------------------------------------------------------
    // Backward-compatible aliases (deprecated, will be removed in v2.0)
    // ----------------------------------------------------------------------

    /// @deprecated Use createFile() instead.
    inline void createFile_deprecated(const std::string& filename,
                                      const std::string& content) {
        createFile(filename, content);
    }

    /// @deprecated Use printFile() instead.
    inline void CatFile(const std::string& filename) {
        printFile(filename);
    }

    /// @deprecated Use countLines() instead.
    inline void TotalLinescode(const std::string& filename) {
        auto r = countLines(filename);
        if (r.ok()) {
            std::cout << "Total lines in " << filename << ": " << r.value()
                      << std::endl;
        }
    }

    /// @deprecated Use listDirectory() instead.
    inline void dirfile(const std::string& path) {
        auto r = listDirectory(path);
        if (!r.ok()) return;
        std::cout << "Contents of directory " << path << ":\n";
        for (const auto& name : r.value()) {
            std::cout << name << '\n';
        }
        std::cout.flush();
    }

    /// @deprecated Use fileSize() instead.
    inline void SizeFile(const std::string& filename) {
        auto r = fileSize(filename);
        if (r.ok()) {
            std::cout << "Size of " << filename << ": " << r.value() << " bytes"
                      << std::endl;
        }
    }

private:
    // Helper: record an error and possibly throw.
    void recordError(ErrorCode code,
                     const std::string& message,
                     const std::string& path);

    // Helper: record success and clear any prior error.
    void recordSuccess() noexcept;

    // Helper: write an info message respecting the current log level.
    void logInfo(const std::string& message) const;

    // Helper: write an error message respecting the current log level.
    void logError(const std::string& message) const;

    // Helper: map a std::error_code to our ErrorCode enum.
    static ErrorCode mapStdError(const std::error_code& ec) noexcept;

    LogLevel         logLevel_{LogLevel::Error};
    bool             throwOnError_{false};
    std::ostream*    infoStream_{nullptr};
    std::ostream*    errorStream_{nullptr};
    ErrorCode        lastCode_{ErrorCode::None};
    std::string      lastMessage_;
    std::string      lastPath_;
};

} // namespace fsw

// Backward-compatibility alias so existing user code keeps compiling.
// New code should use fsw::FileManager explicitly.
using FileManager = fsw::FileManager;

#endif // LIBRARY_CPP_WRAPPER_FILEMANAGER_H
