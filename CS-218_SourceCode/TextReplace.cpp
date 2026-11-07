#include "TextReplace.h"
#include "InputValidator.h"

#include <stdexcept>

TextReplace::TextReplace(TextDocument& document, EditOperations& editOperations)
    : document_(document),
      editOperations_(editOperations),
      search_(document) {}

ReplaceStatus TextReplace::validateReplacement(const std::string& pattern,
                                               const std::string& replacement) const {
    // Validate before searching or editing so a rejected request leaves state unchanged.
    if (InputValidator::isSearchStringEmpty(pattern)) {
        return ReplaceStatus::EmptyPattern;
    }
    if (InputValidator::isDocumentEmpty(document_)) {
        return ReplaceStatus::EmptyDocument;
    }
    // Newlines would break the line-based structure of the document.
    if (replacement.find_first_of("\r\n") != std::string::npos) {
        return ReplaceStatus::InvalidText;
    }
    return ReplaceStatus::Success;
}

std::size_t TextReplace::countOccurrences(const std::string& pattern,
                                          bool matchCase, bool wholeWord) const {
    return search_.findAllOccurrences(pattern, matchCase, wholeWord).size();
}

ReplaceResult TextReplace::replaceFirstOccurrence(const std::string& pattern,
                                                  const std::string& replacement) {
    const ReplaceStatus validation = validateReplacement(pattern, replacement);
    if (validation != ReplaceStatus::Success) {
        return ReplaceResult{validation, 0};
    }

    Match first{0, 0};
    if (!search_.findFirstOccurrence(pattern, first)) {
        return ReplaceResult{ReplaceStatus::NotFound, 0};
    }

    if (pattern == replacement) return ReplaceResult{ReplaceStatus::Success, 1};
    applyReplacement(first.line, first.column, pattern.size(), replacement);
    return ReplaceResult{ReplaceStatus::Success, 1};
}

ReplaceResult TextReplace::replaceOccurrenceAt(const Match& location, const std::string& pattern,
                                               const std::string& replacement) {
    const ReplaceStatus validation = validateReplacement(pattern, replacement);
    if (validation != ReplaceStatus::Success) {
        return ReplaceResult{validation, 0};
    }

    if (!InputValidator::isValidLineNumber(location.line, document_.lineCount())) {
        return ReplaceResult{ReplaceStatus::InvalidLocation, 0};
    }

    const std::string lineText = document_.getLine(location.line);
    if (location.column > lineText.size() ||
        pattern.size() > lineText.size() - location.column) {
        return ReplaceResult{ReplaceStatus::InvalidLocation, 0};
    }
    const std::size_t endColumn = location.column + pattern.size();
    if (!InputValidator::isValidRange(location.column, endColumn, lineText.size())) {
        return ReplaceResult{ReplaceStatus::InvalidLocation, 0};
    }

    // The text at the given location must actually equal the pattern.
    if (lineText.compare(location.column, pattern.size(), pattern) != 0) {
        return ReplaceResult{ReplaceStatus::NoMatchAtLocation, 0};
    }

    if (pattern == replacement) return ReplaceResult{ReplaceStatus::Success, 1};
    applyReplacement(location.line, location.column, pattern.size(), replacement);
    return ReplaceResult{ReplaceStatus::Success, 1};
}

ReplaceResult TextReplace::replaceAllOccurrences(const std::string& pattern,
                                                 const std::string& replacement,
                                                 bool matchCase, bool wholeWord) {
    const ReplaceStatus validation = validateReplacement(pattern, replacement);
    if (validation != ReplaceStatus::Success) {
        return ReplaceResult{validation, 0};
    }

    // Assemble the result before changing the document so replace-all is atomic
    // and can be recorded as a single undoable document edit.
    std::size_t total = 0;
    std::string newDocument;
    const std::size_t lines = document_.lineCount();

    for (std::size_t line = 1; line <= lines; ++line) {
        const std::vector<Match> matches = search_.findMatchesInLine(pattern, line, matchCase, wholeWord);
        const std::string oldLine = document_.getLine(line);
        if (matches.empty()) {
            newDocument += oldLine;
        } else {
            std::size_t copied = 0;
            for (const Match& match : matches) {
                newDocument.append(oldLine, copied, match.column - copied);
                newDocument += replacement;
                copied = match.column + pattern.size();
            }
            newDocument.append(oldLine, copied, std::string::npos);
            total += matches.size();
        }
        if (line < lines) newDocument.push_back('\n');
    }

    if (total == 0) {
        return ReplaceResult{ReplaceStatus::NotFound, 0};
    }
    // replaceDocumentText() returns false only when nothing changed (every match
    // already equalled the replacement); that is still a successful replace-all.
    editOperations_.replaceDocumentText(newDocument);
    return ReplaceResult{ReplaceStatus::Success, total};
}

std::string TextReplace::describe(ReplaceStatus status) {
    switch (status) {
        case ReplaceStatus::Success:           return "Replacement completed.";
        case ReplaceStatus::NotFound:          return "The search string was not found.";
        case ReplaceStatus::NoMatchAtLocation: return "The text at that location does not match the search string.";
        case ReplaceStatus::EmptyPattern:      return "Error: the search string cannot be empty.";
        case ReplaceStatus::EmptyDocument:     return "Error: the document is empty.";
        case ReplaceStatus::InvalidLocation:   return "Error: the specified location is invalid.";
        case ReplaceStatus::InvalidText:       return "Error: the replacement text cannot contain line breaks.";
    }
    return "Unknown error.";
}

void TextReplace::applyReplacement(std::size_t lineNumber, std::size_t column,
                                   std::size_t length, const std::string& replacement) {
    if (!editOperations_.replaceText(lineNumber, column, length, replacement)) {
        throw std::runtime_error("The replacement could not be recorded or applied.");
    }
}