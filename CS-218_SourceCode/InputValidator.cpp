#include "InputValidator.h"

#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include "PathUtil.h"
#include <iostream>
#include <system_error>

namespace {

// Keep console input and file names within portable, predictable limits.
const std::size_t kMaxFilenameLength = 255;
const std::size_t kMaxInputLength = 1024;
const char* const kInvalidFilenameChars = "<>\"|?*";

std::string trim(const std::string& text) {
    const std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

bool isReservedWindowsDeviceName(const std::string& component) {
    if (component.empty()) {
        return false;
    }

    // Windows reserves device names even when they have an extension.
    std::string lower;
    lower.reserve(component.size());
    for (unsigned char ch : component) {
        lower.push_back(static_cast<char>(std::tolower(ch)));
    }

    const std::size_t extension = lower.find('.');
    const std::string trimmed = trim(lower.substr(0, extension));
    if (trimmed.empty()) {
        return false;
    }

    const std::string deviceNames[] = {
        "con", "prn", "aux", "nul",
        "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
        "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
    };

    for (const std::string& name : deviceNames) {
        if (trimmed == name) {
            return true;
        }
    }

    return false;
}

bool parseInteger(const std::string& text, int minValue, int maxValue, int& value) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }

    int parsed = 0;
    const char* begin = trimmed.c_str();
    const char* end = begin + trimmed.size();
    // from_chars requires consuming the whole trimmed string, rejecting inputs
    // such as "12xyz" instead of accepting their numeric prefix.
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);

    if (ec != std::errc() || ptr != end) {
        return false;
    }
    if (parsed < minValue || parsed > maxValue) {
        return false;
    }

    value = parsed;
    return true;
}

} // namespace

bool InputValidator::isValidLineNumber(std::size_t lineNumber, std::size_t lineCount) {
    return lineNumber >= 1 && lineNumber <= lineCount;
}

bool InputValidator::isValidPosition(std::size_t column, std::size_t lineLength) {
    // A position equal to lineLength is valid: it is the end-of-line insertion point.
    return column <= lineLength;
}

bool InputValidator::isValidRange(std::size_t startColumn, std::size_t endColumn, std::size_t lineLength) {
    return startColumn < endColumn && endColumn <= lineLength;
}

bool InputValidator::isSearchStringEmpty(const std::string& pattern) {
    // Only a truly empty pattern is invalid: spaces are legitimate search text
    // (for example, replacing double spaces with a single one).
    return pattern.empty();
}

bool InputValidator::isDocumentEmpty(const TextDocument& document) {
    return document.isEmpty();
}

bool InputValidator::isValidFilename(const std::string& filename) {
    const std::string trimmed = trim(filename);
    if (trimmed.empty() || trimmed.size() > kMaxFilenameLength) {
        return false;
    }
    if (trimmed == "." || trimmed == ".." || trimmed == "/" || trimmed == "\\") {
        return false;
    }

    std::string normalized = trimmed;
    for (char& ch : normalized) {
        if (ch == '\\') ch = '/';
    }

    if (normalized.find_first_of(kInvalidFilenameChars) != std::string::npos) {
        return false;
    }

    // Parse drive, UNC, and rooted path prefixes before validating components.
    std::size_t start = 0;
    const bool hasDrivePrefix = normalized.size() >= 2 && normalized[1] == ':' &&
                                std::isalpha(static_cast<unsigned char>(normalized[0]));
    const bool hasUncPrefix = !hasDrivePrefix && normalized.size() >= 2 &&
                              normalized[0] == '/' && normalized[1] == '/';
    if (hasDrivePrefix) {
        if (normalized.size() == 2) {
            return false;
        }
        start = 2;
        if (normalized[start] != '/' && normalized[start] != '\\') {
            normalized.insert(start, "/");
        }
        ++start; // Skip the separator after the Windows drive prefix.
    } else if (hasUncPrefix) {
        start = 2;
        if (start >= normalized.size()) {
            return false;
        }
    }

    if (!hasUncPrefix && normalized.size() > 1 && normalized[0] == '/') {
        start = 1;
    }

    std::size_t componentCount = 0;
    std::size_t first = start;
    while (first < normalized.size()) {
        const std::size_t last = normalized.find('/', first);
        const std::size_t length = (last == std::string::npos) ? normalized.size() - first : last - first;
        const std::string component = normalized.substr(first, length);

        if (component.empty()) {
            return false;
        }
        if (component == "." || component == "..") {
            // Relative steps such as "../notes.txt" are fine; the LAST component
            // must be a real file name, not "." or "..".
            if (last == std::string::npos) return false;
            ++componentCount;
            first = last + 1;
            continue;
        }
        if (component.front() == ' ' || component.back() == ' ' || component.back() == '.') {
            return false;
        }
        if (isReservedWindowsDeviceName(component)) {
            return false;
        }
        ++componentCount;
        for (unsigned char ch : component) {
            if (ch < 32 || ch == 127) {
                return false;
            }
            if (ch == ':' || ch == '<' || ch == '>' || ch == '"' ||
                ch == '|' || ch == '?' || ch == '*') {
                return false;
            }
        }

        if (last == std::string::npos) {
            break;
        }
        first = last + 1;
    }

    return !hasUncPrefix || componentCount >= 2;
}

bool InputValidator::fileExists(const std::string& filename) {
    std::error_code error;
    return std::filesystem::is_regular_file(pathFromUtf8(filename), error);
}

bool InputValidator::validateMenuChoice(const std::string& input, int minChoice, int maxChoice, int& choice) {
    return parseInteger(input, minChoice, maxChoice, choice);
}

bool InputValidator::validateUserInput(const std::string& input) {
    if (input.size() > kMaxInputLength) {
        return false;
    }
    for (unsigned char c : input) {
        if (c < 32 || c == 127) {
            return false;
        }
    }
    return true;
}

bool InputValidator::readLine(const std::string& prompt, std::string& line) {
    std::cout << prompt;
    if (!std::getline(std::cin, line)) {
        return false; // EOF or stream error: caller should exit gracefully
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

bool InputValidator::readIntInRange(const std::string& prompt, int minValue, int maxValue, int& value) {
    std::string line;
    // Bound retries so invalid input cannot trap the caller in an endless prompt.
    constexpr int kMaxAttempts = 5;

    for (int attempts = 0; attempts < kMaxAttempts; ++attempts) {
        if (!readLine(prompt, line)) {
            return false;
        }
        if (validateUserInput(line) && parseInteger(line, minValue, maxValue, value)) {
            return true;
        }
        std::cout << "Invalid input. Please enter a number between "
                  << minValue << " and " << maxValue << ".\n";
    }

    std::cout << "Too many invalid attempts. Input cancelled.\n";
    return false;
}
