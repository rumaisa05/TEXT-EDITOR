#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <string>
#include "TextDocument.h"

// Opens and saves files while tracking the current document's file state.
class FileManager {
public:
    // The referenced objects must outlive this manager.
    FileManager(TextDocument& document, std::string& filename, bool& modified);

    bool createNewDocument(bool resolveChanges = true);
    bool openFile(const std::string& name, bool resolveChanges = true);
    bool saveFile();
    bool saveAs(const std::string& newName, bool confirmOverwrite = true);
    bool closeFile();
    bool isFileOpen() const;
    bool isModified() const;

    // Returns false if the user cancels; save failures are reported by exception.
    bool resolveUnsavedChanges();

private:
    TextDocument& document_;
    std::string& filename_;
    bool& modified_;
    bool fileOpen_;
    bool crlf_;  // Preserve the file's line-ending style when saving.

    enum UnsavedChoice { CHOICE_SAVE, CHOICE_DISCARD, CHOICE_CANCEL };

    static bool readAnswer(std::string& answer);
    static bool fileExists(const std::string& name);
    static bool resolveFilename(const std::string& name, std::string& resolvedName);
    static void readFromDisk(const std::string& name, std::string& content);
    void writeToDisk(const std::string& name) const;
    UnsavedChoice askUnsavedChoice() const;
    bool saveCurrentDocument();
};

#endif
