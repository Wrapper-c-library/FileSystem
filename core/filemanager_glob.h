// Simple glob matcher supporting '*' (any sequence) and '?' (any single char).
// Other characters match literally. Used by FileManager::findFiles.
#pragma once

#include <string>

namespace fsw {

inline bool globMatch(const std::string& pattern, const std::string& text) {
    const char* p = pattern.c_str();
    const char* t = text.c_str();
    const char* star_p = nullptr;
    const char* star_t = nullptr;

    while (*t) {
        if (*p == *t || *p == '?') {
            ++p; ++t;
        } else if (*p == '*') {
            star_p = p++;
            star_t = t;
        } else if (star_p != nullptr) {
            p = star_p + 1;
            t = ++star_t;
        } else {
            return false;
        }
    }
    while (*p == '*') ++p;
    return *p == '\0';
}

} // namespace fsw
