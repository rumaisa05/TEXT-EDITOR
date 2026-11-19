#ifndef TEXT_DOCUMENT_H
#define TEXT_DOCUMENT_H

#include <cstddef>
#include <string>

// Stores text as a doubly linked list of lines; public line numbers are 1-based.
class TextDocument {
private:
    // Each node owns one null-terminated line buffer.
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

    // Internal line indexes are zero-based.
    LineNode* nodeAt(std::size_t zeroBasedLine) const;
    void insertLineRaw(std::size_t zeroBasedLine, const std::string& text);
    std::string deleteLineRaw(std::size_t zeroBasedLine);
    void removeLineRaw(std::size_t zeroBasedLine);

    friend class EditOperations;

public:
    // Starts with one empty line; copying is disabled because nodes are owned.
    TextDocument();
    ~TextDocument();
    TextDocument(const TextDocument&) = delete;
    TextDocument& operator=(const TextDocument&) = delete;

    std::size_t lineCount() const;
    bool isEmpty() const;
    // Public line numbers start at one; invalid numbers throw out_of_range.
    std::string getLine(std::size_t oneBasedLine) const;
    std::size_t lineLength(std::size_t oneBasedLine) const;
    // Reconstructs the document with LF separators between lines.
    std::string getAllText() const;
    void clear();
    // Normalizes line endings and preserves the current document if loading fails.
    void loadText(const std::string& text);
    // Exchanges ownership of the linked lists in constant time.
    void swap(TextDocument& other) noexcept;
};

#endif
