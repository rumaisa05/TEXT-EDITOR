#include "DisplayManager.h"
#include "EditOperations.h"
#include "FileManager.h"
#include "TextReplace.h"
#include "TextSearch.h"
#include "InputValidator.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

bool colorsEnabled = false;
const char* const kCyan = "\x1b[96m";
const char* const kGreen = "\x1b[92m";
const char* const kYellow = "\x1b[93m";
const char* const kBlue = "\x1b[94m";
const char* const kRed = "\x1b[91m";
const char* const kDim = "\x1b[2m";
const char* const kReset = "\x1b[0m";

bool enableConsoleColors() {
#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    return output != INVALID_HANDLE_VALUE &&
           GetConsoleMode(output, &mode) &&
           SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    return std::getenv("NO_COLOR") == nullptr && isatty(STDOUT_FILENO) != 0;
#endif
}

std::string colorize(const std::string& text, const char* color) {
    return colorsEnabled ? std::string(color) + text + kReset : text;
}

void printMenuSection(const std::string& title) {
    std::cout << '\n' << colorize("  " + title, kCyan) << '\n';
}

void printMenuOption(int choice, const std::string& label) {
    const std::string number = choice < 10
        ? "0" + std::to_string(choice)
        : std::to_string(choice);
    const std::string prefix = "  [" + number + "] ";
    std::cout << colorize(prefix, kYellow) << label;
    const std::size_t visibleWidth = prefix.size() + label.size();
    if (visibleWidth < 34) {
        std::cout << std::string(34 - visibleWidth, ' ');
    }
}

void printMenuRow(int firstChoice, const std::string& firstLabel,
                  int secondChoice, const std::string& secondLabel) {
    printMenuOption(firstChoice, firstLabel);
    printMenuOption(secondChoice, secondLabel);
    std::cout << '\n';
}

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
    const std::string border = "+" + std::string(60, '-') + "+";
    const std::string title = "TEXT EDITOR";
    const std::size_t titlePadding = (60 - title.size()) / 2;

    std::cout << "\n";
    std::cout << "  " << border << '\n';
    std::cout << "  |" << std::string(titlePadding, ' ')
              << colorize(title, kCyan)
              << std::string(60 - titlePadding - title.size(), ' ')
              << "|\n";
    std::cout << "  " << border << '\n';
    std::cout << "  File   : " << colorize(formatFilename(filename), kGreen) << '\n';
    std::cout << "  Status : "
              << colorize(modified ? "[ UNSAVED ]" : "[ SAVED ]",
                          modified ? kYellow : kGreen) << '\n';

    if (lineCount == 0) {
        std::cout << colorize("  (empty document)\n", kDim);
    } else {
        for (std::size_t line = 1; line <= lineCount && line <= 5; ++line) {
            std::cout << "  " << colorize(std::to_string(line), kBlue)
                      << " | " << document.getLine(line) << '\n';
        }
        if (lineCount > 5) {
            std::cout << colorize("  ... " + std::to_string(lineCount - 5) +
                                  " more line(s)\n", kDim);
        }
    }

    std::cout << "  ------------------------------------------------------------\n";
    std::cout << "  " << colorize("DOCUMENT", kCyan) << "  "
              << lineCount << " lines  |  " << allText.size() << " characters\n";

    printMenuSection("FILE");
    printMenuRow(1, "New document", 2, "Open file");
    printMenuRow(3, "Save", 4, "Save as");
    printMenuRow(0, "Exit", 19, "Return to writing");

    printMenuSection("DOCUMENT");
    printMenuRow(5, "View document", 6, "View one line");
    printMenuRow(17, "Statistics", 18, "View all lines");

    printMenuSection("EDIT");
    printMenuRow(7, "Insert text", 8, "Delete text");
    printMenuRow(9, "Delete word", 10, "Insert line");
    printMenuRow(11, "Delete line", 12, "Delete line range");

    printMenuSection("SEARCH & HISTORY");
    printMenuRow(13, "Search", 14, "Replace first");
    printMenuRow(15, "Replace all", 16, "Undo");
    std::cout << '\n';
}

bool readAndValidateText(const std::string& prompt, std::string& text) {
    if (!InputValidator::readLine(colorize(prompt, kCyan), text)) return false;
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
        colorize(prompt, kCyan), 1, static_cast<int>(document.lineCount()), line);
}

