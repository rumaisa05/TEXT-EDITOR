// FileManager.cpp  (revised: exception handling)
#include "FileManager.h"
#include "InputValidator.h"
#include "PathUtil.h"

#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>     // std::strerror
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <system_error>

// File operations stage changes before committing them so a failed load or save
// does not damage the open document or an existing destination file.
namespace {

// "what" plus the operating system's reason, when errno has one.
std::string withReason(const std::string& what, int err) {
    if (err == 0) return what;
    return what + ": " + std::strerror(err);
}

std::filesystem::path createTempDirectory(const std::string& targetName) {
    static std::atomic<unsigned long long> nextId(0);
    const std::filesystem::path target = pathFromUtf8(targetName);
    std::filesystem::path parent = target.parent_path();
    if (parent.empty()) parent = ".";

    for (unsigned int attempt = 0; attempt < 100; ++attempt) {
        const std::string directoryName =
            ".cs218_editor_tmp_" + std::to_string(nextId.fetch_add(1));
        const std::filesystem::path directory = parent / directoryName;
        std::error_code error;
        if (std::filesystem::create_directory(directory, error)) return directory;
        if (error && error != std::errc::file_exists) {
            throw std::filesystem::filesystem_error(
                "could not create a temporary save directory", directory, error);
        }
    }
    throw std::runtime_error("could not allocate a unique temporary save directory");
}

// Owns the unique temporary directory created for one save operation.
class TempDirectoryGuard {
public:
    explicit TempDirectoryGuard(const std::filesystem::path& path)
        : path_(path), armed_(true) {}
    ~TempDirectoryGuard() {
        if (armed_) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }
    void disarm() { armed_ = false; }
private:
    TempDirectoryGuard(const TempDirectoryGuard&);
    TempDirectoryGuard& operator=(const TempDirectoryGuard&);
    std::filesystem::path path_;
    bool armed_;
};

} // namespace

FileManager::FileManager(TextDocument& document, std::string& filename, bool& modified)
    : document_(document), filename_(filename), modified_(modified), fileOpen_(false),
#ifdef _WIN32
      crlf_(true)
#else
      crlf_(false)
#endif
{
    modified_ = false;     // nothing has been changed yet
}

bool FileManager::isFileOpen() const { return fileOpen_; }
bool FileManager::isModified() const { return fileOpen_ && modified_; }

// ---------------------------------------------------------------- helpers

// Reads the next NON-EMPTY line from the keyboard.
// Returns false if input has ended (Ctrl+D / Ctrl+Z or a closed pipe), so
// our loops can never run forever. Skipping empty lines also makes us safe
// against a leftover '\n' from an earlier "cin >> x" in main.cpp.
bool FileManager::readAnswer(std::string& answer) {
    while (std::getline(std::cin, answer)) {
        if (!answer.empty()) return true;
    }
    return false;
}

// True if a file with this name can be opened for reading.
bool FileManager::fileExists(const std::string& name) {
    std::error_code error;
    const std::filesystem::path path = pathFromUtf8(name);
    return std::filesystem::exists(path, error) &&
           std::filesystem::is_regular_file(path, error);
}

// Reads a whole file into 'content'. 'content' is touched ONLY on success,
// which is how openFile() keeps the old document safe when something fails.
// Throws std::runtime_error if the file cannot be opened and
// std::ios_base::failure if reading it fails part-way (or it is a folder).
void FileManager::readFromDisk(const std::string& name, std::string& content) {
    errno = 0;
    std::ifstream in(pathFromUtf8(name), std::ios::binary);
    if (!in) {
        throw std::runtime_error(
            withReason("cannot open '" + name + "' for reading", errno));
    }

    // Read in blocks through the stream (not through a raw iterator) so that
    // a failing device sets badbit instead of escaping as an unrelated error.
    std::string temp;
    char buffer[65536];
    do {
        in.read(buffer, sizeof buffer);
        temp.append(buffer, static_cast<std::string::size_type>(in.gcount()));
    } while (in);

    // The loop only ends at end-of-file (normal) or on an error.
    if (in.bad() || !in.eof()) {
        throw std::ios_base::failure("error while reading '" + name +
                                     "' (is it a regular file?)");
    }
    content.swap(temp);                // swap = no copy of the big string
}

