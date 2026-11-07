// FileManager.h  -  Member 1: File Operations (revised)
//
// FileManager works on three pieces of SHARED state that live in main.cpp:
//     TextDocument document - the text
//     std::string filename   - current file name ("" for a new, unnamed doc)
//     bool        modified   - true when the text has unsaved changes
// FileManager stores only REFERENCES to them (it never keeps its own copy),
// so every team member's module sees exactly the same data.
//
// ERROR HANDLING
//   Return value:  true  = the operation was done
//                  false = nothing was done because of a normal outcome
//                          (user cancelled, unnamed document needs Save As,
//                          input ended). Never used for a real failure.
//   Exceptions (all derive from std::exception; the caller catches them):
//     std::runtime_error      a file cannot be opened/created/replaced
//     std::ios_base::failure  reading, writing, flushing or closing failed
//     std::logic_error        saveFile/saveAs/closeFile called while no
//                             document is open (check isFileOpen() first)
//   When an exception is thrown the shared document, filename and modified
//   flag are unchanged, and an existing destination file is left intact.
#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include "TextDocument.h"

class FileManager {
public:
    // All three objects must be created in main.cpp BEFORE this object
    // and must outlive it. FileManager does not own them.
    FileManager(TextDocument& document, std::string& filename, bool& modified);

    bool createNewDocument(bool resolveChanges = true); // new empty, unnamed document
    // Load into temporary state first, then replace the current document on success.
    bool openFile(const std::string& name, bool resolveChanges = true);
    bool saveFile();                             // save to current filename; throws if it fails
    bool saveAs(const std::string& newName, bool confirmOverwrite = true);
    bool closeFile();                            // close the current document after resolving pending edits
    bool isFileOpen() const;                     // is a document open?

    bool isModified() const;                     // unsaved changes? (read only)

    // Safe-exit workflow. If there are unsaved changes it asks the user:
    // (s)ave, (d)iscard or (c)ancel. Returns true when it is SAFE to
    // continue (nothing to lose, saved, or user chose discard) and false
    // when the user cancelled. If the chosen save fails it THROWS (see above)
    // instead of returning false. exitEditor() should call this.
    bool resolveUnsavedChanges();

private:
    TextDocument& document_;  // shared text
    std::string& filename_;   // shared filename
    bool&        modified_;   // shared "unsaved changes" flag
    bool         fileOpen_;   // true while a document is open
    bool         crlf_;       // file uses Windows (CRLF) line endings; kept on save

    enum UnsavedChoice { CHOICE_SAVE, CHOICE_DISCARD, CHOICE_CANCEL };

    static bool readAnswer(std::string& answer);   // next non-empty input line
    static bool fileExists(const std::string& name);
    static void readFromDisk(const std::string& name, std::string& content);   // throws (binary read)
    void writeToDisk(const std::string& name) const;                           // throws
    UnsavedChoice askUnsavedChoice() const;
    bool saveCurrentDocument();
};

#endif
