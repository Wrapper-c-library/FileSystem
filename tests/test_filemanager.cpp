// ===========================================================================
// Lightweight test suite for FileManager.
//
// No external test framework required — assertions are implemented inline
// using a small CHECK macro. Exits with code 0 on success or 1 on failure.
//
// Build:
//   g++ -std=c++17 -Icore tests/test_filemanager.cpp core/filemanager.cpp
//       -o test_fm
// Run:
//   ./test_fm
// ===========================================================================

#include "filemanager.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

int g_checks_total     = 0;
int g_checks_failed    = 0;
std::string g_test_name;

void startTest(const std::string& name) {
    g_test_name = name;
    std::cout << "[ RUN      ] " << name << '\n';
}

#define CHECK(cond)                                                     \
    do {                                                                \
        ++g_checks_total;                                               \
        if (!(cond)) {                                                  \
            ++g_checks_failed;                                          \
            std::cout << "    FAIL: " << __FILE__ << ':' << __LINE__   \
                      << "  CHECK(" #cond ")  (test: "                  \
                      << g_test_name << ")\n";                         \
        }                                                               \
    } while (0)

template <typename T>
void checkResultOk(const fsw::Result<T>& r) {
    ++g_checks_total;
    if (!r.ok()) {
        ++g_checks_failed;
        std::cout << "    FAIL: result not ok (test: " << g_test_name
                  << ")  error: " << r.error() << '\n';
    }
}

void endTest() {
    std::cout << "[       OK ] " << g_test_name << '\n';
}

std::string scratchDir() {
    auto dir = std::filesystem::temp_directory_path() /
               ("fm_test_" + std::to_string(std::atoi(std::getenv("USER")
                                                      ? std::getenv("USER")
                                                      : "anon")));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir.string();
}