// Writes the document SAFELY: first to a temporary file, and only when that
// worked is it moved over the real file. If the disk fills up halfway, the
// old file is still intact instead of being half-erased.
// Throws std::runtime_error if a file cannot be created or replaced and
// std::ios_base::failure if writing, flushing or closing fails. On any
// failure the destination file is left as it was and the temp file is removed.
void FileManager::writeToDisk(const std::string& name) const {
    const std::filesystem::path target = pathFromUtf8(name);
    const std::filesystem::path temporaryDirectory = createTempDirectory(name);
    TempDirectoryGuard guard(temporaryDirectory);
    const std::filesystem::path tmp = temporaryDirectory / target.filename();

    std::string text = document_.getAllText();
    if (crlf_) {
        std::string converted;
        converted.reserve(text.size() + text.size() / 16);
        for (char c : text) {
            if (c == '\n') converted.push_back('\r');
            converted.push_back(c);
        }
        text.swap(converted);
    }
    {
        errno = 0;
        std::ofstream out(tmp, std::ios::binary);
        if (!out) {
            throw std::runtime_error(
                withReason("cannot create '" + name + "' (check folder and permissions)",
                           errno));
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();                   // push buffered data to the OS now ...
        if (!out) {                    // ... so a write error is visible here
            throw std::ios_base::failure("failed while writing to '" + name + "'");
        }
        out.close();
        if (out.fail()) {              // close() can fail too (final flush)
            throw std::ios_base::failure("failed while closing '" + name + "'");
        }
    }

    // Move the finished temporary file over the real one. std::filesystem::rename
    // replaces an existing file on both POSIX and Windows.
    std::error_code renameError;
    std::filesystem::rename(tmp, target, renameError);
    if (renameError) {
#ifdef _WIN32
        // Fallback for a destination that cannot be replaced directly: park the
        // original next to the temp file, move the new one in, restore on failure.
        const std::filesystem::path bak =
            temporaryDirectory / (target.filename().string() + ".editor_bak");
        std::error_code e1;
        std::filesystem::rename(target, bak, e1);
        if (!e1) {
            std::error_code e2;
            std::filesystem::rename(tmp, target, e2);
            if (!e2) {
                guard.disarm();
                std::error_code ignored;
                std::filesystem::remove_all(temporaryDirectory, ignored);
                return;
            }
            std::error_code e3;
            std::filesystem::rename(bak, target, e3);
            const bool restored = !e3;
            if (!restored) guard.disarm();
            throw std::runtime_error(
                "could not replace '" + name + "'" +
                (restored ? " (the original file was restored)"
                          : "; the original file is preserved at '" + bak.string() + "'"));
        }
#endif
        throw std::runtime_error("could not replace '" + name + "': " + renameError.message());
    }
    guard.disarm();
    std::error_code ignored;
    std::filesystem::remove_all(temporaryDirectory, ignored);
}

FileManager::UnsavedChoice FileManager::askUnsavedChoice() const {
    std::string answer;
    while (true) {
        std::cout << "You have unsaved changes. (s)ave, (d)iscard, (c)ancel: ";
        if (!readAnswer(answer)) return CHOICE_CANCEL;      // input ended: be safe
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
        if (c == 's') return CHOICE_SAVE;
        if (c == 'd') return CHOICE_DISCARD;
        if (c == 'c') return CHOICE_CANCEL;
        std::cout << "Please type s, d or c.\n";
    }
}

// Saves with whichever route fits: Save if the file has a name, otherwise
// ask for a name and use Save As.
bool FileManager::saveCurrentDocument() {
    if (filename_.empty()) {
        std::cout << "Enter a filename to save as: ";
        std::string name;
        if (!readAnswer(name)) return false;
        return saveAs(name);
    }
    return saveFile();
}

// ---------------------------------------------------- unsaved-change guard

bool FileManager::resolveUnsavedChanges() {
    if (!fileOpen_ || !modified_) return true;      // nothing to lose

    switch (askUnsavedChoice()) {
        case CHOICE_SAVE:
            // If saving fails it throws, so we never get to the caller's
            // "commit" step and nothing is lost.
            return saveCurrentDocument();
        case CHOICE_DISCARD:
            std::cout << "Changes discarded.\n";
            modified_ = false;
            return true;
        default:
            std::cout << "Cancelled.\n";
            return false;
    }
}

// ------------------------------------------------------------ public API

bool FileManager::createNewDocument(bool resolveChanges) {
    if (resolveChanges && !resolveUnsavedChanges()) return false;
    TextDocument emptyDocument;
    document_.swap(emptyDocument);
    filename_.clear();
    modified_ = false;
    fileOpen_ = true;
#ifdef _WIN32
    crlf_ = true;
#else
    crlf_ = false;
#endif
    std::cout << "New empty document created.\n";
    return true;
}

bool FileManager::openFile(const std::string& name, bool resolveChanges) {
    if (!InputValidator::isValidFilename(name)) {
        std::cout << "Error: filename is invalid.\n";
        return false;
    }

    // Step 1: read the file into a TEMPORARY string. If this throws, the
    // current document is completely untouched.
    std::string content;
    readFromDisk(name, content);
    TextDocument newDocument;
    newDocument.loadText(content);

    // Step 2: only now (we know the new file is readable) ask about unsaved
    // changes in the current document. A failed save throws from here.
    if (resolveChanges && !resolveUnsavedChanges()) return false;

    // Step 3: commit. Everything that can throw (the copy of the name) happens
    // first; the swaps cannot throw, so the commit is all-or-nothing.
    std::string newName(name);
    const bool newCrlf = content.find("\r\n") != std::string::npos;
    document_.swap(newDocument);
    crlf_ = newCrlf;
    filename_.swap(newName);
    modified_ = false;
    fileOpen_ = true;
    std::cout << "Opened '" << filename_ << "' ("
              << document_.getAllText().size() << " characters).\n";
    return true;
}

bool FileManager::saveFile() {
    if (!fileOpen_) {
        throw std::logic_error("FileManager::saveFile: no document is open");
    }
    if (filename_.empty()) {
        std::cout << "This document has no filename yet. Use Save As.\n";
        return false;
    }
    writeToDisk(filename_);                         // throws; flag stays "modified"
    modified_ = false;
    std::cout << "Saved to '" << filename_ << "'.\n";
    return true;
}

bool FileManager::saveAs(const std::string& newName, bool confirmOverwrite) {
    if (!fileOpen_) {
        throw std::logic_error("FileManager::saveAs: no document is open");
    }
    if (!InputValidator::isValidFilename(newName)) {
        std::cout << "Error: filename is invalid.\n";
        return false;
    }

    // Warn before overwriting a DIFFERENT file that already exists.
    if (confirmOverwrite && newName != filename_ && fileExists(newName)) {
        std::string answer;
        while (true) {
            std::cout << "Warning: '" << newName << "' already exists. Overwrite? (y/n): ";
            if (!readAnswer(answer)) {                       // input ended: be safe
                std::cout << "Save As cancelled.\n";
                return false;
            }
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
            if (c == 'y') break;
            if (c == 'n') {
                std::cout << "Save As cancelled.\n";
                return false;
            }
            std::cout << "Please type y or n.\n";
        }
    }

    writeToDisk(newName);                      // throws; old filename + flag unchanged
    filename_ = newName;
    modified_ = false;
    std::cout << "Saved as '" << filename_ << "'.\n";
    return true;
}

bool FileManager::closeFile() {
    if (!fileOpen_) {
        throw std::logic_error("FileManager::closeFile: no document is open");
    }
    if (!resolveUnsavedChanges()) return false;
    TextDocument emptyDocument;
    document_.swap(emptyDocument);
    filename_.clear();
    modified_ = false;
    fileOpen_ = false;
    std::cout << "Document closed.\n";
    return true;
}
