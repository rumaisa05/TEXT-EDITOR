#ifndef UNDO_STACK_H
#define UNDO_STACK_H

#include <cstddef>
#include <string>

// Identifies the inverse operation needed to restore an earlier document state.
enum class EditType {
    InsertText,
    DeleteText,
    InsertLine,
    DeleteLine,
    ReplaceText,
    ReplaceDocument
};

// Stores the minimum text and location needed to reverse one edit.
struct UndoRecord {
    EditType type;
    std::size_t line;       // zero-based line index
    std::size_t position;   // zero-based byte position (ReplaceDocument: offset in the whole text)
    std::string oldText;
    std::string newText;

    UndoRecord(EditType t = EditType::InsertText, std::size_t l = 0,
               std::size_t p = 0, const std::string& oldValue = "",
               const std::string& newValue = "")
        : type(t), line(l), position(p), oldText(oldValue), newText(newValue) {}
};

// Stores recent undo records; adding to a full stack drops the oldest record.
class UndoStack {
private:
    struct StackNode {
        UndoRecord record;
        StackNode* next;   // towards the bottom (older)
        StackNode* prev;   // towards the top (newer)
        explicit StackNode(const UndoRecord& r) : record(r), next(nullptr), prev(nullptr) {}
    };
    StackNode* top_;
    StackNode* bottom_;
    std::size_t size_;
    std::size_t capacity_;

    void dropBottom() noexcept;

public:
    // Caps the size of a merged typing or deletion undo step.
    static const std::size_t kMaxCoalescedBytes = 64;

    explicit UndoStack(std::size_t capacity = 100);
    ~UndoStack();
    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    // Adds a record and evicts the oldest one if the stack is full.
    bool push(const UndoRecord& record);
    // Merges compatible adjacent edits or pushes a separate record.
    bool pushCoalescing(const UndoRecord& record);
    // Copies and removes the newest record.
    bool pop(UndoRecord& record);
    // Copies the newest record without removing it.
    bool peek(UndoRecord& record) const;
    // Removes the newest record after its inverse has succeeded.
    bool discardTop() noexcept;
    bool isEmpty() const;
    bool isFull() const;
    std::size_t size() const;
    std::size_t capacity() const;
    // Releases all records and resets the stack.
    void clear();
};

#endif
