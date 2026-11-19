#include "DisplayManager.h"
#include "TextDocument.h"

#include <climits>
#include <ios>
#include <iostream>
#include <stdexcept>

namespace {

// Detect output failures at the operation boundary instead of silently reporting success.
void requireOutputOk(const char* operation) {
    if (!std::cout) {
        throw std::ios_base::failure(std::string("DisplayManager::") + operation +
                                     ": output stream failed");
    }
}

void finishOutput(const char* operation) {
    // Flush so buffered write errors are reported before returning to the caller.
    std::cout.flush();
    requireOutputOk(operation);
}

} // namespace

DisplayManager::DisplayManager(const TextDocument& document)
    : document_(document) {}

int DisplayManager::countLines() const {
    // Guard the conversion because the display API returns int.
    if (document_.lineCount() > static_cast<std::size_t>(INT_MAX)) {
        throw std::overflow_error("DisplayManager::countLines: document too large for an int count");
    }
    return static_cast<int>(document_.lineCount());
}

int DisplayManager::countCharacters() const {
    // Count bytes in the flattened text, including line separators.
    const std::string text = document_.getAllText();
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::overflow_error("DisplayManager::countCharacters: document too large for an int count");
    }
    return static_cast<int>(text.size());
}

void DisplayManager::displayDocument() const {
    // Add a display-only final newline without changing the document.
    const std::string text = document_.getAllText();
    if (text.empty()) {
        std::cout << "(document is empty)\n";
    } else {
        std::cout << text;
        if (text.back() != '\n') std::cout << '\n';
    }
    finishOutput("displayDocument");
}

void DisplayManager::displayLine(int lineNumber) const {
    // Validate the public one-based line number before accessing the document.
    if (lineNumber < 1 || static_cast<std::size_t>(lineNumber) > document_.lineCount()) {
        std::cout << "Error: invalid line number " << lineNumber
                  << ". Valid range is 1 to " << countLines() << ".\n";
        finishOutput("displayLine");
        return;
    }
    std::cout << lineNumber << ": "
              << document_.getLine(static_cast<std::size_t>(lineNumber)) << '\n';
    finishOutput("displayLine");
}

void DisplayManager::displayAllLines() const {
    if (document_.lineCount() == 0) {
        std::cout << "(document is empty)\n";
        finishOutput("displayAllLines");
        return;
    }
    // Check each write so output failures are reported at the affected line.
    for (std::size_t line = 1; line <= document_.lineCount(); ++line) {
        std::cout << line << ": " << document_.getLine(line) << '\n';
        requireOutputOk("displayAllLines");
    }
    finishOutput("displayAllLines");
}

void DisplayManager::displayStatistics() const {
    // Use the checked counters so oversized values are reported explicitly.
    std::cout << "Lines: " << countLines()
              << ", Characters: " << countCharacters() << '\n';
    finishOutput("displayStatistics");
}
