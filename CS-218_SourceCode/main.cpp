#include "DisplayManager.h"
#include "EditOperations.h"
#include "FileManager.h"
#include "TextReplace.h"
#include "TextSearch.h"
#include "InputValidator.h"

#include <climits>
#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// Show unnamed documents consistently in the console status panel.
std::string formatFilename(const std::string& filename) {
    return filename.empty() ? "Untitled" : filename;
}

void printEditorScreen(const TextDocument& document,
                       const std::string& filename,
                       bool modified) {
    // Keep the menu compact by previewing at most five lines.
    const std::size_t lineCount = document.lineCount();
    const std::string allText = document.getAllText();

    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "  TEXT EDITOR";
    std::cout << "      File: " << formatFilename(filename);
    std::cout << (modified ? "      [Unsaved]" : "      [Saved]") << '\n';
    std::cout << "============================================================\n";

    if (lineCount == 0) {
        std::cout << "  (empty document)\n";
    } else {
        for (std::size_t line = 1; line <= lineCount && line <= 5; ++line) {
            std::cout << "  " << line << ": " << document.getLine(line) << '\n';
        }
        if (lineCount > 5) {
            std::cout << "  ... " << (lineCount - 5) << " more line(s)\n";
        }
    }

    std::cout << "------------------------------------------------------------\n";
    std::cout << "  Status: " << lineCount << " lines   |   " << allText.size()
              << " characters\n";
    std::cout << "============================================================\n";
    std::cout << "  FILE    : [01] New           [02] Open            [03] Save                [04] Save As     [00] Exit\n";
    std::cout << "  VIEW    : [05] Document      [06] One Line        [17] Stats               [18] All Lines\n";
    std::cout << "  EDIT    : [07] Insert        [08] Delete          [09] Delete Word\n";
    std::cout << "            [10] Insert Line   [11] Delete Line     [12] Delete Line Range\n";
    std::cout << "  SEARCH  : [13] Search        [14] Replace First   [15] Replace All\n";
    std::cout << "  HISTORY : [16] Undo\n";
    std::cout << "============================================================\n";
}

bool readAndValidateText(const std::string& prompt, std::string& text) {
    if (!InputValidator::readLine(prompt, text)) return false;
    if (!InputValidator::validateUserInput(text)) {
        std::cout << "Input contains unsupported control characters or is too long.\n";
        return false;
    }
    return true;
}

bool readLineNumber(const TextDocument& document, const std::string& prompt,
                    int& line) {
    // The console API accepts int values, while document indexes use size_t.
    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
        std::cerr << "The document has too many lines for the console interface.\n";
        return false;
    }
    return InputValidator::readIntInRange(
        prompt, 1, static_cast<int>(document.lineCount()), line);
}

} // namespace

