#include "TextSearch.h"
#include "InputValidator.h"

#include <cctype>
#include <iostream>

namespace {

// Fold ASCII case only; unsigned-byte conversion keeps ctype calls well-defined.
unsigned char fold(char c, bool matchCase) {
    const unsigned char u = static_cast<unsigned char>(c);
    return matchCase ? u : static_cast<unsigned char>(std::tolower(u));
}

bool isWordByte(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    return u >= 0x80 || std::isalnum(u) || u == '_';
}

// Builds the KMP failure table (longest proper prefix which is also a suffix)
// over the (optionally case-folded) pattern.
std::vector<std::size_t> buildFailureTable(const std::string& pattern, bool matchCase) {
    std::vector<std::size_t> lps(pattern.size(), 0);
    std::size_t length = 0;
    std::size_t i = 1;
    while (i < pattern.size()) {
        if (fold(pattern[i], matchCase) == fold(pattern[length], matchCase)) {
            lps[i++] = ++length;
        } else if (length != 0) {
            length = lps[length - 1];
        } else {
            lps[i++] = 0;
        }
    }
    return lps;
}

// KMP scan of one line. Appends the starting column of every non-overlapping
// accepted occurrence to 'columns'. Stops after the first one if firstOnly.
void scanLine(const std::string& text, const std::string& pattern,
              const std::vector<std::size_t>& lps, bool matchCase, bool wholeWord,
              bool firstOnly, std::vector<std::size_t>& columns) {
    if (pattern.empty() || pattern.size() > text.size()) return;

    std::size_t matched = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        while (matched > 0 && fold(text[i], matchCase) != fold(pattern[matched], matchCase)) {
            matched = lps[matched - 1];
        }
        if (fold(text[i], matchCase) == fold(pattern[matched], matchCase)) {
            ++matched;
        }
        if (matched == pattern.size()) {
            const std::size_t start = i + 1 - pattern.size();
            bool accepted = true;
            if (wholeWord) {
                const bool leftOk = start == 0 || !isWordByte(text[start - 1]);
                const bool rightOk = i + 1 == text.size() || !isWordByte(text[i + 1]);
                accepted = leftOk && rightOk;
            }
            if (accepted) {
                columns.push_back(start);
                if (firstOnly) return;
                matched = 0;                 // restart so the next match cannot overlap
            } else {
                matched = lps[matched - 1];  // rejected: keep scanning for overlapping candidates
            }
        }
    }
}

} // namespace

TextSearch::TextSearch(const TextDocument& document)
    : document_(document) {}

SearchStatus TextSearch::searchText(const std::string& pattern, std::vector<Match>& results,
                                    bool matchCase, bool wholeWord) const {
    results.clear();

    // Report invalid search inputs separately from a valid pattern with no matches.
    if (InputValidator::isSearchStringEmpty(pattern)) {
        return SearchStatus::EmptyPattern;
    }
    if (InputValidator::isDocumentEmpty(document_)) {
        return SearchStatus::EmptyDocument;
    }

    const std::vector<std::size_t> lps = buildFailureTable(pattern, matchCase);
    const std::size_t lines = document_.lineCount();
    std::vector<std::size_t> columns;
    for (std::size_t line = 1; line <= lines; ++line) {
        columns.clear();
        scanLine(document_.getLine(line), pattern, lps, matchCase, wholeWord, false, columns);
        for (std::size_t column : columns) results.push_back(Match{line, column});
    }
    return results.empty() ? SearchStatus::NotFound : SearchStatus::Found;
}

bool TextSearch::findFirstOccurrence(const std::string& pattern, Match& first,
                                     bool matchCase, bool wholeWord) const {
    if (InputValidator::isSearchStringEmpty(pattern) || InputValidator::isDocumentEmpty(document_)) {
        return false;
    }

    const std::vector<std::size_t> lps = buildFailureTable(pattern, matchCase);
    const std::size_t lines = document_.lineCount();
    std::vector<std::size_t> columns;
    for (std::size_t line = 1; line <= lines; ++line) {
        columns.clear();
        scanLine(document_.getLine(line), pattern, lps, matchCase, wholeWord, true, columns);
        if (!columns.empty()) {
            first = Match{line, columns.front()};
            return true;
        }
    }
    return false;
}

std::vector<Match> TextSearch::findAllOccurrences(const std::string& pattern,
                                                  bool matchCase, bool wholeWord) const {
    std::vector<Match> results;
    searchText(pattern, results, matchCase, wholeWord);
    return results;
}

bool TextSearch::isSearchStringPresent(const std::string& pattern) const {
    Match unused{0, 0};
    return findFirstOccurrence(pattern, unused);
}

std::vector<Match> TextSearch::findMatchesInLine(const std::string& pattern, std::size_t lineNumber,
                                                 bool matchCase, bool wholeWord) const {
    std::vector<Match> matches;
    if (InputValidator::isSearchStringEmpty(pattern) ||
        !InputValidator::isValidLineNumber(lineNumber, document_.lineCount())) {
        return matches;
    }
    const std::vector<std::size_t> lps = buildFailureTable(pattern, matchCase);
    std::vector<std::size_t> columns;
    scanLine(document_.getLine(lineNumber), pattern, lps, matchCase, wholeWord, false, columns);
    for (std::size_t column : columns) matches.push_back(Match{lineNumber, column});
    return matches;
}

void TextSearch::displaySearchResults(const std::string& pattern) const {
    std::vector<Match> results;
    switch (searchText(pattern, results)) {
        case SearchStatus::EmptyPattern:
            std::cout << "Error: the search string cannot be empty.\n";
            break;
        case SearchStatus::EmptyDocument:
            std::cout << "Error: the document is empty. Nothing to search.\n";
            break;
        case SearchStatus::NotFound:
            std::cout << "\"" << pattern << "\" was not found in the document.\n";
            break;
        case SearchStatus::Found:
            std::cout << "\"" << pattern << "\" found " << results.size() << " occurrence(s):\n";
            for (const Match& match : results) {
                std::cout << "  Line " << match.line << ", Column " << (match.column + 1) << '\n';
            }
            break;
    }
}