void cleanup(const std::string& dir) {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

// ---------------------------------------------------------------------------

void testCreateAndReadFile() {
    startTest("testCreateAndReadFile");
    FileManager fm;
    std::string path = scratchDir() + "/hello.txt";

    CHECK(fm.createFile(path, "hello world"));
    CHECK(fm.lastOperationSucceeded());
    CHECK(fm.exists(path));
    CHECK(fm.isFile(path));

    auto r = fm.readFile(path);
    checkResultOk(r);
    CHECK(r.value() == "hello world");

    cleanup(scratchDir());
    endTest();
}

void testAppendFile() {
    startTest("testAppendFile");
    FileManager fm;
    std::string path = scratchDir() + "/log.txt";

    CHECK(fm.createFile(path, "line1\n"));
    CHECK(fm.appendFile(path, "line2\n"));
    CHECK(fm.appendFile(path, "line3\n"));

    auto r = fm.readFile(path);
    checkResultOk(r);
    CHECK(r.value() == "line1\nline2\nline3\n");

    cleanup(scratchDir());
    endTest();
}

void testReadLines() {
    startTest("testReadLines");
    FileManager fm;
    std::string path = scratchDir() + "/lines.txt";

    CHECK(fm.createFile(path, "a\nb\nc"));
    auto r = fm.readLines(path);
    checkResultOk(r);
    CHECK(r.value().size() == 3);
    CHECK(r.value()[0] == "a");
    CHECK(r.value()[1] == "b");
    CHECK(r.value()[2] == "c");

    cleanup(scratchDir());
    endTest();
}

void testBinaryIO() {
    startTest("testBinaryIO");
    FileManager fm;
    std::string path = scratchDir() + "/data.bin";
    std::vector<std::uint8_t> bytes = {0x00, 0xFF, 0x42, 0x37, 0xDE, 0xAD};

    CHECK(fm.writeFileBinary(path, bytes));
    auto r = fm.readFileBinary(path);
    checkResultOk(r);
    CHECK(r.value() == bytes);

    cleanup(scratchDir());
    endTest();
}

void testDeleteFileIdempotent() {
    startTest("testDeleteFileIdempotent");
    FileManager fm;
    std::string path = scratchDir() + "/ghost.txt";

    // Deleting a non-existent file should still succeed (idempotent).
    CHECK(fm.deleteFile(path));
    CHECK(fm.lastOperationSucceeded());

    cleanup(scratchDir());
    endTest();
}

void testRenameAndCopy() {
    startTest("testRenameAndCopy");
    FileManager fm;
    std::string dir = scratchDir();
    std::string src = dir + "/src.txt";
    std::string dst = dir + "/dst.txt";

    CHECK(fm.createFile(src, "content"));
    CHECK(fm.copyFile(src, dst, /*overwrite=*/false));
    CHECK(fm.exists(src));
    CHECK(fm.exists(dst));

    // Copying again without overwrite should fail.
    CHECK(!fm.copyFile(src, dst, /*overwrite=*/false));
    CHECK(fm.lastError() == fsw::ErrorCode::AlreadyExists);

    // Renaming should move src.
    std::string renamed = dir + "/renamed.txt";
    CHECK(fm.renameFile(src, renamed));
    CHECK(!fm.exists(src));
    CHECK(fm.exists(renamed));

    cleanup(dir);
    endTest();
}

void testDirectoryOperations() {
    startTest("testDirectoryOperations");
    FileManager fm;
    std::string dir = scratchDir();
    std::string nested = dir + "/a/b/c";

    CHECK(fm.createDirectory(nested, /*recursive=*/true));
    CHECK(fm.isDirectory(nested));

    // Creating again should succeed (idempotent).
    CHECK(fm.createDirectory(nested, /*recursive=*/true));

    // List directory.
    fm.createFile(dir + "/a/b/c/file1.txt", "");
    fm.createFile(dir + "/a/b/c/file2.txt", "");
    auto listing = fm.listDirectory(dir + "/a/b/c");
    checkResultOk(listing);
    CHECK(listing.value().size() == 2);

    // Recursive remove.
    CHECK(fm.removeDirectory(dir + "/a", /*recursive=*/true));
    CHECK(!fm.exists(dir + "/a"));

    cleanup(dir);
    endTest();
}

void testListDirectoryRecursive() {
    startTest("testListDirectoryRecursive");
    FileManager fm;
    std::string dir = scratchDir();
    fm.createDirectory(dir + "/x/y/z");
    fm.createFile(dir + "/x/a.txt", "");
    fm.createFile(dir + "/x/y/b.txt", "");
    fm.createFile(dir + "/x/y/z/c.txt", "");

    auto r = fm.listDirectoryRecursive(dir + "/x");
    checkResultOk(r);
    CHECK(r.value().size() == 5);  // a.txt, y, y/b.txt, y/z, y/z/c.txt

    cleanup(dir);
    endTest();
}

void testFileStats() {
    startTest("testFileStats");
    FileManager fm;
    std::string path = scratchDir() + "/stats.txt";
    fm.createFile(path, "one two three\nfour five\nsix\n");

    CHECK(fm.countLines(path).valueOr(0) == 3);
    CHECK(fm.countWords(path).valueOr(0) == 6);
    CHECK(fm.countChars(path, true).valueOr(0) ==
          std::string("one two three\nfour five\nsix\n").size());
    // Without whitespace: 3+3+5+4+4+3 = 22 chars
    CHECK(fm.countChars(path, false).valueOr(0) == 22);

    cleanup(scratchDir());
    endTest();
}

void testFindFilesGlob() {
    startTest("testFindFilesGlob");
    FileManager fm;
    std::string dir = scratchDir();
    fm.createDirectory(dir + "/src");
    fm.createFile(dir + "/src/a.cpp", "");
    fm.createFile(dir + "/src/b.cpp", "");
    fm.createFile(dir + "/src/c.txt", "");
    fm.createFile(dir + "/src/d.hpp", "");

    auto cpp = fm.findFiles(dir + "/src", "*.cpp");
    checkResultOk(cpp);
    CHECK(cpp.value().size() == 2);

    auto all = fm.findFiles(dir + "/src", "*");
    checkResultOk(all);
    CHECK(all.value().size() == 4);

    cleanup(dir);
    endTest();
}

void testDiskUsage() {
    startTest("testDiskUsage");
    FileManager fm;
    std::string dir = scratchDir();
    fm.createFile(dir + "/f1.txt", "12345");  // 5 bytes
    fm.createFile(dir + "/f2.txt", "abc");    // 3 bytes

    auto r = fm.diskUsage(dir);
    checkResultOk(r);
    CHECK(r.value() == 8);

    auto fs = fm.freeSpace(dir);
    checkResultOk(fs);
    CHECK(fs.value() > 0);

    cleanup(dir);
    endTest();
}

void testPathUtilities() {
    startTest("testPathUtilities");
    using FM = FileManager;
    CHECK(FM::getFileName("/a/b/c.txt")    == "c.txt");
    CHECK(FM::getStem("/a/b/c.txt")        == "c");
    CHECK(FM::getExtension("/a/b/c.txt")   == ".txt");
    CHECK(FM::getParentPath("/a/b/c.txt")  == "/a/b");
    CHECK(FM::joinPaths("a", "b/c")        == "a/b/c");
    CHECK(FM::normalizePath("a/./b//c/..") == "a/b/");
    CHECK(!FM::getAbsolutePath(".").empty());
    endTest();
}

void testErrorHandling() {
    startTest("testErrorHandling");
    FileManager fm;

    auto r = fm.readFile("definitely_does_not_exist_xyz.txt");
    CHECK(!r.ok());
    CHECK(r.code() == fsw::ErrorCode::FileNotFound);
    CHECK(fm.lastError() == fsw::ErrorCode::FileNotFound);
    CHECK(!fm.lastErrorMessage().empty());

    // Empty-path argument should be InvalidArgument.
    CHECK(!fm.createFile("", "x"));
    CHECK(fm.lastError() == fsw::ErrorCode::InvalidArgument);

    endTest();
}

void testThrowOnError() {
    startTest("testThrowOnError");
    FileManager fm;
    fm.setThrowOnError(true);

    bool threw = false;
    try {
        fm.readFile("definitely_does_not_exist_xyz.txt");
    } catch (const fsw::FileError& ex) {
        threw = true;
        CHECK(ex.code() == fsw::ErrorCode::FileNotFound);
        CHECK(std::string(ex.what()).empty() == false);
    }
    CHECK(threw);
    endTest();
}

void testBackwardCompatAliases() {
    startTest("testBackwardCompatAliases");
    FileManager fm;
    std::string path = scratchDir() + "/legacy.txt";
    fm.createFile(path, "line1\nline2\n");

    // These deprecated methods still print to stdout. They should not crash.
    fm.CatFile(path);
    fm.TotalLinescode(path);
    fm.SizeFile(path);
    fm.dirfile(scratchDir());

    cleanup(scratchDir());
    endTest();
}

} // namespace

int main() {
    std::cout << "=== FileManager test suite ===\n";

    testCreateAndReadFile();
    testAppendFile();
    testReadLines();
    testBinaryIO();
    testDeleteFileIdempotent();
    testRenameAndCopy();
    testDirectoryOperations();
    testListDirectoryRecursive();
    testFileStats();
    testFindFilesGlob();
    testDiskUsage();
    testPathUtilities();
    testErrorHandling();
    testThrowOnError();
    testBackwardCompatAliases();

    std::cout << "\n=== Summary ===\n";
    std::cout << "  Checks: " << g_checks_total << '\n';
    std::cout << "  Failed: " << g_checks_failed << '\n';

    return g_checks_failed == 0 ? 0 : 1;
}
