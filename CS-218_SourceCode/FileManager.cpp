#include "FileManager.h"
#include "InputValidator.h"
#include "PathUtil.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// Stage file changes so failed reads and saves leave existing data intact.
namespace {

std::string trimFilename(const std::string& text) {
    const std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

// Matches a full filename or its stem using platform-appropriate case rules.
bool filenamesEqual(const std::filesystem::path& path,
                    const std::string& requestedName) {
#ifdef _WIN32
    const std::wstring actualWide = path.filename().wstring();
    const std::wstring stemWide = path.stem().wstring();
    const std::wstring requestedWide = pathFromUtf8(requestedName).wstring();
    const auto equalIgnoringCase = [](const std::wstring& left,
                                      const std::wstring& right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::towlower(left[i]) != std::towlower(right[i])) return false;
        }
        return true;
    };
    return equalIgnoringCase(actualWide, requestedWide) ||
           equalIgnoringCase(stemWide, requestedWide);
#else
    const std::string actual = path.filename().u8string();
    const std::string stem = path.stem().u8string();
    const auto equalIgnoringCase = [](const std::string& left,
                                      const std::string& right) {
        if (left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            const unsigned char a = static_cast<unsigned char>(left[i]);
            const unsigned char b = static_cast<unsigned char>(right[i]);
            if (std::tolower(a) != std::tolower(b)) return false;
        }
        return true;
    };
    if (equalIgnoringCase(actual, requestedName) ||
        equalIgnoringCase(stem, requestedName)) {
        return true;
    }
    return false;
#endif
}

bool isBareFilename(const std::string& name) {
    const std::filesystem::path path = pathFromUtf8(name);
    return path.parent_path().empty();
}

// Recursively collects matching regular files while skipping inaccessible entries.
void collectMatches(const std::filesystem::path& root,
                    const std::string& requestedName,
                    std::vector<std::string>& matches,
                    const std::size_t limit) {
    if (matches.size() >= limit) return;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end && matches.size() < limit) {
        try {
            const std::filesystem::directory_entry& entry = *iterator;
            if (entry.is_regular_file(error) &&
                filenamesEqual(entry.path(), requestedName)) {
                matches.push_back(entry.path().lexically_normal().u8string());
            }
            if (error) error.clear();
            iterator.increment(error);
        } catch (const std::filesystem::filesystem_error&) {
            // Continue past entries that disappear or cannot be inspected.
            error.clear();
            iterator.increment(error);
        }
    }
}

std::string withReason(const std::string& what, int err) {
    if (err == 0) return what;
    return what + ": " + std::strerror(err);
}

std::filesystem::path createTempDirectory(const std::string& targetName) {
    static std::atomic<unsigned long long> nextId(0);
    const std::filesystem::path target = pathFromUtf8(targetName);
    std::filesystem::path parent = target.parent_path();
    if (parent.empty()) parent = ".";

    // Retry with fresh names to avoid colliding with another save operation.
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

class TempDirectoryGuard {
public:
    explicit TempDirectoryGuard(const std::filesystem::path& path)
        : path_(path), armed_(true) {}
    // Remove incomplete save files unless the caller has committed the save.
    ~TempDirectoryGuard() {
        if (armed_) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }
    // Preserve the directory when a failed fallback must keep a backup.
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
    modified_ = false;
}

bool FileManager::isFileOpen() const { return fileOpen_; }
bool FileManager::isModified() const { return fileOpen_ && modified_; }

// Ignore empty answers and stop cleanly when input ends.
bool FileManager::readAnswer(std::string& answer) {
    while (std::getline(std::cin, answer)) {
        if (!answer.empty()) return true;
    }
    return false;
}

bool FileManager::fileExists(const std::string& name) {
    std::error_code error;
    const std::filesystem::path path = pathFromUtf8(name);
    return std::filesystem::exists(path, error) &&
           std::filesystem::is_regular_file(path, error);
}

bool FileManager::resolveFilename(const std::string& name,
                                  std::string& resolvedName) {
    resolvedName = trimFilename(name);
    if (resolvedName.empty() || !isBareFilename(resolvedName)) {
        return true;
    }

    // Bare names search the current directory and available drives.
    std::error_code error;
    const std::filesystem::path cwd = std::filesystem::current_path(error);
    if (!error) {
        const std::filesystem::path local = cwd / pathFromUtf8(resolvedName);
        if (fileExists(local.u8string())) {
            resolvedName = local.u8string();
            return true;
        }
    }

    std::vector<std::string> matches;
    std::vector<std::filesystem::path> roots;

#ifdef _WIN32
    char driveList[256] = {};
    if (GetLogicalDriveStringsA(static_cast<DWORD>(sizeof(driveList) - 1),
                                 driveList) != 0) {
        for (char* drive = driveList; *drive; drive += std::strlen(drive) + 1) {
            roots.emplace_back(pathFromUtf8(drive));
        }
    }
#else
    roots.emplace_back("/");
#endif

    // Search each available root, limiting results per root to keep selection manageable.
    for (const std::filesystem::path& root : roots) {
        const std::size_t matchesBeforeDrive = matches.size();
        if (std::filesystem::exists(root, error) &&
            std::filesystem::is_directory(root, error)) {
            collectMatches(root, resolvedName, matches, matchesBeforeDrive + 20);
        }
        error.clear();
    }

    if (matches.empty()) return false;

    std::sort(matches.begin(), matches.end(), [](const std::string& left,
                                                  const std::string& right) {
        return left.size() < right.size() ||
               (left.size() == right.size() &&
                std::tolower(static_cast<unsigned char>(left[0])) <
                std::tolower(static_cast<unsigned char>(right[0])));
    });

    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    if (matches.size() == 1) {
        resolvedName = matches.front();
        return true;
    }

    std::cout << "Multiple files named '" << resolvedName << "' were found:\n";
    for (std::size_t i = 0; i < matches.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << matches[i] << '\n';
    }

    int selection = 0;
    if (!InputValidator::readIntInRange(
            "Enter the number of the file to open: ", 1,
            static_cast<int>(matches.size()), selection)) {
        return false;
    }
    resolvedName = matches[static_cast<std::size_t>(selection - 1)];
    return true;
}

// Read into a temporary buffer so failed reads leave content unchanged.
void FileManager::readFromDisk(const std::string& name, std::string& content) {
    errno = 0;
    std::ifstream in(pathFromUtf8(name), std::ios::binary);
    if (!in) {
        throw std::runtime_error(
            withReason("cannot open '" + name + "' for reading", errno));
    }

    // Stream reads expose device errors through badbit.
    std::string temp;
    char buffer[65536];
    do {
        in.read(buffer, sizeof buffer);
        temp.append(buffer, static_cast<std::string::size_type>(in.gcount()));
    } while (in);

    if (in.bad() || !in.eof()) {
        throw std::ios_base::failure("error while reading '" + name +
                                     "' (is it a regular file?)");
    }
    content.swap(temp);
}

// Write to a temporary file before replacing the destination.
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
        out.flush();
        if (!out) {
            throw std::ios_base::failure("failed while writing to '" + name + "'");
        }
        out.close();
        if (out.fail()) {
            throw std::ios_base::failure("failed while closing '" + name + "'");
        }
    }

    // Replace the destination only after the temporary file is complete.
    std::error_code renameError;
    std::filesystem::rename(tmp, target, renameError);
    if (renameError) {
#ifdef _WIN32
        // Back up and restore the original if Windows cannot replace it directly.
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
    // Keep prompting until the user chooses save, discard, or cancel.
    while (true) {
        std::cout << "You have unsaved changes. (s)ave, (d)iscard, (c)ancel: ";
        if (!readAnswer(answer)) return CHOICE_CANCEL;
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
        if (c == 's') return CHOICE_SAVE;
        if (c == 'd') return CHOICE_DISCARD;
        if (c == 'c') return CHOICE_CANCEL;
        std::cout << "Please type s, d or c.\n";
    }
}

