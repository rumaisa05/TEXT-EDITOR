#include "TextDocument.h"
#include <stdexcept>
#include <algorithm>
#include <limits>

// Manages the growable buffer for one line.
TextDocument::LineNode::LineNode(const std::string& text)
    : data(nullptr), length(0), capacity(0), prev(nullptr), next(nullptr) {
    setText(text);
}

// Releases the character buffer owned by this line.
TextDocument::LineNode::~LineNode() { delete[] data; }

// Replaces line contents and grows the buffer only when it is too small.
void TextDocument::LineNode::setText(const std::string& text) {
    // Include the null terminator and grow the buffer when needed.
    if (text.size() == std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("Line text is too large.");
    }
    std::size_t needed = text.size() + 1;
    if (needed > capacity) {
        const std::size_t grownCapacity =
            capacity == 0 ? 16 :
            (capacity > std::numeric_limits<std::size_t>::max() / 2
                 ? std::numeric_limits<std::size_t>::max()
                 : capacity * 2);
        std::size_t newCapacity = std::max<std::size_t>(needed, grownCapacity);
        char* replacement = new char[newCapacity];
        std::copy(text.begin(), text.end(), replacement);
        replacement[text.size()] = '\0';
        delete[] data;
        data = replacement;
        capacity = newCapacity;
    } else {
        std::copy(text.begin(), text.end(), data);
        data[text.size()] = '\0';
    }
    length = text.size();
}

// Starts the linked list with the empty line required by editing operations.
TextDocument::TextDocument() : head_(nullptr), tail_(nullptr), lineCount_(0) {
    insertLineRaw(0, "");
}

// Releases every node owned by the document.
TextDocument::~TextDocument() { clear(); }

std::size_t TextDocument::lineCount() const { return lineCount_; }

// Blank lines count as content; one empty line is an empty document.
bool TextDocument::isEmpty() const {
    return lineCount_ == 0 || (lineCount_ == 1 && head_->length == 0);
}

// Finds a zero-based line by traversing from the nearer end of the list.
TextDocument::LineNode* TextDocument::nodeAt(std::size_t index) const {
    if (index >= lineCount_) throw std::out_of_range("Line number is out of range.");
    // Start from the closer end of the list.
    if (index < lineCount_ / 2) {
        LineNode* p = head_;
        for (std::size_t i = 0; i < index; ++i) p = p->next;
        return p;
    }
    LineNode* p = tail_;
    for (std::size_t i = lineCount_ - 1; i > index; --i) p = p->prev;
    return p;
}

// Returns a copy of the requested one-based line.
std::string TextDocument::getLine(std::size_t oneBasedLine) const {
    if (oneBasedLine == 0) throw std::out_of_range("Line numbers start at 1.");
    LineNode* p = nodeAt(oneBasedLine - 1);
    return std::string(p->data, p->length);
}

std::size_t TextDocument::lineLength(std::size_t oneBasedLine) const {
    if (oneBasedLine == 0) throw std::out_of_range("Line numbers start at 1.");
    return nodeAt(oneBasedLine - 1)->length;
}

// Joins line buffers with newline separators into one string.
std::string TextDocument::getAllText() const {
    std::string result;
    // Reserve space for the text and separating newlines.
    std::size_t totalLength = lineCount_ == 0 ? 0 : lineCount_ - 1;
    // Measure each line with overflow checks before allocating the result.
    for (LineNode* p = head_; p; p = p->next) {
        if (p->length > std::numeric_limits<std::size_t>::max() - totalLength) {
            throw std::length_error("Document text is too large.");
        }
        totalLength += p->length;
    }
    result.reserve(totalLength);

    LineNode* p = head_;
    // Traverse once so each line and separator is appended in document order.
    while (p) {
        result.append(p->data, p->length);
        if (p->next) result.push_back('\n');
        p = p->next;
    }
    return result;
}

// Inserts a node at a zero-based position and reconnects its neighbors.
void TextDocument::insertLineRaw(std::size_t index, const std::string& text) {
    if (index > lineCount_) throw std::out_of_range("Insertion line is out of range.");
    if (lineCount_ == std::numeric_limits<std::size_t>::max()) {
        throw std::length_error("Document has too many lines.");
    }
    LineNode* n = new LineNode(text);
    if (lineCount_ == 0) {
        head_ = tail_ = n;
    } else if (index == 0) {
        n->next = head_;
        head_->prev = n;
        head_ = n;
    } else if (index == lineCount_) {
        n->prev = tail_;
        tail_->next = n;
        tail_ = n;
    } else {
        LineNode* current = nodeAt(index);
        n->prev = current->prev;
        n->next = current;
        current->prev->next = n;
        current->prev = n;
    }
    ++lineCount_;
}

// Preserves the line text before unlinking its node.
std::string TextDocument::deleteLineRaw(std::size_t index) {
    LineNode* n = nodeAt(index);
    std::string old(n->data, n->length);
    removeLineRaw(index);
    return old;
}

// Unlinks the selected node from the line list.
void TextDocument::removeLineRaw(std::size_t index) {
    LineNode* n = nodeAt(index);
    // Keep one empty line so the document remains editable.
    if (lineCount_ == 1) {
        n->setText("");
        return;
    }
    if (n->prev) n->prev->next = n->next; else head_ = n->next;
    if (n->next) n->next->prev = n->prev; else tail_ = n->prev;
    delete n;
    --lineCount_;
}

// Deletes all nodes and resets the document to an empty list.
void TextDocument::clear() {
    LineNode* p = head_;
    // Save the next pointer before deleting the current node.
    while (p) {
        LineNode* next = p->next;
        delete p;
        p = next;
    }
    head_ = tail_ = nullptr;
    lineCount_ = 0;
}

// Builds a replacement list first so failed parsing leaves the current text intact.
void TextDocument::loadText(const std::string& text) {
    // Normalize CRLF and CR to '\n' before splitting into lines.
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            normalized.push_back('\n');
        } else {
            normalized.push_back(text[i]);
        }
    }

    // Keep the current document intact if parsing fails.
    TextDocument parsed;
    parsed.clear();
    std::size_t start = 0;
    // Split normalized text while preserving an empty final line after a newline.
    while (true) {
        std::size_t newline = normalized.find('\n', start);
        std::string line = newline == std::string::npos
            ? normalized.substr(start) : normalized.substr(start, newline - start);
        parsed.insertLineRaw(parsed.lineCount_, line);
        if (newline == std::string::npos) break;
        start = newline + 1;
        if (start == normalized.size()) {
            parsed.insertLineRaw(parsed.lineCount_, "");
            break;
        }
    }
    swap(parsed);
}

// Exchanges list ownership without copying any line contents.
void TextDocument::swap(TextDocument& other) noexcept {
    LineNode* head = head_;
    head_ = other.head_;
    other.head_ = head;

    LineNode* tail = tail_;
    tail_ = other.tail_;
    other.tail_ = tail;

    std::size_t count = lineCount_;
    lineCount_ = other.lineCount_;
    other.lineCount_ = count;
}
