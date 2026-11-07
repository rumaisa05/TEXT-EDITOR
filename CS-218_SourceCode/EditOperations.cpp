#include "EditOperations.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
// UTF-8 continuation bytes are not valid character-start edit positions.
bool isContinuationByte(char c) {
    return (static_cast<unsigned char>(c) & 0xC0u) == 0x80u;
}
} // namespace

EditOperations::EditOperations(TextDocument& document, std::size_t undoCapacity)
    : document_(document), undoStack_(undoCapacity) {}

void EditOperations::insertTextRaw(std::size_t line, std::size_t position,
                                   const std::string& text) {
    TextDocument::LineNode* node = document_.nodeAt(line);
    if (position > node->length) {
        throw std::out_of_range("Character position is out of range.");
    }
    if (text.size() > std::numeric_limits<std::size_t>::max() - node->length - 1) {
        throw std::length_error("Inserted text is too large.");
    }

    // Build a replacement buffer before releasing the old one.
    const std::size_t newLength = node->length + text.size();
    char* newData = new char[newLength + 1];
    std::copy(node->data, node->data + position, newData);
    std::copy(text.begin(), text.end(), newData + position);
    std::copy(node->data + position, node->data + node->length,
              newData + position + text.size());
    newData[newLength] = '\0';

    delete[] node->data;
    node->data = newData;
    node->length = newLength;
    node->capacity = newLength + 1;
}

void EditOperations::deleteRangeRaw(std::size_t line, std::size_t position,
                                    std::size_t count) {
    TextDocument::LineNode* node = document_.nodeAt(line);
    if (position > node->length || count > node->length - position) {
        throw std::out_of_range("Deletion range is out of bounds.");
    }
    std::memmove(node->data + position, node->data + position + count,
                 node->length - position - count);
    node->length -= count;
    node->data[node->length] = '\0';
}

bool EditOperations::insertText(std::size_t line, std::size_t position,
                                const std::string& text) {
    if (text.empty() || text.find_first_of("\r\n") != std::string::npos ||
        line == 0 || line > document_.lineCount()) {
        return false;
    }
    {
        const std::string current = document_.getLine(line);
        if (position > current.size()) return false;
        if (position < current.size() && isContinuationByte(current[position])) return false;
    }

    if (!undoStack_.push(UndoRecord(EditType::InsertText, line - 1, position, "", text))) {
        return false;
    }
    try {
        insertTextRaw(line - 1, position, text);
    } catch (...) {
        undoStack_.discardTop();
        throw;
    }
    return true;
}

bool EditOperations::insertCharacter(std::size_t line, std::size_t position, char ch) {
    if (ch == '\n' || ch == '\r') return false;
    return insertText(line, position, std::string(1, ch));
}

// Deletes one whole character. The document stores UTF-8 bytes, so a character
// may be several bytes long: removing just one byte would corrupt the text.
bool EditOperations::deleteCharacter(std::size_t line, std::size_t position) {
    if (line == 0 || line > document_.lineCount()) return false;
    const std::string text = document_.getLine(line);
    if (position >= text.size()) return false;
    if (isContinuationByte(text[position])) return false;   // not at a character boundary
    std::size_t length = 1;
    while (position + length < text.size() && isContinuationByte(text[position + length])) {
        ++length;
    }
    return deleteRange(line, position, length);
}

bool EditOperations::deleteWord(std::size_t line, std::size_t position) {
    if (line == 0 || line > document_.lineCount()) return false;

    const std::string text = document_.getLine(line);
    if (position >= text.size()) return false;
    // Delete the whitespace at the cursor and the following word as one edit.
    std::size_t end = position;
    while (end < text.size() &&
           std::isspace(static_cast<unsigned char>(text[end]))) {
        ++end;
    }
    while (end < text.size() &&
           !std::isspace(static_cast<unsigned char>(text[end]))) {
        ++end;
    }
    if (end == position) return false;
    return deleteRange(line, position, end - position);
}

