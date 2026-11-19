#ifndef EDIT_OPERATIONS_H
#define EDIT_OPERATIONS_H

#include "TextDocument.h"
#include "UndoStack.h"
#include <cstddef>
#include <string>

// Applies edits to a document and records reversible operations in a bounded history.
class EditOperations {
private:
    TextDocument& document_;
    UndoStack undoStack_;

    void insertTextRaw(std::size_t line, std::size_t position, const std::string& text);
    void deleteRangeRaw(std::size_t line, std::size_t position, std::size_t count);
    void applyUndo(const UndoRecord& record);

public:
    explicit EditOperations(TextDocument& document, std::size_t undoCapacity = 100);

    // Lines are 1-based; text positions are zero-based byte offsets.
    // Inserts nonempty single-line text and records the inverse edit.
    bool insertText(std::size_t oneBasedLine, std::size_t position, const std::string& text);
    bool insertCharacter(std::size_t oneBasedLine, std::size_t position, char ch);
    // Deletes a complete encoded character, a word segment, or a byte range.
    bool deleteCharacter(std::size_t oneBasedLine, std::size_t position);
    bool deleteWord(std::size_t oneBasedLine, std::size_t position);
    bool deleteRange(std::size_t oneBasedLine, std::size_t position, std::size_t count);
    // Line ranges are one-based; deleting a range creates one undo step.
    bool insertLine(std::size_t oneBasedLine, const std::string& text);
    bool deleteLine(std::size_t oneBasedLine);
    bool deleteLineRange(std::size_t firstLine, std::size_t count);
    bool replaceText(std::size_t oneBasedLine, std::size_t position,
                     std::size_t count, const std::string& replacement);
    // Applies a document update as a local edit when possible, otherwise as a span edit.
    bool replaceDocumentText(const std::string& text, bool coalesceLocalEdit = false);
    // Reverses the newest edit and removes its record only after success.
    bool undoLastOperation();

    bool undoIsEmpty() const;
    bool undoIsFull() const;
    std::size_t undoCount() const;
    void clearUndoHistory();
};

#endif
