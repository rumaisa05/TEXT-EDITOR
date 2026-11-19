#ifndef TEXT_SEARCH_H
#define TEXT_SEARCH_H

#include <cstddef>
#include <string>
#include <vector>
#include "TextDocument.h"

struct Match {
    std::size_t line;   // 1-based line number
    std::size_t column; // 0-based BYTE index within the line
};

// Distinguishes an unsuccessful search from invalid or empty input.
enum class SearchStatus {
    Found,
    NotFound,
    EmptyPattern,
    EmptyDocument
};

// Searches with KMP. Case-insensitive matching folds ASCII letters only.
class TextSearch {
public:
    explicit TextSearch(const TextDocument& document);

    SearchStatus searchText(const std::string& pattern, std::vector<Match>& results,
                            bool matchCase = true, bool wholeWord = false) const;

    bool findFirstOccurrence(const std::string& pattern, Match& first,
                             bool matchCase = true, bool wholeWord = false) const;

    std::vector<Match> findAllOccurrences(const std::string& pattern,
                                          bool matchCase = true, bool wholeWord = false) const;

    bool isSearchStringPresent(const std::string& pattern) const;

    std::vector<Match> findMatchesInLine(const std::string& pattern, std::size_t lineNumber,
                                         bool matchCase = true, bool wholeWord = false) const;

    static std::vector<std::size_t> buildLpsTable(const std::string& pattern,
                                                  bool matchCase = true);

    // Pass a prebuilt LPS table to reuse pattern preprocessing across lines.
    std::vector<Match> findMatchesInLine(const std::string& pattern, std::size_t lineNumber,
                                         const std::vector<std::size_t>& lps,
                                         bool matchCase, bool wholeWord) const;

    void displaySearchResults(const std::string& pattern) const;

private:
    const TextDocument& document_;
};

#endif // TEXT_SEARCH_H
