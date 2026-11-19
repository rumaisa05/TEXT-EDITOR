#include "DisplayManager.h"
#include "EditOperations.h"
#include "FileManager.h"
#include "InputValidator.h"
#include "PathUtil.h"
#include "TextDocument.h"
#include "TextReplace.h"
#include "TextSearch.h"
#include "UndoStack.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {
int passed = 0;
int failed = 0;

void check(const std::string& id, bool condition) {
    if (condition) {
        ++passed;
        std::cout << "[PASS] " << id << '\n';
    } else {
        ++failed;
        std::cout << "[FAIL] " << id << '\n';
    }
}

bool hasMatch(const std::vector<Match>& matches, std::size_t line,
              std::size_t column) {
    for (const Match& match : matches) {
        if (match.line == line && match.column == column) return true;
    }
    return false;
}

class ConsoleRedirect {
public:
    explicit ConsoleRedirect(const std::string& input = "")
        : input_(input),
          output_(),
          oldInput_(std::cin.rdbuf(input_.rdbuf())),
          oldOutput_(std::cout.rdbuf(output_.rdbuf())) {}

    ~ConsoleRedirect() {
        std::cin.rdbuf(oldInput_);
        std::cout.rdbuf(oldOutput_);
        std::cin.clear();
        std::cout.clear();
    }

    std::string output() const { return output_.str(); }

private:
    std::istringstream input_;
    std::ostringstream output_;
    std::streambuf* oldInput_;
    std::streambuf* oldOutput_;
};

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static unsigned long long sequence = 0;
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        const std::filesystem::path base = std::filesystem::temp_directory_path();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = base / ("cs218_editor_tests_" + std::to_string(stamp) +
                            "_" + std::to_string(sequence++));
            std::error_code error;
            if (std::filesystem::create_directory(path_, error)) return;
            if (error && error != std::errc::file_exists) {
                throw std::filesystem::filesystem_error(
                    "could not create test directory", path_, error);
            }
        }
        throw std::runtime_error("could not create a unique test directory");
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    std::string file(const std::string& name) const {
        return (path_ / pathFromUtf8(name)).u8string();
    }

private:
    std::filesystem::path path_;
};

bool writeFile(const std::string& filename, const std::string& content) {
    std::ofstream output(pathFromUtf8(filename), std::ios::binary);
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(output);
}

