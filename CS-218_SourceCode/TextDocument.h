#ifndef TEXT_DOCUMENT_H
#define TEXT_DOCUMENT_H

#include <cstddef>
#include <string>

// Stores text as a doubly linked list of lines; public line numbers are 1-based.
class TextDocument {
private:
    // Each line owns a growable, null-terminated byte buffer.
    struct LineNode {
        char* data;
        std::size_t length;
        std::size_t capacity;
        LineNode* prev;
        LineNode* next;

        explicit LineNode(const std::string& text);
        ~LineNode();
        void setText(const std::string& text);
    };

    LineNode* head_;
    LineNode* tail_;
    std::size_t lineCount_;

    // Internal helpers use zero-based indexes and keep the list links consistent.
    LineNode* nodeAt(std::size_t zeroBasedLine) const;
    void insertLineRaw(std::size_t zeroBasedLine, const std::string& text);
    std::string deleteLineRaw(std::size_t zeroBasedLine);
    void removeLineRaw(std::size_t zeroBasedLine);

    friend class EditOperations;

public:
    // A new document starts with one empty line; copies are disabled to preserve ownership.
    TextDocument();
    ~TextDocument();
    TextDocument(const TextDocument&) = delete;
    TextDocument& operator=(const TextDocument&) = delete;

    std::size_t lineCount() const; // number of stored lines
    bool isEmpty() const; // true only when the document contains no text
    std::string getLine(std::size_t oneBasedLine) const;
    std::size_t lineLength(std::size_t oneBasedLine) const;
    std::string getAllText() const; // joins lines with '\n'
    void clear(); // removes all line nodes
    void loadText(const std::string& text); // normalizes CRLF/CR and replaces the document
    void swap(TextDocument& other) noexcept; // exchanges document contents without copying
};

#endif