bool FileManager::saveCurrentDocument() {
    // An unnamed document must be assigned a path before it can be saved.
    if (filename_.empty()) {
        std::cout << "Enter a filename to save as: ";
        std::string name;
        if (!readAnswer(name)) return false;
        return saveAs(name);
    }
    return saveFile();
}

bool FileManager::resolveUnsavedChanges() {
    if (!fileOpen_ || !modified_) return true;

    switch (askUnsavedChoice()) {
        case CHOICE_SAVE:
            // A failed save throws before the current document can be replaced.
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

bool FileManager::createNewDocument(bool resolveChanges) {
    // Replace the current state only after the user resolves pending edits.
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
    // Load into a candidate document so failures cannot replace current content.
    if (!InputValidator::isValidFilename(name)) {
        std::cout << "Error: filename is invalid.\n";
        return false;
    }

    std::string resolvedName;
    if (!resolveFilename(name, resolvedName)) {
        std::cout << "No file named '" << trimFilename(name)
                  << "' was found on the available drives.\n";
        return false;
    }
    if (!std::filesystem::is_regular_file(pathFromUtf8(resolvedName))) {
        std::cout << "Error: resolved filename is invalid.\n";
        return false;
    }

    // Load the new file before changing the current document.
    std::string content;
    readFromDisk(resolvedName, content);
    TextDocument newDocument;
    newDocument.loadText(content);

    if (resolveChanges && !resolveUnsavedChanges()) return false;

    // Swap in the new state only after all fallible work is complete.
    std::string newName(resolvedName);
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
    // Keep the modified flag set if writing the current file throws.
    if (!fileOpen_) {
        throw std::logic_error("FileManager::saveFile: no document is open");
    }
    if (filename_.empty()) {
        std::cout << "This document has no filename yet. Use Save As.\n";
        return false;
    }
    writeToDisk(filename_);
    modified_ = false;
    std::cout << "Saved to '" << filename_ << "'.\n";
    return true;
}

bool FileManager::saveAs(const std::string& newName, bool confirmOverwrite) {
    // Adopt the new path only after the content has been written successfully.
    if (!fileOpen_) {
        throw std::logic_error("FileManager::saveAs: no document is open");
    }
    if (!InputValidator::isValidFilename(newName)) {
        std::cout << "Error: filename is invalid.\n";
        return false;
    }

    // Confirm before replacing a different existing file.
    if (confirmOverwrite && newName != filename_ && fileExists(newName)) {
        std::string answer;
        while (true) {
            std::cout << "Warning: '" << newName << "' already exists. Overwrite? (y/n): ";
            if (!readAnswer(answer)) {
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

    writeToDisk(newName);
    filename_ = newName;
    modified_ = false;
    std::cout << "Saved as '" << filename_ << "'.\n";
    return true;
}

bool FileManager::closeFile() {
    // Resolve pending edits before discarding the active document.
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