std::string readFile(const std::string& filename) {
    std::ifstream input(pathFromUtf8(filename), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void testFileOperations() {
    TemporaryDirectory directory;
    const std::string source = directory.file("source.txt");
    const std::string renamed = directory.file("renamed.txt");
    const std::string missing = directory.file("missing.txt");
    bool created = false;
    bool unnamedSaveRejected = false;
    bool opened = false;
    bool missingRejected = false;
    bool missingPreservedDocument = false;
    bool savedCurrentFile = false;
    bool savedAs = false;
    bool exitSaveWorked = false;
    bool exitCancelPreservedChanges = false;
    bool exitDiscardWorked = false;
    bool closed = false;
    bool invalidPathRejected = false;

    {
        ConsoleRedirect console("s\nc\nd\n");
        TextDocument document;
        std::string filename;
        bool modified = false;
        FileManager files(document, filename, modified);

        created = files.createNewDocument() && files.isFileOpen() &&
                  document.isEmpty();
        unnamedSaveRejected = !files.saveFile();
        const bool sourceWritten = writeFile(source, "original");
        opened = sourceWritten && files.openFile(source, false) &&
                 document.getAllText() == "original" && filename == source;

        missingRejected = !files.openFile(missing, false);
        missingPreservedDocument = document.getAllText() == "original" &&
                                   filename == source;

        document.loadText("saved");
        modified = true;
        savedCurrentFile = files.saveFile() && readFile(source) == "saved" &&
                           !files.isModified();

        document.loadText("renamed content");
        modified = true;
        savedAs = files.saveAs(renamed, false) && filename == renamed &&
                  readFile(renamed) == "renamed content" &&
                  !files.isModified();

        document.loadText("save on exit");
        modified = true;
        exitSaveWorked = files.resolveUnsavedChanges() &&
                         readFile(renamed) == "save on exit" &&
                         !files.isModified();

        document.loadText("pending edit");
        modified = true;
        exitCancelPreservedChanges = !files.resolveUnsavedChanges() &&
                                     files.isModified() &&
                                     document.getAllText() == "pending edit";
        exitDiscardWorked = files.resolveUnsavedChanges() &&
                            !files.isModified() &&
                            document.getAllText() == "pending edit";

        closed = files.closeFile() && !files.isFileOpen();
        invalidPathRejected = !files.openFile("invalid?.txt", false);
    }

    check("FILE-01 create a new open document", created);
    check("FILE-02 reject Save before a filename is assigned", unnamedSaveRejected);
    check("FILE-03 open an existing file", opened);
    check("FILE-04 reject a missing file without replacing the document",
          missingRejected && missingPreservedDocument);
    check("FILE-05 save changes to the current file", savedCurrentFile);
    check("FILE-06 Save As writes to and adopts the new filename", savedAs);
    check("FILE-07 resolve exit by saving changes", exitSaveWorked);
    check("FILE-08 cancel exit without losing edits", exitCancelPreservedChanges);
    check("FILE-09 discard exit changes", exitDiscardWorked);
    check("FILE-10 close a document", closed);
    check("FILE-11 reject an invalid filename", invalidPathRejected);
}

void testTextDisplay() {
    TextDocument document;
    document.loadText("one\ntwo");
    DisplayManager display(document);
    std::string output;
    {
        ConsoleRedirect console;
        display.displayDocument();
        display.displayLine(2);
        display.displayAllLines();
        display.displayStatistics();
        display.displayLine(3);
        output = console.output();
    }

    check("DISPLAY-01 show all document text",
          output.find("one\ntwo\n") != std::string::npos);
    check("DISPLAY-02 show individual and numbered lines",
          output.find("2: two\n") != std::string::npos &&
              output.find("1: one\n2: two\n") != std::string::npos);
    check("DISPLAY-03 report line/character counts and invalid lines",
          output.find("Lines: 2, Characters: 7\n") != std::string::npos &&
              output.find("Error: invalid line number 3.") != std::string::npos);
}
}

int main() {
    TextDocument document;
    check("DOC-01 new document starts with one empty line",
          document.lineCount() == 1 && document.isEmpty());
    document.loadText("a\r\nb\r");
    check("DOC-02 normalize CRLF/CR and preserve a trailing newline",
          document.lineCount() == 3 && document.getAllText() == "a\nb\n" &&
              document.getLine(3).empty());

    TextDocument editDocument;
    editDocument.loadText("cat sat");
    EditOperations edits(editDocument);
    check("EDIT-01 insert text at a valid position",
          edits.insertText(1, 3, ",") &&
              editDocument.getLine(1) == "cat, sat");
    check("EDIT-02 undo text insertion",
          edits.undoLastOperation() && editDocument.getLine(1) == "cat sat");
    check("EDIT-03 reject out-of-range insertion without changing text",
          !edits.insertText(1, 99, "x") &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-04 delete a character and undo it",
          edits.deleteCharacter(1, 0) &&
              editDocument.getLine(1) == "at sat" &&
              edits.undoLastOperation() &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-05 delete a word and undo it",
          edits.deleteWord(1, 3) && editDocument.getLine(1) == "cat" &&
              edits.undoLastOperation() &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-06 delete a range and undo it",
          edits.deleteRange(1, 4, 3) && editDocument.getLine(1) == "cat " &&
              edits.undoLastOperation() &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-07 reject an invalid deletion range",
          !edits.deleteRange(1, 6, 3) &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-08 insert and undo a line",
          edits.insertLine(2, "second") && editDocument.lineCount() == 2 &&
              edits.undoLastOperation() && editDocument.lineCount() == 1);
    check("EDIT-09 delete the only line and undo it",
          edits.deleteLine(1) && editDocument.isEmpty() &&
              edits.undoLastOperation() &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-10 delete and undo a line range",
          edits.insertLine(2, "second") &&
              edits.deleteLineRange(1, 2) && editDocument.isEmpty() &&
              edits.undoLastOperation() &&
              editDocument.getAllText() == "cat sat\nsecond");
    check("EDIT-11 replace text and undo it",
          edits.replaceText(1, 0, 3, "dog") &&
              editDocument.getLine(1) == "dog sat" &&
              edits.undoLastOperation() &&
              editDocument.getLine(1) == "cat sat");
    check("EDIT-12 undo a multi-line document replacement",
          edits.replaceDocumentText("first\nsecond") &&
              editDocument.getAllText() == "first\nsecond" &&
              edits.undoLastOperation() &&
              editDocument.getAllText() == "cat sat\nsecond");

    TextDocument searchDocument;
    searchDocument.loadText("cat scatter\nCAT cat");
    TextSearch search(searchDocument);
    std::vector<Match> matches;
    SearchStatus searchStatus =
        search.searchText("cat", matches, true, false);
    check("SEARCH-01 find case-sensitive matches and report locations",
          searchStatus == SearchStatus::Found && matches.size() == 3 &&
              hasMatch(matches, 1, 0) && hasMatch(matches, 1, 5) &&
              hasMatch(matches, 2, 4));
    searchStatus = search.searchText("cat", matches, false, false);
    check("SEARCH-02 find matches without case sensitivity",
          searchStatus == SearchStatus::Found && matches.size() == 4);
    searchStatus = search.searchText("cat", matches, false, true);
    check("SEARCH-03 whole-word search excludes an embedded match",
          searchStatus == SearchStatus::Found && matches.size() == 3 &&
              !hasMatch(matches, 1, 5));
    check("SEARCH-04 report missing and empty search strings",
          search.searchText("missing", matches) == SearchStatus::NotFound &&
              search.searchText("", matches) == SearchStatus::EmptyPattern);
    TextDocument emptySearchDocument;
    TextSearch emptySearch(emptySearchDocument);
    check("SEARCH-05 reject a search on an empty document",
          emptySearch.searchText("word", matches) ==
              SearchStatus::EmptyDocument);

    TextDocument replaceDocument;
    replaceDocument.loadText("cat sits cat");
    EditOperations replaceEdits(replaceDocument);
    TextReplace replace(replaceDocument, replaceEdits);
    ReplaceResult replacement =
        replace.replaceFirstOccurrence("cat", "dog");
    check("REPLACE-01 replace one occurrence",
          replacement.status == ReplaceStatus::Success &&
              replacement.replacements == 1 &&
              replaceDocument.getLine(1) == "dog sits cat");
    check("REPLACE-02 undo a replacement",
          replaceEdits.undoLastOperation() &&
              replaceDocument.getLine(1) == "cat sits cat");
    replacement = replace.replaceAllOccurrences("cat", "fox");
    check("REPLACE-03 replace all occurrences as one undoable edit",
          replacement.status == ReplaceStatus::Success &&
              replacement.replacements == 2 &&
              replaceDocument.getLine(1) == "fox sits fox" &&
              replaceEdits.undoLastOperation() &&
              replaceDocument.getLine(1) == "cat sits cat");
    replacement = replace.replaceOccurrenceAt(Match{1, 9}, "cat", "dog");
    check("REPLACE-04 replace a specified occurrence",
          replacement.status == ReplaceStatus::Success &&
              replaceDocument.getLine(1) == "cat sits dog");
    replacement = replace.replaceOccurrenceAt(Match{1, 0}, "dog", "fox");
    check("REPLACE-05 reject a location that does not contain the pattern",
          replacement.status == ReplaceStatus::NoMatchAtLocation &&
              replaceDocument.getLine(1) == "cat sits dog");
    replacement = replace.replaceFirstOccurrence("", "x");
    check("REPLACE-06 reject an empty pattern",
          replacement.status == ReplaceStatus::EmptyPattern);
    replacement = replace.replaceFirstOccurrence("cat", "x\ny");
    check("REPLACE-07 reject replacement text containing a line break",
          replacement.status == ReplaceStatus::InvalidText);
    TextDocument emptyReplaceDocument;
    EditOperations emptyReplaceEdits(emptyReplaceDocument);
    TextReplace emptyReplace(emptyReplaceDocument, emptyReplaceEdits);
    check("REPLACE-08 reject replacement in an empty document",
          emptyReplace.replaceFirstOccurrence("word", "text").status ==
              ReplaceStatus::EmptyDocument);

    UndoStack undo(2);
    const bool undoBasics =
        undo.push(UndoRecord(EditType::InsertText, 0, 0, "", "a")) &&
        undo.push(UndoRecord(EditType::InsertText, 0, 1, "", "b")) &&
        undo.isFull() &&
        undo.push(UndoRecord(EditType::InsertText, 0, 2, "", "c"));
    UndoRecord record;
    check("UNDO-01 bounded stack evicts oldest and preserves LIFO order",
          undoBasics && undo.size() == 2 && undo.pop(record) &&
              record.newText == "c" && undo.pop(record) &&
              record.newText == "b" && !undo.pop(record));

    testFileOperations();
    testTextDisplay();

    int choice = -1;
    check("INPUT-01 accept valid menu choices and reject invalid ones",
          InputValidator::validateMenuChoice("19", 0, 19, choice) &&
              choice == 19 &&
              !InputValidator::validateMenuChoice("abc", 0, 19, choice) &&
              !InputValidator::validateMenuChoice("20", 0, 19, choice));
    check("INPUT-02 validate line, position, and filename boundaries",
          InputValidator::isValidLineNumber(1, 3) &&
              !InputValidator::isValidLineNumber(0, 3) &&
              InputValidator::isValidPosition(5, 5) &&
              !InputValidator::isValidPosition(6, 5) &&
              InputValidator::isValidFilename("notes.txt") &&
              !InputValidator::isValidFilename("bad?.txt"));

    int recoveredChoice = -1;
    std::string inputMessages;
    bool invalidInputRecovered = false;
    {
        ConsoleRedirect console("not-a-number\n2\n");
        invalidInputRecovered =
            InputValidator::readIntInRange("choice: ", 0, 3, recoveredChoice);
        inputMessages = console.output();
    }
    check("INPUT-03 recover from invalid input without terminating",
          invalidInputRecovered && recoveredChoice == 2 &&
              inputMessages.find("Invalid input.") != std::string::npos);

    std::cout << "\n==============================\n"
              << "Tests passed: " << passed << '\n'
              << "Tests failed: " << failed << '\n'
              << "==============================\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
