#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <cstddef>
#include <string>

class TextDocument;

// Displays document text and statistics without modifying the document.
class DisplayManager {
public:
    explicit DisplayManager(const TextDocument& document);

    void displayDocument() const;
    void displayLine(int lineNumber) const;
    void displayAllLines() const;
    int countLines() const;
    int countCharacters() const;
    void displayStatistics() const;

private:
    const TextDocument& document_;
};

#endif