bool EditOperations::deleteRange(std::size_t line, std::size_t position,
                                 std::size_t count) {
    if (line == 0 || line > document_.lineCount() || count == 0) {
        return false;
    }
    const std::string text = document_.getLine(line);
    if (position > text.size() || count > text.size() - position) return false;

    const std::string removed = text.substr(position, count);
    if (!undoStack_.push(UndoRecord(EditType::DeleteText, line - 1, position,
                                    removed, ""))) {
        return false;
    }
    deleteRangeRaw(line - 1, position, count);
    return true;
}

bool EditOperations::insertLine(std::size_t line, const std::string& text) {
    if (line == 0 ||
        document_.lineCount() == std::numeric_limits<std::size_t>::max() ||
        line > document_.lineCount() + 1 ||
        text.find_first_of("\r\n") != std::string::npos) {
        return false;
    }
    if (!undoStack_.push(UndoRecord(EditType::InsertLine, line - 1, 0, "", text))) {
        return false;
    }
    try {
        document_.insertLineRaw(line - 1, text);
    } catch (...) {
        undoStack_.discardTop();
        throw;
    }
    return true;
}

bool EditOperations::deleteLine(std::size_t line) {
    if (line == 0 || line > document_.lineCount()) return false;

    const std::string oldText = document_.getLine(line);
    const bool deletingOnlyLine = document_.lineCount() == 1;
    if (!undoStack_.push(UndoRecord(EditType::DeleteLine, line - 1, 0, oldText,
                                    deletingOnlyLine ? "1" : ""))) {
        return false;
    }
    document_.removeLineRaw(line - 1);
    return true;
}

bool EditOperations::deleteLineRange(std::size_t firstLine, std::size_t count) {
    if (count == 0 || firstLine == 0 || firstLine > document_.lineCount() ||
        count > document_.lineCount() - firstLine + 1) {
        return false;
    }

    std::string removed;
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0) removed.push_back('\n');
        removed += document_.getLine(firstLine + i);
    }
    const bool deletingWholeDocument =
        firstLine == 1 && count == document_.lineCount();
    if (!undoStack_.push(UndoRecord(EditType::DeleteLine, firstLine - 1, count,
                                    removed, deletingWholeDocument ? "1" : ""))) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        document_.removeLineRaw(firstLine - 1);
    }
    return true;
}

bool EditOperations::replaceText(std::size_t line, std::size_t position,
                                 std::size_t count, const std::string& replacement) {
    if (line == 0 || line > document_.lineCount() || count == 0 ||
        replacement.find_first_of("\r\n") != std::string::npos) {
        return false;
    }

    const std::string oldLine = document_.getLine(line);
    if (position > oldLine.size() || count > oldLine.size() - position) return false;
    const std::string oldText = oldLine.substr(position, count);
    if (oldText == replacement) return false;

    std::string newLine = oldLine;
    newLine.replace(position, count, replacement);
    if (!undoStack_.push(UndoRecord(EditType::ReplaceText, line - 1, position,
                                    oldText, replacement))) {
        return false;
    }
    try {
        document_.nodeAt(line - 1)->setText(newLine);
    } catch (...) {
        undoStack_.discardTop();
        throw;
    }
    return true;
}

