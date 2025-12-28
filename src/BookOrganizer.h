#pragma once
#include "BookMetadata.h"
#include "FileWatcher.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>

class BookOrganizer {
public:
    BookOrganizer();
    ~BookOrganizer();

    bool initialize(const std::string& watchDirectory);
    void render();
    void shutdown();

    bool isInitialized() const { return initialized; }

private:
    void onFileChanged(const std::filesystem::path& path, bool isNew);
    void refreshBookList();
    void renderFileList();
    void renderMetadataPanel();
    void selectBook(int index);
    void updateBookMetadata();
    bool renameBookFile(const BookMetadata& book, const std::string& newFilename);

    bool initialized = false;
    std::string watchDirectory;
    std::unique_ptr<FileWatcher> fileWatcher;

    std::vector<BookMetadata> books;
    mutable std::mutex booksMutex;

    int selectedBookIndex = -1;

    // UI state for editing
    char titleBuffer[256];
    char authorBuffer[256];
    char yearBuffer[16];
    bool metadataChanged = false;

    // UI dimensions
    float leftPanelWidth = 400.0f;
};
