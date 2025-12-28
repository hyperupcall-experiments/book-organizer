#pragma once
#include "BookMetadata.h"
#include "FileWatcher.h"
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <filesystem>
#include <unordered_map>

struct TreeNode {
    std::string name;
    std::filesystem::path fullPath;
    bool isDirectory;
    bool isExpanded;
    std::vector<std::unique_ptr<TreeNode>> children;
    TreeNode* parent;
    BookMetadata* bookData; // only for files

    TreeNode(const std::string& n, const std::filesystem::path& path, bool isDir, TreeNode* p = nullptr)
        : name(n), fullPath(path), isDirectory(isDir), isExpanded(true), parent(p), bookData(nullptr) {}
};

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
    void buildTree();
    void renderFileList();
    void renderTreeNode(TreeNode* node, int depth = 0);
    void renderMetadataPanel();
    void selectBook(BookMetadata* book);
    void updateBookMetadata();
    bool renameBookFile(const BookMetadata& book, const std::string& newFilename);
    TreeNode* findOrCreateNode(const std::filesystem::path& path);
    void sortTreeNode(TreeNode* node);

    bool initialized = false;
    std::string watchDirectory;
    std::unique_ptr<FileWatcher> fileWatcher;

    std::vector<BookMetadata> books;
    mutable std::mutex booksMutex;

    // Tree structure
    std::unique_ptr<TreeNode> rootNode;
    std::unordered_map<std::string, TreeNode*> pathToNode;
    BookMetadata* selectedBook = nullptr;

    // UI state for editing
    char titleBuffer[256];
    char authorBuffer[256];
    char yearBuffer[16];
    bool metadataChanged = false;

    // UI dimensions
    float leftPanelWidth = 400.0f;
};
