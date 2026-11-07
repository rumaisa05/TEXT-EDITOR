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

// A bounded undo history implemented as a doubly linked list.
// The newest record is the "top"; when the history is full, pushing a new
// record silently drops the OLDEST one (the bottom), so editing never locks up.
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
    // Merged typing/deleting runs never grow beyond this many bytes, so a
    // single undo step cannot wipe out an arbitrarily long piece of text.
    static const std::size_t kMaxCoalescedBytes = 64;

    explicit UndoStack(std::size_t capacity = 100);
    ~UndoStack();
    UndoStack(const UndoStack&) = delete;
    UndoStack& operator=(const UndoStack&) = delete;

    bool push(const UndoRecord& record);            // evicts the oldest record when full
    bool pushCoalescing(const UndoRecord& record);  // merges with the top record when possible
    bool pop(UndoRecord& record);
    bool peek(UndoRecord& record) const;
    bool discardTop() noexcept;
    bool isEmpty() const;
    bool isFull() const;                            // true when at capacity (next push evicts)
    std::size_t size() const;
    std::size_t capacity() const;
    void clear();
};

#endif