int main() {
#ifdef _WIN32
    // File names and text are handled as UTF-8 throughout.
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    TextDocument document;
    std::string filename;
    bool modified = false;

    FileManager files(document, filename, modified);
    EditOperations edits(document);
    DisplayManager display(document);
    TextSearch search(document);
    TextReplace replace(document, edits);

    files.createNewDocument();
    bool running = true;

    // Each loop iteration redraws the current status and handles one menu action.
    while (running) {
        printEditorScreen(document, filename, files.isModified());

        int choice = 0;
        if (!InputValidator::readIntInRange("Enter choice > ", 0, 18, choice)) {
            running = false;
            continue;
        }

        try {
            std::string text;
            std::string secondText;
            int line = 0;
            int position = 0;
            int count = 0;
            bool succeeded = false;

            // Route menu commands through the shared document and editing services.
            switch (choice) {
                case 0:
                    if (files.resolveUnsavedChanges()) running = false;
                    break;
                case 1:
                    if (files.createNewDocument()) edits.clearUndoHistory();
                    break;
                case 2:
                    if (InputValidator::readLine("Text filename or path to open: ", text) &&
                        files.openFile(text)) {
                        edits.clearUndoHistory();
                    }
                    break;
                case 3:
                    if (files.isFileOpen()) files.saveFile();
                    else std::cout << "No document is open.\n";
                    break;
                case 4:
                    if (InputValidator::readLine("New filename: ", text) &&
                        files.isFileOpen()) {
                        files.saveAs(text);
                    } else if (!files.isFileOpen()) {
                        std::cout << "No document is open.\n";
                    }
                    break;
                case 5:
                    display.displayDocument();
                    break;
                case 6:
                    if (readLineNumber(document, "Line number: ", line)) {
                        display.displayLine(line);
                    }
                    break;
                case 7:
                    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
                        std::cerr << "The document has too many lines for the console interface.\n";
                        break;
                    }
                    if (!readLineNumber(document, "Line number: ", line) ||
                        !InputValidator::readIntInRange(
                            "Column (0-based): ", 0, INT_MAX, position) ||
                        !readAndValidateText("Text to insert: ", text)) {
                        break;
                    }
                    succeeded = edits.insertText(static_cast<std::size_t>(line),
                                                 static_cast<std::size_t>(position), text);
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Text inserted.\n" :
                                               "Insertion was rejected. Check the line and column.\n");
                    break;
                case 8:
                    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
                        std::cerr << "The document has too many lines for the console interface.\n";
                        break;
                    }
                    if (!readLineNumber(document, "Line number: ", line) ||
                        !InputValidator::readIntInRange(
                            "Start column (0-based): ", 0, INT_MAX, position) ||
                        !InputValidator::readIntInRange(
                            "Number of characters: ", 1, INT_MAX, count)) {
                        break;
                    }
                    succeeded = edits.deleteRange(static_cast<std::size_t>(line),
                                                  static_cast<std::size_t>(position),
                                                  static_cast<std::size_t>(count));
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Text deleted.\n" :
                                               "Deletion was rejected. Check the range.\n");
                    break;
                case 9:
                    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
                        std::cerr << "The document has too many lines for the console interface.\n";
                        break;
                    }
                    if (!readLineNumber(document, "Line number: ", line) ||
                        !InputValidator::readIntInRange(
                            "Start column (0-based): ", 0, INT_MAX, position)) {
                        break;
                    }
                    succeeded = edits.deleteWord(static_cast<std::size_t>(line),
                                                 static_cast<std::size_t>(position));
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Word deleted.\n" :
                                               "No word was deleted at that position.\n");
                    break;
                case 10:
                    if (document.lineCount() >= static_cast<std::size_t>(INT_MAX)) {
                        std::cout << "The document has too many lines for another line.\n";
                        break;
                    }
                    if (!InputValidator::readIntInRange(
                            "Insert before line (or last line + 1): ", 1,
                            static_cast<int>(document.lineCount()) + 1, line) ||
                        !readAndValidateText("Line text: ", text)) {
                        break;
                    }
                    succeeded = edits.insertLine(static_cast<std::size_t>(line), text);
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Line inserted.\n" :
                                               "Line insertion was rejected.\n");
                    break;
                case 11:
                    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
                        std::cerr << "The document has too many lines for the console interface.\n";
                        break;
                    }
                    if (!readLineNumber(document, "Line number: ", line)) break;
                    succeeded = edits.deleteLine(static_cast<std::size_t>(line));
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Line deleted.\n" :
                                               "Line deletion was rejected.\n");
                    break;
                case 12:
                    if (document.lineCount() > static_cast<std::size_t>(INT_MAX)) {
                        std::cerr << "The document has too many lines for the console interface.\n";
                        break;
                    }
                    if (!readLineNumber(document, "First line: ", line) ||
                        !InputValidator::readIntInRange(
                            "Number of lines: ", 1, static_cast<int>(document.lineCount()), count)) {
                        break;
                    }
                    succeeded = edits.deleteLineRange(static_cast<std::size_t>(line),
                                                      static_cast<std::size_t>(count));
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Lines deleted.\n" :
                                               "Line deletion was rejected. Check the range.\n");
                    break;
                case 13:
                    if (readAndValidateText("Search text: ", text)) {
                        search.displaySearchResults(text);
                    }
                    break;
                case 14:
                case 15:
                    if (!readAndValidateText("Search text: ", text) ||
                        !readAndValidateText("Replacement text: ", secondText)) {
                        break;
                    }
                    {
                        const ReplaceResult result = choice == 14
                            ? replace.replaceFirstOccurrence(text, secondText)
                            : replace.replaceAllOccurrences(text, secondText);
                        if (result.status == ReplaceStatus::Success) {
                            if (text != secondText) modified = true;
                            std::cout << TextReplace::describe(result.status) << " "
                                      << result.replacements << " occurrence(s).\n";
                        } else {
                            std::cout << TextReplace::describe(result.status) << '\n';
                        }
                    }
                    break;
                case 16:
                    succeeded = edits.undoLastOperation();
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Last operation undone.\n" :
                                               "There is no operation to undo.\n");
                    break;
                case 17:
                    display.displayStatistics();
                    break;
                case 18:
                    display.displayAllLines();
                    break;
                default:
                    break;
            }
        } catch (const std::exception& error) {
            std::cerr << "Operation failed: " << error.what() << '\n';
        }
    }

    if (files.isModified()) {
        try {
            if (!files.resolveUnsavedChanges()) {
                std::cerr << "Exiting would leave unsaved changes.\n";
                return 1;
            }
        } catch (const std::exception& error) {
            std::cerr << "Could not resolve unsaved changes: " << error.what() << '\n';
            return 1;
        }
    }
    return 0;
}
