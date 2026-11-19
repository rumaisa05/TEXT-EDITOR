#ifndef INPUT_VALIDATOR_H
#define INPUT_VALIDATOR_H

#include <cstddef>
#include <string>
#include "TextDocument.h"

// Centralizes validation for document positions, text, file paths, and console input.
class InputValidator {
public:
    // Line numbers are 1-based; columns are 0-based.
    static bool isValidLineNumber(std::size_t lineNumber, std::size_t lineCount);
    static bool isValidPosition(std::size_t column, std::size_t lineLength);
    static bool isValidRange(std::size_t startColumn, std::size_t endColumn, std::size_t lineLength);

    static bool isSearchStringEmpty(const std::string& pattern);
    static bool isDocumentEmpty(const TextDocument& document);

    static bool isValidFilename(const std::string& filename);
    static bool fileExists(const std::string& filename);

    static bool validateMenuChoice(const std::string& input, int minChoice, int maxChoice, int& choice);
    static bool validateUserInput(const std::string& input);
    static bool readLine(const std::string& prompt, std::string& line);
    static bool readIntInRange(const std::string& prompt, int minValue, int maxValue, int& value);
};

#endif // INPUT_VALIDATOR_H
