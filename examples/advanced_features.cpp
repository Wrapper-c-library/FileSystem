// ===========================================================================
// Advanced features example — demonstrates the richer API surface:
//
// - Result<T> for value-returning operations
// - throw-on-error mode
// - custom log streams and log levels
// - directory creation / recursive listing
// - glob-based file search
// - binary I/O
// - file metadata and permissions
// - disk usage and free space
// - path utilities
//
// Build (modular):
//   g++ -std=c++17 -Icore examples/advanced_features.cpp core/filemanager.cpp
//       -o advanced
// ===========================================================================

#include "filemanager.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace {

void printHeader(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

} // namespace

int main() {
    FileManager fm;

    // ---------------------------------------------------------------------
    // 1. Custom log level + custom error stream.
    //    LogLevel::Info makes FileManager echo every successful operation.
    // ---------------------------------------------------------------------
    std::ostringstream errorLog;
    fm.setLogLevel(fsw::LogLevel::Info);
    fm.setErrorStream(&errorLog);

    // ---------------------------------------------------------------------
    // 2. Directory creation (recursive, like mkdir -p).
    // ---------------------------------------------------------------------
    printHeader("Directory creation");
    fm.createDirectory("workspace/subdir1/subdir2", /*recursive=*/true);
    fm.createDirectory("workspace/subdir1/subdir3", /*recursive=*/true);

    // ---------------------------------------------------------------------
    // 3. Create some files for the search demo.
    // ---------------------------------------------------------------------
    printHeader("File creation");
    fm.createFile("workspace/subdir1/a.txt",   "alpha\n");
    fm.createFile("workspace/subdir1/b.cpp",   "int main(){return 0;}\n");
    fm.createFile("workspace/subdir1/subdir2/c.txt", "charlie\n");
    fm.createFile("workspace/subdir1/subdir2/d.cpp", "// d\n");
    fm.createFile("workspace/subdir1/subdir3/e.md",  "# e\n");

    // ---------------------------------------------------------------------
    // 4. Glob-based file search.
    // ---------------------------------------------------------------------
    printHeader("Find all .cpp files under workspace/");
    auto cppFiles = fm.findFiles("workspace", "*.cpp");
    if (cppFiles) {
        for (const auto& p : cppFiles.value()) {
            std::cout << "  " << p << '\n';
        }
    }

    // ---------------------------------------------------------------------
    // 5. Recursive directory listing.
    // ---------------------------------------------------------------------
    printHeader("Recursive listing of workspace/");
    auto entries = fm.listDirectoryRecursive("workspace");
    if (entries) {
        for (const auto& p : entries.value()) {
            std::cout << "  " << p << '\n';
        }
    }

    // ---------------------------------------------------------------------
    // 6. Binary I/O.
    // ---------------------------------------------------------------------
    printHeader("Binary file I/O");
    std::vector<std::uint8_t> bytes = {0x00, 0xFF, 0x42, 0x37, 0xDE, 0xAD};
    fm.writeFileBinary("workspace/binary.dat", bytes);
    auto readBack = fm.readFileBinary("workspace/binary.dat");
    if (readBack) {
        std::cout << "  Read back " << readBack.value().size() << " bytes:";
        for (auto b : readBack.value()) {
            std::cout << " 0x" << std::hex << std::setw(2)
                      << std::setfill('0') << static_cast<int>(b);
        }
        std::cout << std::dec << '\n';
    }

    // ---------------------------------------------------------------------
    // 7. File metadata + permissions.
    // ---------------------------------------------------------------------
    printHeader("File metadata");
    std::cout << "  isFile(a.txt):       "
              << (fm.isFile("workspace/subdir1/a.txt") ? "yes" : "no") << '\n';
    std::cout << "  isDirectory(subdir1):"
              << (fm.isDirectory("workspace/subdir1") ? "yes" : "no") << '\n';
    std::cout << "  fileSize(a.txt):     "
              << fm.fileSize("workspace/subdir1/a.txt").valueOr(0) << '\n';
    std::cout << "  lastModified exists: "
              << (fm.lastModified("workspace/subdir1/a.txt").ok() ? "yes" : "no")
              << '\n';
    fm.setPermissions("workspace/subdir1/a.txt",
                      std::filesystem::perms::owner_read |
                      std::filesystem::perms::owner_write);

    // ---------------------------------------------------------------------
    // 8. Disk usage + free space.
    // ---------------------------------------------------------------------
    printHeader("Disk information");
    std::cout << "  diskUsage(workspace): "
              << fm.diskUsage("workspace").valueOr(0) << " bytes\n";
    std::cout << "  freeSpace(.):         "
              << fm.freeSpace(".").valueOr(0) << " bytes\n";

    // ---------------------------------------------------------------------
    // 9. Path utilities (static).
    // ---------------------------------------------------------------------
    printHeader("Path utilities");
    std::cout << "  getFileName('/x/y/a.txt') = "
              << FileManager::getFileName("/x/y/a.txt") << '\n';
    std::cout << "  getStem('/x/y/a.txt')     = "
              << FileManager::getStem("/x/y/a.txt") << '\n';
    std::cout << "  getExtension('/x/y/a.txt')= "
              << FileManager::getExtension("/x/y/a.txt") << '\n';
    std::cout << "  getParentPath('/x/y/a.txt') = "
              << FileManager::getParentPath("/x/y/a.txt") << '\n';
    std::cout << "  joinPaths('a','b/c')      = "
              << FileManager::joinPaths("a", "b/c") << '\n';
    std::cout << "  normalizePath('a/./b//c/..') = "
              << FileManager::normalizePath("a/./b//c/..") << '\n';
    std::cout << "  getAbsolutePath('.')      = "
              << FileManager::getAbsolutePath(".") << '\n';

    // ---------------------------------------------------------------------
    // 10. Error handling: explicit Result inspection.
    // ---------------------------------------------------------------------
    printHeader("Error handling");
    auto missing = fm.readFile("nonexistent_file.txt");
    if (!missing) {
        std::cout << "  Expected failure caught via Result:\n"
                  << "    code:    " << errorCodeLabel(missing.code()) << '\n'
                  << "    message: " << missing.error() << '\n';
    }

    // Inspect last-error slot.
    std::cout << "  lastError:           "
              << errorCodeLabel(fm.lastError()) << '\n';
    std::cout << "  lastErrorMessage:    " << fm.lastErrorMessage() << '\n';

    // Dump anything the error stream received.
    if (!errorLog.str().empty()) {
        std::cout << "  captured error stream:\n" << errorLog.str();
    }

    // ---------------------------------------------------------------------
    // 11. Error handling: throw-on-error mode.
    // ---------------------------------------------------------------------
    printHeader("Throw-on-error mode");
    fm.setThrowOnError(true);
    try {
        fm.readFile("another_missing_file.txt");
    } catch (const fsw::FileError& ex) {
        std::cout << "  Caught FileError:\n"
                  << "    code:    " << errorCodeLabel(ex.code()) << '\n'
                  << "    what:    " << ex.what() << '\n'
                  << "    path:    " << ex.path() << '\n';
    }

    // ---------------------------------------------------------------------
    // 12. Backward-compatible aliases still work.
    // ---------------------------------------------------------------------
    printHeader("Backward-compatible aliases");
    fm.setThrowOnError(false);
    fm.CatFile("workspace/subdir1/a.txt");          // deprecated alias
    fm.TotalLinescode("workspace/subdir1/a.txt");   // deprecated alias
    fm.SizeFile("workspace/subdir1/a.txt");         // deprecated alias
    fm.dirfile("workspace/subdir1");                // deprecated alias

    // ---------------------------------------------------------------------
    // 13. Cleanup.
    // ---------------------------------------------------------------------
    printHeader("Cleanup");
    fm.removeDirectory("workspace", /*recursive=*/true);
    std::cout << "  workspace/ removed\n";

    return 0;
}