bool EditOperations::replaceDocumentText(const std::string& rawText, bool coalesceLocalEdit) {
    // The document never stores '\r', so normalise line endings first.
    std::string text;
    text.reserve(rawText.size());
    for (std::size_t i = 0; i < rawText.size(); ++i) {
        if (rawText[i] == '\r') {
            if (i + 1 < rawText.size() && rawText[i + 1] == '\n') ++i;
            text.push_back('\n');
        } else {
            text.push_back(rawText[i]);
        }
    }

    const std::string oldText = document_.getAllText();
    if (text == oldText) return false;

    // Store only the changed span, not two full document snapshots.
    std::size_t prefix = 0;
    const std::size_t sharedLimit = std::min(oldText.size(), text.size());
    while (prefix < sharedLimit && oldText[prefix] == text[prefix]) {
        ++prefix;
    }
    // Never start the changed region in the middle of a multi-byte UTF-8 character.
    while (prefix > 0 &&
           ((prefix < oldText.size() && isContinuationByte(oldText[prefix])) ||
            (prefix < text.size() && isContinuationByte(text[prefix])))) {
        --prefix;
    }

    std::size_t oldEnd = oldText.size();
    std::size_t newEnd = text.size();
    while (oldEnd > prefix && newEnd > prefix &&
           oldText[oldEnd - 1] == text[newEnd - 1]) {
        --oldEnd;
        --newEnd;
    }
    // ... and never end it in the middle of one either.
    while ((oldEnd < oldText.size() && isContinuationByte(oldText[oldEnd])) ||
           (newEnd < text.size() && isContinuationByte(text[newEnd]))) {
        if (oldEnd < oldText.size()) ++oldEnd;
        if (newEnd < text.size()) ++newEnd;
    }

    const std::string removed = oldText.substr(prefix, oldEnd - prefix);
    const std::string inserted = text.substr(prefix, newEnd - prefix);
    if (coalesceLocalEdit &&
        removed.find_first_of("\r\n") == std::string::npos &&
        inserted.find_first_of("\r\n") == std::string::npos) {
        std::size_t line = 1;
        std::size_t lineStart = 0;
        while (line <= document_.lineCount()) {
            const std::size_t currentLineLength = document_.lineLength(line);
            const std::size_t lineEnd = lineStart + currentLineLength;
            if (prefix >= lineStart && prefix <= lineEnd &&
                removed.size() <= currentLineLength - (prefix - lineStart)) {
                const std::size_t column = prefix - lineStart;
                const std::string oldLine = document_.getLine(line);
                std::string newLine = oldLine;
                newLine.replace(column, removed.size(), inserted);

                UndoRecord record;
                if (removed.empty()) {
                    record = UndoRecord(EditType::InsertText, line - 1, column, "", inserted);
                } else if (inserted.empty()) {
                    record = UndoRecord(EditType::DeleteText, line - 1, column, removed, "");
                } else {
                    record = UndoRecord(EditType::ReplaceText, line - 1, column,
                                        removed, inserted);
                }

                TextDocument::LineNode* node = document_.nodeAt(line - 1);
                node->setText(newLine);
                try {
                    if (!undoStack_.pushCoalescing(record)) {
                        node->setText(oldLine);
                        return false;
                    }
                } catch (...) {
                    node->setText(oldLine);
                    throw;
                }
                return true;
            }
            lineStart = lineEnd + 1;
            ++line;
        }
    }

    if (removed.find_first_of("\r\n") == std::string::npos &&
        inserted.find_first_of("\r\n") == std::string::npos) {
        std::size_t line = 1;
        std::size_t lineStart = 0;
        while (line <= document_.lineCount()) {
            const std::size_t lineEnd = lineStart + document_.lineLength(line);
            if (prefix >= lineStart && prefix <= lineEnd) {
                const std::size_t column = prefix - lineStart;
                if (removed.empty()) {
                    return insertText(line, column, inserted);
                }
                if (inserted.empty()) {
                    return deleteRange(line, column, removed.size());
                }
                return replaceText(line, column, removed.size(), inserted);
            }
            lineStart = lineEnd + 1;
            ++line;
        }
    }

    // Multi-line change: remember only the changed region (and where it starts
    // in the whole text) instead of a full copy of the document.
    if (!undoStack_.push(UndoRecord(EditType::ReplaceDocument, 0, prefix,
                                    removed, inserted))) {
        return false;
    }
    try {
        document_.loadText(text);
    } catch (...) {
        undoStack_.discardTop();
        throw;
    }
    return true;
}

