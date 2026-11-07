#include "UndoStack.h"

#include <limits>
#include <utility>

// Keeps the most recent undo record at the head of the linked list.
UndoStack::UndoStack(std::size_t capacity)
    : top_(nullptr), bottom_(nullptr), size_(0), capacity_(capacity) {}

UndoStack::~UndoStack() { clear(); }

void UndoStack::dropBottom() noexcept {
    // Evict the oldest edit while preserving the newest-to-oldest links.
    StackNode* oldest = bottom_;
    if (!oldest) return;
    bottom_ = oldest->prev;
    if (bottom_) bottom_->next = nullptr;
    else top_ = nullptr;
    delete oldest;
    --size_;
}

bool UndoStack::push(const UndoRecord& record) {
    if (capacity_ == 0) return false;
    StackNode* node = new StackNode(record);   // allocate first: if it throws, nothing changed
    if (size_ >= capacity_) dropBottom();
    node->next = top_;
    if (top_) top_->prev = node;
    top_ = node;
    if (!bottom_) bottom_ = node;
    ++size_;
    return true;
}

bool UndoStack::pushCoalescing(const UndoRecord& record) {
    // Adjacent typing/deletion edits can share a single undo record when their
    // ranges line up and the combined edit remains small.
    if (top_ != nullptr && top_->record.line == record.line) {
        UndoRecord combined = top_->record;
        bool canMerge = false;
        const std::size_t kMax = kMaxCoalescedBytes;

        if (combined.type == EditType::InsertText &&
            record.type == EditType::InsertText &&
            combined.newText.size() + record.newText.size() <= kMax &&
            combined.position <= std::numeric_limits<std::size_t>::max() -
                                     combined.newText.size() &&
            record.position == combined.position + combined.newText.size()) {
            combined.newText += record.newText;
            canMerge = true;
        } else if (combined.type == EditType::DeleteText &&
                   record.type == EditType::DeleteText &&
                   combined.oldText.size() + record.oldText.size() <= kMax) {
            if (record.position == combined.position) {
                combined.oldText += record.oldText;
                canMerge = true;
            } else if (record.position <= std::numeric_limits<std::size_t>::max() -
                                              record.oldText.size() &&
                       record.position + record.oldText.size() == combined.position) {
                combined.position = record.position;
                combined.oldText = record.oldText + combined.oldText;
                canMerge = true;
            }
        } else if (combined.type == EditType::ReplaceText &&
                   record.type == EditType::InsertText &&
                   combined.newText.size() + record.newText.size() <= kMax &&
                   combined.position <= std::numeric_limits<std::size_t>::max() -
                                            combined.newText.size() &&
                   record.position == combined.position + combined.newText.size()) {
            combined.newText += record.newText;
            canMerge = true;
        } else if (combined.type == EditType::ReplaceText &&
                   record.type == EditType::ReplaceText &&
                   record.position == combined.position &&
                   record.oldText == combined.newText) {
            combined.newText = record.newText;
            canMerge = true;
        } else if (combined.type == EditType::DeleteText &&
                   record.type == EditType::InsertText &&
                   record.position == combined.position &&
                   combined.oldText.size() + record.newText.size() <= kMax) {
            combined.type = EditType::ReplaceText;
            combined.newText = record.newText;
            canMerge = true;
        }

        if (canMerge) {
            top_->record = std::move(combined);
            return true;
        }
    }

    return push(record);
}

bool UndoStack::pop(UndoRecord& record) {
    if (isEmpty()) return false;
    record = top_->record;
    discardTop();
    return true;
}

bool UndoStack::peek(UndoRecord& record) const {
    if (isEmpty()) return false;
    record = top_->record;
    return true;
}

bool UndoStack::discardTop() noexcept {
    if (isEmpty()) return false;
    StackNode* oldTop = top_;
    top_ = oldTop->next;
    if (top_) top_->prev = nullptr;
    else bottom_ = nullptr;
    delete oldTop;
    --size_;
    return true;
}

bool UndoStack::isEmpty() const { return top_ == nullptr; }
bool UndoStack::isFull() const { return size_ >= capacity_; }
std::size_t UndoStack::size() const { return size_; }
std::size_t UndoStack::capacity() const { return capacity_; }

void UndoStack::clear() {
    while (top_) {
        StackNode* oldTop = top_;
        top_ = top_->next;
        delete oldTop;
    }
    bottom_ = nullptr;
    size_ = 0;
}
