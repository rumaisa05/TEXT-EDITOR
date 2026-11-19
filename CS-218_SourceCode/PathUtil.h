#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <filesystem>
#include <string>

// Converts UTF-8 file names to the path format expected by the platform.
inline std::filesystem::path pathFromUtf8(const std::string& name) {
#ifdef _WIN32
    return std::filesystem::u8path(name);
#else
    return std::filesystem::path(name);
#endif
}

#endif
