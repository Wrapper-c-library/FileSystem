#ifndef LIBRARY_CPP_WRAPPER_FILEMANAGER_ERRORS_H
#define LIBRARY_CPP_WRAPPER_FILEMANAGER_ERRORS_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include "filemanager_export.h"

namespace fsw {

/**
 * @brief Enumeration of all error codes that FileManager operations can report.
 *
 * The numeric value is stable across releases so user code can compare against
 * specific codes without relying on string matching.
 */
enum class ErrorCode : std::int32_t {
    /// Operation completed successfully.
    None = 0,
    /// The path does not refer to an existing filesystem object.
    FileNotFound = 1,
    /// The caller lacks the necessary permissions for the operation.
    PermissionDenied = 2,
    /// A file or directory already exists where one was not expected.
    AlreadyExists = 3,
    /// The path exists but is not a regular file.
    NotAFile = 4,
    /// The path exists but is not a directory.
    NotADirectory = 5,
    /// The directory is not empty and the operation requires it to be empty.
    DirectoryNotEmpty = 6,
    /// One or more arguments supplied to the operation were invalid.
    InvalidArgument = 7,
    /// A path component was too long for the underlying filesystem.
    PathTooLong = 8,
    /// The filesystem reported that no space is left on the device.
    OutOfSpace = 9,
    /// An input/output error was reported by the operating system.
    IOError = 10,
    /// The operation is not supported on this platform or filesystem.
    Unsupported = 11,
    /// An error occurred that does not fit any of the more specific codes.
    Unknown = 99
};

/**
 * @brief Convert an ErrorCode into a short human-readable label.
 *
 * The returned string is suitable for logging and is guaranteed to be stable
 * across releases. It contains no spaces, so it can also be used as a token
 * for programmatic matching.
 *
 * @param code The error code to translate.
 * @return A lowercase snake_case label, e.g. "file_not_found".
 */
inline const char* errorCodeLabel(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::None:              return "none";
        case ErrorCode::FileNotFound:      return "file_not_found";
        case ErrorCode::PermissionDenied:  return "permission_denied";
        case ErrorCode::AlreadyExists:     return "already_exists";
        case ErrorCode::NotAFile:          return "not_a_file";
        case ErrorCode::NotADirectory:     return "not_a_directory";
        case ErrorCode::DirectoryNotEmpty: return "directory_not_empty";
        case ErrorCode::InvalidArgument:   return "invalid_argument";
        case ErrorCode::PathTooLong:       return "path_too_long";
        case ErrorCode::OutOfSpace:        return "out_of_space";
        case ErrorCode::IOError:           return "io_error";
        case ErrorCode::Unsupported:       return "unsupported";
        case ErrorCode::Unknown:           return "unknown";
    }
    return "unknown";
}

/**
 * @brief Exception type thrown when FileManager is configured to throw on error
 *        (see FileManager::setThrowOnError) or when an unrecoverable condition
 *        is reached.
 *
 * The exception carries the same information that would otherwise be retrievable
 * from FileManager::lastError() / lastErrorMessage(), so callers can catch it
 * and inspect either the structured code or the human-readable message.
 */
class FSW_API FileError : public std::runtime_error {
public:
    /**
     * @brief Construct a FileError.
     *
     * @param code     Structured error code.
     * @param message  Human-readable detail message.
     * @param path     Filesystem path associated with the failure, if any.
     */
    FileError(ErrorCode code, const std::string& message, std::string path = "")
        : std::runtime_error(message),
          code_(code),
          path_(std::move(path)) {}

    /// @return The structured error code.
    ErrorCode code() const noexcept { return code_; }

    /// @return The filesystem path associated with the failure, if any.
    const std::string& path() const noexcept { return path_; }

private:
    ErrorCode code_;
    std::string path_;
};

/**
 * @brief A discriminated union holding either a value of type T or an error.
 *
 * Result is the primary return type for FileManager operations that produce a
 * value (such as readFile or fileSize). It allows the caller to choose between
 * inspecting the error explicitly or accessing the value with a fallback.
 *
 * Example usage:
 *
 * @code
 * auto content = fm.readFile("data.txt");
 * if (content.ok()) {
 *     std::cout << content.value() << '\n';
 * } else {
 *     std::cerr << "read failed: " << content.error() << '\n';
 * }
 * @endcode
 */
template <typename T>
class Result {
public:
    /// Construct a successful Result holding @p value.
    Result(T value) : value_(std::move(value)), code_(ErrorCode::None) {}

    /// Construct a failed Result holding the given error code and message.
    Result(ErrorCode code, std::string message)
        : code_(code), message_(std::move(message)) {}

    /// @return true if the Result holds a value, false if it holds an error.
    bool ok() const noexcept { return code_ == ErrorCode::None; }

    /// Boolean conversion matching ok() — enables `if (result) { ... }`.
    explicit operator bool() const noexcept { return ok(); }

    /// @return The structured error code.
    ErrorCode code() const noexcept { return code_; }

    /// @return The human-readable error message (empty when ok() is true).
    const std::string& error() const noexcept { return message_; }

    /**
     * @brief Access the stored value.
     *
     * Calling value() when ok() is false invokes undefined behavior. Prefer
     * checking ok() first, or use valueOr() for a safe accessor.
     */
    T& value() { return value_; }
    /// @copydoc value()
    const T& value() const { return value_; }

    /**
     * @brief Return the stored value if ok(), otherwise return @p fallback.
     *
     * This is the safe accessor — it never fails and is the recommended way to
     * consume a Result when a sensible default exists.
     */
    T valueOr(T fallback) const {
        return ok() ? value_ : fallback;
    }

private:
    T value_;
    ErrorCode code_;
    std::string message_;
};

} // namespace fsw

#endif // LIBRARY_CPP_WRAPPER_FILEMANAGER_ERRORS_H