void EditOperations::applyUndo(const UndoRecord& record) {
    // Reconstruct the document by applying the inverse described by the record.
    switch (record.type) {
        case EditType::InsertText:
            deleteRangeRaw(record.line, record.position, record.newText.size());
            break;
        case EditType::DeleteText:
            insertTextRaw(record.line, record.position, record.oldText);
            break;
        case EditType::InsertLine:
            document_.removeLineRaw(record.line);
            break;
        case EditType::DeleteLine:
            if (record.position == 0) {
                if (record.newText == "1") {
                    document_.nodeAt(0)->setText(record.oldText);
                } else {
                    document_.insertLineRaw(record.line, record.oldText);
                }
                break;
            }
            {
                const std::size_t lineCount = record.position;
                std::vector<std::unique_ptr<TextDocument::LineNode>> nodes;
                nodes.reserve(lineCount);
                std::size_t start = 0;
                for (std::size_t i = 0; i < lineCount; ++i) {
                    const std::size_t newline = record.oldText.find('\n', start);
                    if (i + 1 < lineCount && newline == std::string::npos) {
                        throw std::logic_error("Undo record has an invalid deleted line range.");
                    }
                    const std::size_t end =
                        i + 1 == lineCount ? record.oldText.size() : newline;
                    nodes.push_back(std::make_unique<TextDocument::LineNode>(
                        record.oldText.substr(start, end - start)));
                    start = end + 1;
                }

                const bool deletingWholeDocument = record.newText == "1";
                if (deletingWholeDocument) {
                    if (document_.lineCount() != 1 ||
                        !document_.getLine(1).empty()) {
                        throw std::logic_error("Undo record no longer matches the document.");
                    }
                    document_.clear();
                }

                const std::size_t index = record.line;
                TextDocument::LineNode* previous = nullptr;
                TextDocument::LineNode* current = nullptr;
                if (index == 0) {
                    current = document_.head_;
                } else if (index == document_.lineCount_) {
                    previous = document_.tail_;
                } else {
                    current = document_.nodeAt(index);
                    previous = current->prev;
                }

                TextDocument::LineNode* first = nodes.front().get();
                TextDocument::LineNode* last = nodes.back().get();
                for (std::size_t i = 1; i < nodes.size(); ++i) {
                    nodes[i - 1]->next = nodes[i].get();
                    nodes[i]->prev = nodes[i - 1].get();
                }
                first->prev = previous;
                last->next = current;
                if (previous) previous->next = first;
                else document_.head_ = first;
                if (current) current->prev = last;
                else document_.tail_ = last;
                document_.lineCount_ += lineCount;
                for (std::unique_ptr<TextDocument::LineNode>& node : nodes) {
                    node.release();
                }
            }
            break;
        case EditType::ReplaceText: {
            const std::string line = document_.getLine(record.line + 1);
            if (record.position > line.size() ||
                record.newText.size() > line.size() - record.position ||
                line.compare(record.position, record.newText.size(), record.newText) != 0) {
                throw std::logic_error("Undo record no longer matches the document.");
            }
            std::string restored = line;
            restored.replace(record.position, record.newText.size(), record.oldText);
            document_.nodeAt(record.line)->setText(restored);
            break;
        }
        case EditType::ReplaceDocument: {
            std::string whole = document_.getAllText();
            if (record.position > whole.size() ||
                record.newText.size() > whole.size() - record.position ||
                whole.compare(record.position, record.newText.size(), record.newText) != 0) {
                throw std::logic_error("Undo record no longer matches the document.");
            }
            whole.replace(record.position, record.newText.size(), record.oldText);
            document_.loadText(whole);
            break;
        }
    }
}

bool EditOperations::undoLastOperation() {
    UndoRecord record;
    if (!undoStack_.peek(record)) return false;
    // Keep the record until reversal succeeds, so failed undo can be retried.
    applyUndo(record);
    undoStack_.discardTop();
    return true;
}

bool EditOperations::undoIsEmpty() const { return undoStack_.isEmpty(); }
bool EditOperations::undoIsFull() const { return undoStack_.isFull(); }
std::size_t EditOperations::undoCount() const { return undoStack_.size(); }
void EditOperations::clearUndoHistory() { undoStack_.clear(); }
