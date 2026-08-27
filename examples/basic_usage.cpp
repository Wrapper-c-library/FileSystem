// ===========================================================================
// Basic usage example — minimal walkthrough of the most common operations.
//
// Build (modular):
//   g++ -std=c++17 -Icore examples/basic_usage.cpp core/filemanager.cpp -o basic
//
// Build (header-only):
//   g++ -std=c++17 -Icore -DFSW_HEADER_ONLY examples/basic_usage.cpp -o basic
// ===========================================================================

#ifdef FSW_HEADER_ONLY
#  include "filemanager.hpp"
#else
#  include "filemanager.h"
#endif

#include <iostream>

int main() {
    FileManager fm;

    // Create a file and write some text into it.
    fm.createFile("hello.txt", "Hello from the FileManager library!\n");

    // Read it back and print.
    auto content = fm.readFile("hello.txt");
    if (content) {
        std::cout << "File content: " << content.value();
    }

    // Append a second line.
    fm.appendFile("hello.txt", "This line was appended later.\n");
    fm.printFile("hello.txt");

    // Count lines, words, and characters.
    std::cout << "Lines: " << fm.countLines("hello.txt").valueOr(0) << '\n';
    std::cout << "Words: " << fm.countWords("hello.txt").valueOr(0) << '\n';
    std::cout << "Chars: " << fm.countChars("hello.txt").valueOr(0) << '\n';

    // Get the file size.
    std::cout << "Size: " << fm.fileSize("hello.txt").valueOr(0) << " bytes\n";

    // Rename the file.
    fm.renameFile("hello.txt", "greeting.txt");

    // Clean up.
    fm.deleteFile("greeting.txt");

    return 0;
}
