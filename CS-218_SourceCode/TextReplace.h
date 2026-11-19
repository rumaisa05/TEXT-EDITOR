#ifndef TEXT_REPLACE_H
#define TEXT_REPLACE_H

#include <cstddef>
#include <string>
#include "EditOperations.h"
#include "TextDocument.h"
#include "TextSearch.h"

enum class ReplaceStatus {
    Success,
    NotFound,
    NoMatchAtLocation,
    EmptyPattern,
    EmptyDocument,
    InvalidLocation,
    InvalidText
};

struct ReplaceResult {
    ReplaceStatus status;
    std::size_t replacements;
};

// Validates and applies replacements through EditOperations so they remain undoable.
class TextReplace {
public:
    TextReplace(TextDocument& document, EditOperations& editOperations);

    ReplaceStatus validateReplacement(const std::string& pattern, const std::string& replacement) const;

    std::size_t countOccurrences(const std::string& pattern,
                                 bool matchCase = true, bool wholeWord = false) const;

    ReplaceResult replaceFirstOccurrence(const std::string& pattern, const std::string& replacement);

    ReplaceResult replaceOccurrenceAt(const Match& location, const std::string& pattern,
                                      const std::string& replacement);

    // All matches are applied as one undo step.
    ReplaceResult replaceAllOccurrences(const std::string& pattern, const std::string& replacement,
                                        bool matchCase = true, bool wholeWord = false);

    static std::string describe(ReplaceStatus status);

private:
    void applyReplacement(std::size_t lineNumber, std::size_t column,
                          std::size_t length, const std::string& replacement);

    TextDocument& document_;
    EditOperations& editOperations_;
    TextSearch search_;
};

#endif // TEXT_REPLACE_H