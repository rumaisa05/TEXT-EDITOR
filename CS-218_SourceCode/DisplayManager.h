// DisplayManager.h  -  Member 1: Text Display (revised)
// Keeps a CONST reference to the shared document: it can read the text but
// can never change it, and it never makes a copy of it.
//
// ERROR HANDLING
//   The display functions write to std::cout. A failed write only sets the
//   stream's error flags (cout never throws by itself), so every display
//   function checks the stream, flushes, and throws std::ios_base::failure
//   if the output did not go through. countLines()/countCharacters() throw
//   std::overflow_error if the text is too large for an int result.
//   An empty document is NOT an error. Bad line numbers are input validation
//   and are deliberately left as they were.
#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <cstddef>
#include <string>

class TextDocument;

// Read-only console presentation and document statistics.
class DisplayManager {
public:
    explicit DisplayManager(const TextDocument& document);

    // Display methods report stream failures; statistics throw if an int result would overflow.
    void displayDocument() const;             // whole text as-is
    void displayLine(int lineNumber) const;   // one line (numbered from 1)
    void displayAllLines() const;             // every line with its number
    int  countLines() const;                  // total number of lines
    int  countCharacters() const;             // total characters (incl. '\n')
    void displayStatistics() const;           // lines + characters

private:
    const TextDocument& document_;
};

#endif
