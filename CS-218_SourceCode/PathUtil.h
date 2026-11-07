#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <filesystem>
#include <string>

// File names travel through the editor as UTF-8 std::string values (that is
// what Qt gives us). On Windows a narrow std::string passed to the standard
// library is read in the ANSI code page instead, which breaks non-ASCII
// names, so every file access goes through this conversion first.
inline std::filesystem::path pathFromUtf8(const std::string& name) {
#ifdef _WIN32
    return std::filesystem::u8path(name);
#else
    return std::filesystem::path(name);
#endif
}

#endif
