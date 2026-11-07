#include "TextDocument.h"
#include "EditOperations.h"
#include "TextSearch.h"
#include "TextReplace.h"
#include "UndoStack.h"
#include "InputValidator.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Small self-contained regression suite for the editor's backend components.
namespace {
int passed = 0;
int failed = 0;

void check(const std::string& id, bool condition) {
    // Keep running after failures so one test run reports all regressions.
    if (condition) {
        ++passed;
        std::cout << "[PASS] " << id << '\n';
    } else {
        ++failed;
        std::cout << "[FAIL] " << id << '\n';
    }
}

bool hasMatch(const std::vector<Match>& matches, std::size_t line, std::size_t column) {
    // Search coordinates are deliberately checked as exact line/byte pairs.
    for (const Match& m : matches)
        if (m.line == line && m.column == column) return true;
    return false;
}
}

int main() {
    // TextDocument construction, loading and line access.
    TextDocument doc;
    check("TD-01 constructor creates one empty line", doc.lineCount() == 1 && doc.isEmpty());
    check("TD-02 empty line can be read", doc.getLine(1).empty());
    doc.loadText("the cat sat on the mat\nThe Cat\nconcatenate cat");
    check("TD-03 loadText creates three lines", doc.lineCount() == 3);
    check("TD-04 getLine uses one-based line numbers", doc.getLine(2) == "The Cat");
    check("TD-05 getAllText preserves line separators",
          doc.getAllText() == "the cat sat on the mat\nThe Cat\nconcatenate cat");
    doc.loadText("a\nb\n");
    check("TD-06 trailing newline creates final empty line",
          doc.lineCount() == 3 && doc.getLine(3).empty());

    // Editing and undo.
    TextDocument editDoc;
    editDoc.loadText("hello world");
    EditOperations edit(editDoc, 100);
    check("ED-01 insert text at valid position", edit.insertText(1, 5, ",") && editDoc.getLine(1) == "hello, world");
    check("ED-02 undo insertion", edit.undoLastOperation() && editDoc.getLine(1) == "hello world");
    check("ED-03 delete range", edit.deleteRange(1, 0, 6) && editDoc.getLine(1) == "world");
    check("ED-04 undo deletion", edit.undoLastOperation() && editDoc.getLine(1) == "hello world");
    check("ED-05 insert at end of line", edit.insertText(1, 11, "!") && editDoc.getLine(1) == "hello world!");
    check("ED-06 reject insertion beyond end", !edit.insertText(1, 99, "x") && editDoc.getLine(1) == "hello world!");
    check("ED-07 reject empty insertion", !edit.insertText(1, 0, ""));
    check("ED-08 reject line break in single-line insertion", !edit.insertText(1, 0, "x\ny"));
    check("ED-09 reject invalid line number", !edit.insertText(0, 0, "x"));
    check("ED-10 delete character", edit.deleteCharacter(1, 11) && editDoc.getLine(1) == "hello world");
    check("ED-11 undo character deletion", edit.undoLastOperation() && editDoc.getLine(1) == "hello world!");
    check("ED-12 insert line", edit.insertLine(2, "second") && editDoc.lineCount() == 2 && editDoc.getLine(2) == "second");
    check("ED-13 delete line", edit.deleteLine(2) && editDoc.lineCount() == 1);
    check("ED-14 undo line deletion", edit.undoLastOperation() && editDoc.lineCount() == 2 && editDoc.getLine(2) == "second");
    check("ED-15 delete line range", edit.deleteLineRange(1, 2) && editDoc.lineCount() == 1 && editDoc.getLine(1).empty());
    check("ED-16 undo deleted line range", edit.undoLastOperation() && editDoc.lineCount() == 2 && editDoc.getLine(1) == "hello world!");
    check("ED-17 replace text", edit.replaceText(1, 0, 5, "HELLO") && editDoc.getLine(1) == "HELLO world!");
    check("ED-18 undo replacement", edit.undoLastOperation() && editDoc.getLine(1) == "hello world!");
    check("ED-19 undo empty history returns false", (edit.clearUndoHistory(), !edit.undoLastOperation()));

    // Search behaviour: byte columns, case handling and whole-word matching.
    TextDocument searchDoc;
    searchDoc.loadText("the cat sat on the mat\nThe Cat\nconcatenate cat");
    TextSearch search(searchDoc);
    std::vector<Match> matches;
    SearchStatus status = search.searchText("cat", matches, true, false);
    check("SR-01 case-sensitive search finds three matches", status == SearchStatus::Found && matches.size() == 3);
    check("SR-02 match coordinates are one-based lines and zero-based columns",
          hasMatch(matches, 1, 4) && hasMatch(matches, 3, 12));
    status = search.searchText("cat", matches, false, false);
    check("SR-03 case-insensitive search finds four matches", status == SearchStatus::Found && matches.size() == 4);
    status = search.searchText("cat", matches, false, true);
    check("SR-04 whole-word search excludes concatenate", status == SearchStatus::Found && matches.size() == 3);
    status = search.searchText("", matches);
    check("SR-05 empty pattern is rejected", status == SearchStatus::EmptyPattern);
    status = search.searchText("missing", matches);
    check("SR-06 absent pattern reports NotFound", status == SearchStatus::NotFound);

    // Replacement and one-step undo for Replace All.
    TextDocument replaceDoc;
    replaceDoc.loadText("cat sits cat");
    EditOperations replaceEdits(replaceDoc);
    TextReplace replace(replaceDoc, replaceEdits);
    ReplaceResult rr = replace.replaceFirstOccurrence("cat", "dog");
    check("RP-01 Replace First changes one occurrence", rr.status == ReplaceStatus::Success && rr.replacements == 1 && replaceDoc.getLine(1) == "dog sits cat");
    check("RP-02 undo Replace First", replaceEdits.undoLastOperation() && replaceDoc.getLine(1) == "cat sits cat");
    rr = replace.replaceAllOccurrences("cat", "fox");
    check("RP-03 Replace All changes both whole substrings", rr.status == ReplaceStatus::Success && rr.replacements == 2 && replaceDoc.getLine(1) == "fox sits fox");
    check("RP-04 Replace All is one undo operation", replaceEdits.undoLastOperation() && replaceDoc.getLine(1) == "cat sits cat");
    rr = replace.replaceFirstOccurrence("", "x");
    check("RP-05 empty replacement pattern is rejected", rr.status == ReplaceStatus::EmptyPattern);
    rr = replace.replaceFirstOccurrence("cat", "x\ny");
    check("RP-06 replacement containing newline is rejected", rr.status == ReplaceStatus::InvalidText && replaceDoc.getLine(1) == "cat sits cat");

    // UTF-8 boundary protection (é is two bytes in UTF-8).
    TextDocument utfDoc;
    utfDoc.loadText("café");
    EditOperations utfEdit(utfDoc);
    check("UTF-01 insertion inside UTF-8 character is rejected",
          !utfEdit.insertText(1, 4, "X") && utfDoc.getLine(1) == "café");
    check("UTF-02 delete UTF-8 character as a whole", utfEdit.deleteCharacter(1, 3) && utfDoc.getLine(1) == "caf");
    check("UTF-03 undo restores UTF-8 character", utfEdit.undoLastOperation() && utfDoc.getLine(1) == "café");

    // UndoStack capacity and basic LIFO behaviour.
    UndoStack stack(2);
    check("US-01 push first record", stack.push(UndoRecord(EditType::InsertText, 0, 0, "", "a")));
    check("US-02 push second record", stack.push(UndoRecord(EditType::InsertText, 0, 1, "", "b")));
    check("US-03 stack reports full", stack.isFull() && stack.size() == 2);
    check("US-04 third push evicts oldest", stack.push(UndoRecord(EditType::InsertText, 0, 2, "", "c")) && stack.size() == 2);
    UndoRecord record;
    check("US-05 pop returns newest record first", stack.pop(record) && record.newText == "c");
    check("US-06 next pop returns next newest record", stack.pop(record) && record.newText == "b");
    check("US-07 empty stack rejects pop", !stack.pop(record));

    // Input validation helpers.
    int choice = -1;
    check("IV-01 valid menu choice accepted", InputValidator::validateMenuChoice("12", 0, 18, choice) && choice == 12);
    check("IV-02 non-numeric menu choice rejected", !InputValidator::validateMenuChoice("abc", 0, 18, choice));
    check("IV-03 out-of-range menu choice rejected", !InputValidator::validateMenuChoice("19", 0, 18, choice));
    check("IV-04 valid line number accepted", InputValidator::isValidLineNumber(1, 3));
    check("IV-05 line zero rejected", !InputValidator::isValidLineNumber(0, 3));
    check("IV-06 position at line end accepted", InputValidator::isValidPosition(5, 5));
    check("IV-07 position beyond line end rejected", !InputValidator::isValidPosition(6, 5));
    check("IV-08 empty search string detected", InputValidator::isSearchStringEmpty(""));

    std::cout << "\n==============================\n"
              << "Tests passed: " << passed << '\n'
              << "Tests failed: " << failed << '\n'
              << "==============================\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