bool runWritingMode(TextDocument& document, EditOperations& edits,
                    FileManager& files, const std::string& filename,
                    bool& modified) {
    std::cout << "\n" << colorize("  WRITE MODE", kCyan)
              << "  " << colorize(":w", kYellow) << " save  |  "
              << colorize(":menu", kYellow) << " menu  |  "
              << colorize(":help", kYellow) << " commands\n";
    bool canFillTrailingEmptyLine =
        document.isEmpty() || document.getLine(document.lineCount()).empty();

    while (true) {
        std::cout << colorize("  " + std::to_string(document.lineCount()) +
                              " lines  |  " +
                              std::to_string(document.getAllText().size()) +
                              " characters\n", kBlue);
        std::string text;
        if (!InputValidator::readLine(colorize("  write> ", kCyan), text)) return false;
        if (!InputValidator::validateUserInput(text)) {
            std::cout << colorize("Input contains unsupported control characters or is too long.\n",
                                  kRed);
            continue;
        }

        if (text == ":menu") return true;
        if (text == ":help") {
            std::cout << "  Type a line of text to append it to the document.\n"
                      << "  :w or :save  Save the current document\n"
                      << "  :menu        Open the numbered menu\n"
                      << "  :help        Show these commands\n";
            continue;
        }
        if (text == ":w" || text == ":save") {
            if (filename.empty()) {
                std::string newFilename;
                if (!InputValidator::readLine(
                        colorize("Enter path to save the file: ", kCyan), newFilename)) {
                    return false;
                }
                files.saveAs(newFilename);
            } else {
                files.saveFile();
            }
            continue;
        }

        bool succeeded = false;
        if (canFillTrailingEmptyLine && !text.empty()) {
            const std::size_t lastLine = document.lineCount();
            succeeded = edits.insertText(
                lastLine, document.lineLength(lastLine), text);
        } else if (document.lineCount() < static_cast<std::size_t>(INT_MAX)) {
            succeeded = edits.insertLine(document.lineCount() + 1, text);
        } else {
            std::cout << colorize("The document has too many lines for another line.\n",
                                  kRed);
            continue;
        }

        if (succeeded) {
            modified = true;
            canFillTrailingEmptyLine = false;
        } else {
            std::cout << colorize("The line could not be added.\n", kRed);
        }
    }
}

} // namespace

int main() {
#ifdef _WIN32
    // File names and text are handled as UTF-8 throughout.
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    colorsEnabled = enableConsoleColors();
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

    try {
        if (!runWritingMode(document, edits, files, filename, modified)) {
            running = false;
        }
    } catch (const std::exception& error) {
        std::cerr << "Operation failed: " << error.what() << '\n';
    }

    // Each loop iteration redraws the current status and handles one menu action.
    while (running) {
        printEditorScreen(document, filename, files.isModified());

        int choice = 0;
        if (!InputValidator::readIntInRange(
                colorize("Enter choice > ", kCyan), 0, 19, choice)) {
            running = false;
            continue;
        }

        bool returnToWriting = false;
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
                    returnToWriting = files.createNewDocument();
                    if (returnToWriting) edits.clearUndoHistory();
                    break;
                case 2:
                    if (InputValidator::readLine(
                            colorize("Enter the full path of the file to open: ", kCyan), text) &&
                        files.openFile(text)) {
                        edits.clearUndoHistory();
                        returnToWriting = true;
                    }
                    break;
                case 3:
                    if (files.isFileOpen()) returnToWriting = files.saveFile();
                    else std::cout << "No document is open.\n";
                    break;
                case 4:
                    if (InputValidator::readLine(colorize("New filename: ", kCyan), text) &&
                        files.isFileOpen()) {
                        returnToWriting = files.saveAs(text);
                    } else if (!files.isFileOpen()) {
                        std::cout << "No document is open.\n";
                    }
                    break;
                case 5:
                    display.displayDocument();
                    returnToWriting = true;
                    break;
                case 6:
                    if (readLineNumber(document, "Line number: ", line)) {
                        display.displayLine(line);
                        returnToWriting = true;
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
                    returnToWriting = succeeded;
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
                    returnToWriting = succeeded;
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
                    returnToWriting = succeeded;
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
                    returnToWriting = succeeded;
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
                    returnToWriting = succeeded;
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
                    returnToWriting = succeeded;
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Lines deleted.\n" :
                                               "Line deletion was rejected. Check the range.\n");
                    break;
                case 13:
                    if (readAndValidateText("Search text: ", text)) {
                        search.displaySearchResults(text);
                        returnToWriting = true;
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
                            returnToWriting = true;
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
                    returnToWriting = succeeded;
                    if (succeeded) modified = true;
                    std::cout << (succeeded ? "Last operation undone.\n" :
                                               "There is no operation to undo.\n");
                    break;
                case 17:
                    display.displayStatistics();
                    returnToWriting = true;
                    break;
                case 18:
                    display.displayAllLines();
                    returnToWriting = true;
                    break;
                case 19:
                    returnToWriting = true;
                    break;
                default:
                    break;
            }
        } catch (const std::exception& error) {
            std::cerr << "Operation failed: " << error.what() << '\n';
            returnToWriting = false;
        }

        if (running && returnToWriting) {
            try {
                if (!runWritingMode(document, edits, files, filename, modified)) {
                    running = false;
                }
            } catch (const std::exception& error) {
                std::cerr << "Operation failed: " << error.what() << '\n';
            }
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
