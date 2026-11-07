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

// All searching uses the KMP algorithm. Every search function accepts two
// optional switches (defaults keep the original behaviour):
//   matchCase - false folds ASCII letters to lower case before comparing
//   wholeWord - true only accepts matches that are not part of a longer word
//               (letters, digits, '_' and non-ASCII bytes count as word characters)
class TextSearch {
public:
    explicit TextSearch(const TextDocument& document);

    // Collects every non-overlapping match. Returns the status describing the outcome.
    SearchStatus searchText(const std::string& pattern, std::vector<Match>& results,
                            bool matchCase = true, bool wholeWord = false) const;

    // Returns true and fills 'first' if at least one match exists.
    bool findFirstOccurrence(const std::string& pattern, Match& first,
                             bool matchCase = true, bool wholeWord = false) const;

    std::vector<Match> findAllOccurrences(const std::string& pattern,
                                          bool matchCase = true, bool wholeWord = false) const;

    // Convenience check using the default case-sensitive, substring search.
    bool isSearchStringPresent(const std::string& pattern) const;

    // Searches just one 1-based line and returns byte-based columns.
    std::vector<Match> findMatchesInLine(const std::string& pattern, std::size_t lineNumber,
                                         bool matchCase = true, bool wholeWord = false) const;

    // Prints the outcome and every matching location to standard output.
    void displaySearchResults(const std::string& pattern) const;

private:
    const TextDocument& document_;
};

#endif // TEXT_SEARCH_H
